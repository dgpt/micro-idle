#include "game/gpu_sim.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "raymath.h"
#include "external/glad.h"
#include "rlgl.h"

#define GPU_WORKGROUP_SIZE 256
#define GRID_W 128
#define GRID_H 128
#define GRID_CELLS (GRID_W * GRID_H)

typedef struct GpuEntity {
    float pos[4];
    float vel[4];
    float color[4];
    float params[4];
} GpuEntity;

#ifdef GPU_SIM_TESTING
static int gpu_sim_fail_mode = 0;

void gpu_sim_test_set_fail_mode(int mode) {
    gpu_sim_fail_mode = mode;
}
#endif

static const char *kShaderSimInsertName = "sim_insert.comp";
static const char *kShaderSimCollideName = "sim_collide.comp";
static const char *kShaderMicrobeVertName = "microbe.vert";
static const char *kShaderMicrobeFragName = "microbe.frag";

static char *load_shader_source(const char *file_name, char *resolved, size_t resolved_size) {
    const char *roots[] = {"data/shaders", "../data/shaders", "../../data/shaders"};
    char path[512];
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        int written = snprintf(path, sizeof(path), "%s/%s", roots[i], file_name);
        if (written <= 0 || (size_t)written >= sizeof(path)) continue;
        char *text = LoadFileText(path);
        if (text) {
            if (resolved && resolved_size > 0) {
                snprintf(resolved, resolved_size, "%s", path);
            }
            return text;
        }
    }
    fprintf(stderr, "failed to load shader file: %s\n", file_name);
    return NULL;
}

static bool resolve_shader_path(const char *file_name, char *resolved, size_t resolved_size) {
    const char *roots[] = {"data/shaders", "../data/shaders", "../../data/shaders"};
    char path[512];
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        int written = snprintf(path, sizeof(path), "%s/%s", roots[i], file_name);
        if (written <= 0 || (size_t)written >= sizeof(path)) continue;
        if (FileExists(path)) {
            if (resolved && resolved_size > 0) {
                snprintf(resolved, resolved_size, "%s", path);
            }
            return true;
        }
    }
    fprintf(stderr, "shader path not found: %s\n", file_name);
    return false;
}

static unsigned int compile_shader_from_file(GLenum type, const char *file_name) {
    char resolved[512] = {0};
    bool from_file = true;
    char *source = load_shader_source(file_name, resolved, sizeof(resolved));
#ifdef GPU_SIM_TESTING
    if (gpu_sim_fail_mode == 1) {
        if (source) UnloadFileText(source);
        source = strdup("invalid shader");
        from_file = false;
    }
#endif
    if (!source) {
        return 0;
    }
    unsigned int shader = glCreateShader(type);
    if (!shader) {
        if (from_file) UnloadFileText(source); else free(source);
        return 0;
    }
    const char *src_ptr = source;
    glShaderSource(shader, 1, &src_ptr, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 1) {
            char *log = (char *)malloc((size_t)length);
            if (log) {
                glGetShaderInfoLog(shader, length, NULL, log);
                fprintf(stderr, "shader compile failed (%s): %s\n", resolved[0] ? resolved : file_name, log);
                free(log);
            }
        }
        glDeleteShader(shader);
        if (from_file) UnloadFileText(source); else free(source);
        return 0;
    }
    if (from_file) UnloadFileText(source); else free(source);
    return shader;
}


static unsigned int link_program(unsigned int shader) {
    unsigned int program = glCreateProgram();
    if (!program) {
        return 0;
    }
    if (shader) {
        glAttachShader(program, shader);
    }
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
#ifdef GPU_SIM_TESTING
    if (gpu_sim_fail_mode == 2) {
        linked = 0;
    }
#endif
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 1) {
            char *log = (char *)malloc((size_t)length);
            if (log) {
                glGetProgramInfoLog(program, length, NULL, log);
                fprintf(stderr, "program link failed: %s\n", log);
                free(log);
            }
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static void init_quad(GpuSim *sim) {
    float verts[] = {
        -0.5f, 0.0f, -0.5f,
         0.5f, 0.0f, -0.5f,
         0.5f, 0.0f,  0.5f,
        -0.5f, 0.0f,  0.5f
    };
    unsigned short indices[] = {0, 1, 2, 2, 3, 0};

    glGenVertexArrays(1, &sim->vao);
    glBindVertexArray(sim->vao);

    glGenBuffers(1, &sim->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, sim->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glGenBuffers(1, &sim->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sim->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindVertexArray(0);

    sim->indices_count = 6;
}

static bool init_entities(GpuSim *sim, int count) {
    GpuEntity *entities = NULL;
#ifdef GPU_SIM_TESTING
    if (gpu_sim_fail_mode != 3) {
        entities = (GpuEntity *)malloc(sizeof(GpuEntity) * (size_t)count);
    }
#else
    entities = (GpuEntity *)malloc(sizeof(GpuEntity) * (size_t)count);
#endif
    if (!entities) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        uint32_t seed = (uint32_t)(i + 1) * 2654435761u;
        seed ^= seed >> 16;
        float r1 = (float)(seed & 0xFFFFu) / 65535.0f;
        seed = seed * 2246822519u + 3266489917u;
        float r2 = (float)(seed & 0xFFFFu) / 65535.0f;
        seed = seed * 3266489917u + 668265263u;
        float r3 = (float)(seed & 0xFFFFu) / 65535.0f;
        int type = (int)(seed % 4u);
        float px = ((float)(i % 1000) / 1000.0f - 0.5f) * 24.0f;
        float pz = ((float)(i / 1000) / 1000.0f - 0.5f) * 20.0f;
        entities[i].pos[0] = px;
        entities[i].pos[1] = 0.0f;
        entities[i].pos[2] = pz;
        entities[i].pos[3] = 1.0f;
        entities[i].vel[0] = (r1 - 0.5f) * 0.3f;
        entities[i].vel[1] = 0.0f;
        entities[i].vel[2] = (r2 - 0.5f) * 0.3f;
        entities[i].vel[3] = 0.0f;
        switch (type) {
            case 0:
                entities[i].color[0] = 0.26f;
                entities[i].color[1] = 0.76f;
                entities[i].color[2] = 0.36f;
                break;
            case 1:
                entities[i].color[0] = 0.18f;
                entities[i].color[1] = 0.56f;
                entities[i].color[2] = 0.78f;
                break;
            case 2:
                entities[i].color[0] = 0.96f;
                entities[i].color[1] = 0.72f;
                entities[i].color[2] = 0.18f;
                break;
            default:
                entities[i].color[0] = 0.78f;
                entities[i].color[1] = 0.28f;
                entities[i].color[2] = 0.76f;
                break;
        }
        entities[i].color[0] += (r3 - 0.5f) * 0.12f;
        entities[i].color[1] += (r1 - 0.5f) * 0.12f;
        entities[i].color[2] += (r2 - 0.5f) * 0.12f;
        entities[i].color[3] = 0.55f;
        entities[i].params[0] = 0.9f + r3 * 1.25f;
        entities[i].params[1] = 0.0f;
        entities[i].params[2] = (float)type;
        entities[i].params[3] = r2;
    }

    glGenBuffers(1, &sim->ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GpuEntity) * (size_t)count, entities, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sim->ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &sim->ssbo_head);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssbo_head);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(int) * GRID_CELLS, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sim->ssbo_head);

    glGenBuffers(1, &sim->ssbo_next);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssbo_next);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(int) * (size_t)count, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim->ssbo_next);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    free(entities);
    return true;
}

static bool validate_gpu_sim_limits(int count) {
    GLint max_storage = 0;
    GLint max_ssbo = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_storage);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_ssbo);
    if (max_storage <= 0 || max_ssbo < 3) {
        return false;
    }
    size_t bytes = sizeof(GpuEntity) * (size_t)count;
    return bytes <= (size_t)max_storage;
}

bool gpu_sim_init(GpuSim *sim, int entity_count) {
    memset(sim, 0, sizeof(*sim));
    sim->entity_count = entity_count;
    sim->active_count = entity_count;

    if (!gpu_sim_supported()) {
        return false;
    }
    if (!validate_gpu_sim_limits(entity_count)) {
        return false;
    }

    unsigned int cs_insert = compile_shader_from_file(GL_COMPUTE_SHADER, kShaderSimInsertName);
    if (cs_insert == 0) return false;
    sim->sim_insert_program = link_program(cs_insert);
    glDeleteShader(cs_insert);

    unsigned int cs_collide = compile_shader_from_file(GL_COMPUTE_SHADER, kShaderSimCollideName);
    if (cs_collide == 0) return false;
    sim->sim_collide_program = link_program(cs_collide);
    glDeleteShader(cs_collide);

    char vert_path[512] = {0};
    char frag_path[512] = {0};
    if (!resolve_shader_path(kShaderMicrobeVertName, vert_path, sizeof(vert_path))) return false;
    if (!resolve_shader_path(kShaderMicrobeFragName, frag_path, sizeof(frag_path))) return false;
    sim->render_shader = LoadShader(vert_path, frag_path);
    if (sim->sim_insert_program == 0 || sim->sim_collide_program == 0 || sim->render_shader.id == 0) return false;

    sim->loc_vp = GetShaderLocation(sim->render_shader, "u_vp");
#ifdef GPU_SIM_TESTING
    if (gpu_sim_fail_mode == 4) {
        sim->loc_vp = -1;
    }
#endif
    if (sim->loc_vp < 0) {
        sim->loc_vp = 0;
    }
    sim->loc_bounds = rlGetLocationUniform(sim->sim_collide_program, "u_bounds");
    sim->loc_dt = rlGetLocationUniform(sim->sim_collide_program, "u_dt");
    sim->loc_grid_dim = rlGetLocationUniform(sim->sim_collide_program, "u_grid_dim");
    sim->loc_cell = rlGetLocationUniform(sim->sim_collide_program, "u_cell");
    sim->loc_active_insert = glGetUniformLocation(sim->sim_insert_program, "u_active");
    sim->loc_active_collide = glGetUniformLocation(sim->sim_collide_program, "u_active");
    sim->loc_time = GetShaderLocation(sim->render_shader, "u_time");

    init_quad(sim);
    if (!init_entities(sim, entity_count)) {
        return false;
    }
    sim->ready = true;
    return true;
}

void gpu_sim_shutdown(GpuSim *sim) {
    if (!sim->ready) return;
    if (sim->ssbo) glDeleteBuffers(1, &sim->ssbo);
    if (sim->ssbo_head) glDeleteBuffers(1, &sim->ssbo_head);
    if (sim->ssbo_next) glDeleteBuffers(1, &sim->ssbo_next);
    if (sim->vbo) glDeleteBuffers(1, &sim->vbo);
    if (sim->ebo) glDeleteBuffers(1, &sim->ebo);
    if (sim->vao) glDeleteVertexArrays(1, &sim->vao);
    if (sim->render_shader.id) UnloadShader(sim->render_shader);
    if (sim->sim_insert_program) glDeleteProgram(sim->sim_insert_program);
    if (sim->sim_collide_program) glDeleteProgram(sim->sim_collide_program);
    memset(sim, 0, sizeof(*sim));
}

void gpu_sim_update(GpuSim *sim, float dt, Vector2 bounds) {
    if (!sim->ready) return;
    rlDrawRenderBatchActive();

    float cell = (bounds.x * 2.0f) / (float)GRID_W;
    int grid_dim[2] = {GRID_W, GRID_H};
    int clear = -1;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sim->ssbo_head);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32I, GL_RED_INTEGER, GL_INT, &clear);

    glUseProgram(sim->sim_insert_program);
    int loc_bounds = rlGetLocationUniform(sim->sim_insert_program, "u_bounds");
    int loc_cell = rlGetLocationUniform(sim->sim_insert_program, "u_cell");
    int loc_grid = rlGetLocationUniform(sim->sim_insert_program, "u_grid_dim");
    glUniform2f(loc_bounds, bounds.x, bounds.y);
    glUniform1f(loc_cell, cell);
    glUniform2i(loc_grid, grid_dim[0], grid_dim[1]);
    if (sim->loc_active_insert >= 0) {
        glUniform1i(sim->loc_active_insert, sim->active_count);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sim->ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sim->ssbo_head);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim->ssbo_next);

    unsigned int groups = (sim->active_count + GPU_WORKGROUP_SIZE - 1) / GPU_WORKGROUP_SIZE;
    if (groups > 0) {
        glDispatchCompute(groups, 1, 1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glUseProgram(sim->sim_collide_program);
    glUniform1f(sim->loc_dt, dt);
    glUniform2f(sim->loc_bounds, bounds.x, bounds.y);
    glUniform1f(sim->loc_cell, cell);
    glUniform2i(sim->loc_grid_dim, grid_dim[0], grid_dim[1]);
    if (sim->loc_active_collide >= 0) {
        glUniform1i(sim->loc_active_collide, sim->active_count);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sim->ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sim->ssbo_head);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sim->ssbo_next);
    if (groups > 0) {
        glDispatchCompute(groups, 1, 1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void gpu_sim_render(const GpuSim *sim, Camera3D camera) {
    if (!sim->ready) return;
    unsigned int active_fbo = rlGetActiveFramebuffer();
    rlDrawRenderBatchActive();
    if (active_fbo != rlGetActiveFramebuffer()) {
        rlEnableFramebuffer(active_fbo);
    }
    glViewport(0, 0, GetRenderWidth(), GetRenderHeight());

    Matrix view = GetCameraMatrix(camera);
    Matrix proj = MatrixPerspective(DEG2RAD * camera.fovy,
                                    (float)GetRenderWidth() / (float)GetRenderHeight(),
                                    0.1f, 200.0f);
    Matrix vp = MatrixMultiply(view, proj);

    glUseProgram(sim->render_shader.id);
    glUniformMatrix4fv(sim->loc_vp, 1, GL_FALSE, MatrixToFloatV(vp).v);
    if (sim->loc_time >= 0) {
        glUniform1f(sim->loc_time, (float)GetTime());
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(sim->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sim->ebo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sim->ssbo);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDrawElementsInstanced(GL_TRIANGLES,
                            sim->indices_count,
                            GL_UNSIGNED_SHORT,
                            (void *)0,
                            sim->active_count);
    glBindVertexArray(0);
    glUseProgram(0);
}

bool gpu_sim_supported(void) {
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    return (major > 4) || (major == 4 && minor >= 6);
}

void gpu_sim_set_active_count(GpuSim *sim, int active_count) {
    if (!sim) {
        return;
    }
    if (active_count < 0) {
        active_count = 0;
    }
    if (active_count > sim->entity_count) {
        active_count = sim->entity_count;
    }
    sim->active_count = active_count;
}
