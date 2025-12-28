#include "game/xpbd.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "external/glad.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define GRID_W 64
#define GRID_H 64
#define GRID_CELLS (GRID_W * GRID_H)

static const char *kShaderRoots[] = {"data/shaders", "../data/shaders", "../../data/shaders"};

static bool xpbd_supported(void) {
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (!((major > 4) || (major == 4 && minor >= 3))) {
        return false;
    }
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    bool soft = renderer && (strstr(renderer, "llvmpipe") || strstr(renderer, "Software"));
    if (soft) {
        fprintf(stderr, "xpbd: software renderer detected (%s); GPU mode required.\n", renderer ? renderer : "unknown");
        return false;
    }
    return true;
}

static bool xpbd_validate_limits(int max_microbes) {
    GLint max_storage = 0;
    GLint max_bindings = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_storage);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_bindings);
    if (max_storage <= 0 || max_bindings < 3) {
        return false;
    }

    size_t particle_bytes = (size_t)max_microbes * XPBD_PARTICLES_PER_MICROBE * sizeof(XpbdParticle);
    size_t constraint_bytes = (size_t)max_microbes * XPBD_CONSTRAINTS_PER_MICROBE * sizeof(XpbdConstraint);
    size_t microbe_bytes = (size_t)max_microbes * sizeof(XpbdMicrobe);

    return particle_bytes <= (size_t)max_storage &&
           constraint_bytes <= (size_t)max_storage &&
           microbe_bytes <= (size_t)max_storage;
}

static char *load_shader_source(const char *file_name, char *resolved, size_t resolved_size) {
    char path[512];
    for (size_t i = 0; i < sizeof(kShaderRoots) / sizeof(kShaderRoots[0]); i++) {
        int written = snprintf(path, sizeof(path), "%s/%s", kShaderRoots[i], file_name);
        if (written <= 0 || (size_t)written >= sizeof(path)) continue;
        char *text = LoadFileText(path);
        if (text) {
            if (resolved && resolved_size > 0) {
                snprintf(resolved, resolved_size, "%s", path);
            }
            return text;
        }
    }
    fprintf(stderr, "xpbd: failed to load shader source %s\n", file_name);
    return NULL;
}

static unsigned int compile_shader(GLenum type, const char *file_name) {
    char resolved[512] = {0};
    char *source = load_shader_source(file_name, resolved, sizeof(resolved));
    if (!source) {
        return 0;
    }

    unsigned int shader = glCreateShader(type);
    if (!shader) {
        UnloadFileText(source);
        return 0;
    }

    const char *src_ptr = source;
    glShaderSource(shader, 1, &src_ptr, NULL);
    glCompileShader(shader);
    UnloadFileText(source);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 1) {
            char *log = (char *)malloc((size_t)length);
            if (log) {
                glGetShaderInfoLog(shader, length, NULL, log);
                fprintf(stderr, "xpbd: shader compile failed (%s): %s\n", resolved[0] ? resolved : file_name, log);
                free(log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static unsigned int link_program_single(unsigned int shader) {
    unsigned int program = glCreateProgram();
    if (!program) {
        return 0;
    }
    glAttachShader(program, shader);
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 1) {
            char *log = (char *)malloc((size_t)length);
            if (log) {
                glGetProgramInfoLog(program, length, NULL, log);
                fprintf(stderr, "xpbd: program link failed: %s\n", log);
                free(log);
            }
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static unsigned int link_program_pair(unsigned int vs, unsigned int fs) {
    unsigned int program = glCreateProgram();
    if (!program) {
        return 0;
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 1) {
            char *log = (char *)malloc((size_t)length);
            if (log) {
                glGetProgramInfoLog(program, length, NULL, log);
                fprintf(stderr, "xpbd: render program link failed: %s\n", log);
                free(log);
            }
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static unsigned int compile_compute_program(const char *file_name) {
    unsigned int shader = compile_shader(GL_COMPUTE_SHADER, file_name);
    if (!shader) {
        return 0;
    }
    unsigned int program = link_program_single(shader);
    glDeleteShader(shader);
    return program;
}

struct XpbdContext {
    // Counts
    int max_microbes;
    int microbe_count;
    int particle_count;
    int constraint_count;

    // OpenGL buffers
    unsigned int particle_ssbo;
    unsigned int constraint_ssbo;
    unsigned int microbe_ssbo;
    unsigned int grid_head_ssbo;
    unsigned int grid_next_ssbo;

    // Compute shaders
    unsigned int predict_program;
    unsigned int grid_insert_program;
    unsigned int collide_program;
    unsigned int solve_program;
    unsigned int pressure_program;
    unsigned int finalize_program;
    unsigned int microbe_update_program;
    unsigned int bounds_program;

    // Metaball rendering (two-pass)
    unsigned int field_fbo;           // Framebuffer for field accumulation
    unsigned int field_texture;       // Field texture (RGBA16F)
    unsigned int field_shader;        // Pass 1: render particle billboards
    unsigned int surface_shader;      // Pass 2: threshold and display surface

    // Billboard geometry (simple quad for particle rendering)
    unsigned int billboard_vao;
    unsigned int billboard_vbo;

    // Fullscreen quad (for surface pass)
    unsigned int quad_vao;
    unsigned int quad_vbo;

    // Metaball shader uniform locations
    int loc_field_vp;

    // Outline curve rendering
    unsigned int outline_program;
    unsigned int outline_points_ssbo;  // Sorted membrane positions per frame
    unsigned int outline_vao;
    unsigned int outline_vbo;
    int loc_outline_vp;
    int loc_outline_point_count;
    int loc_outline_microbe_index;
    int loc_outline_offset_radius;
    int loc_field_ppm;
    int loc_surface_field_tex;
    int loc_surface_time;
    int loc_surface_threshold;

    // Uniform locations
    int loc_predict_dt;
    int loc_predict_count;
    int loc_predict_ppm;
    int loc_predict_time;
    int loc_predict_cursor;
    int loc_predict_bounds;

    int loc_grid_bounds;
    int loc_grid_cell;
    int loc_grid_dim;
    int loc_grid_count;

    int loc_collide_dt;
    int loc_collide_bounds;
    int loc_collide_cell;
    int loc_collide_dim;
    int loc_collide_count;
    int loc_collide_radius;

    int loc_solve_dt;
    int loc_solve_mcount;
    int loc_solve_cpm;

    int loc_pressure_dt;
    int loc_pressure_count;
    int loc_pressure_ppm;

    int loc_finalize_dt;
    int loc_finalize_pcount;

    int loc_microbe_count;
    int loc_microbe_ppm;
    int loc_microbe_cpm;

    int loc_bounds_count;
    int loc_bounds_ppm;
    int loc_bounds_bounds;

    // CPU staging buffers for initialization
    XpbdParticle *particles_cpu;
    XpbdConstraint *constraints_cpu;
    XpbdMicrobe *microbes_cpu;

    bool ready;
};

static bool init_metaball_pipeline(XpbdContext *ctx) {
    // Compile metaball shaders
    unsigned int field_vs = compile_shader(GL_VERTEX_SHADER, "particle_simple.vert");
    unsigned int field_fs = compile_shader(GL_FRAGMENT_SHADER, "particle_simple.frag");
    unsigned int surface_vs = 0;
    unsigned int surface_fs = 0;

    if (!field_vs || !field_fs) {
        if (field_vs) glDeleteShader(field_vs);
        if (field_fs) glDeleteShader(field_fs);
        if (surface_vs) glDeleteShader(surface_vs);
        if (surface_fs) glDeleteShader(surface_fs);
        return false;
    }

    ctx->field_shader = link_program_pair(field_vs, field_fs);
    ctx->surface_shader = 0;  // No longer using two-pass metaball rendering

    glDeleteShader(field_vs);
    glDeleteShader(field_fs);
    glDeleteShader(surface_vs);
    glDeleteShader(surface_fs);

    if (!ctx->field_shader) {
        return false;
    }

    // Get uniform locations
    ctx->loc_field_vp = glGetUniformLocation(ctx->field_shader, "u_vp");
    ctx->loc_field_ppm = glGetUniformLocation(ctx->field_shader, "u_particles_per_microbe");

    ctx->loc_surface_field_tex = glGetUniformLocation(ctx->surface_shader, "u_field_texture");
    ctx->loc_surface_time = glGetUniformLocation(ctx->surface_shader, "u_time");
    ctx->loc_surface_threshold = glGetUniformLocation(ctx->surface_shader, "u_threshold");

    // Create field accumulation framebuffer (screen-sized, will resize on first render)
    glGenFramebuffers(1, &ctx->field_fbo);
    glGenTextures(1, &ctx->field_texture);
    glBindTexture(GL_TEXTURE_2D, ctx->field_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, ctx->field_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->field_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "xpbd: metaball framebuffer incomplete\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create billboard geometry (simple quad for particle instances)
    // Each particle will be rendered as a billboard quad
    float billboard_verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glGenVertexArrays(1, &ctx->billboard_vao);
    glBindVertexArray(ctx->billboard_vao);

    glGenBuffers(1, &ctx->billboard_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->billboard_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(billboard_verts), billboard_verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

    glBindVertexArray(0);

    // Create fullscreen quad for surface rendering pass
    float quad_verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glGenVertexArrays(1, &ctx->quad_vao);
    glBindVertexArray(ctx->quad_vao);

    glGenBuffers(1, &ctx->quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

    glBindVertexArray(0);

    return true;
}

static bool init_outline_pipeline(XpbdContext *ctx) {
    // Compile outline curve shaders
    unsigned int vs = compile_shader(GL_VERTEX_SHADER, "outline_curve.vert");
    unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, "outline_curve.frag");

    if (!vs || !fs) {
        fprintf(stderr, "xpbd: failed to compile outline shaders\n");
        return false;
    }

    ctx->outline_program = glCreateProgram();
    glAttachShader(ctx->outline_program, vs);
    glAttachShader(ctx->outline_program, fs);
    glLinkProgram(ctx->outline_program);

    GLint success;
    glGetProgramiv(ctx->outline_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(ctx->outline_program, sizeof(log), NULL, log);
        fprintf(stderr, "xpbd: outline shader link error: %s\n", log);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Get uniform locations
    ctx->loc_outline_vp = glGetUniformLocation(ctx->outline_program, "u_vp");
    ctx->loc_outline_point_count = glGetUniformLocation(ctx->outline_program, "u_point_count");
    ctx->loc_outline_microbe_index = glGetUniformLocation(ctx->outline_program, "u_microbe_index");
    ctx->loc_outline_offset_radius = glGetUniformLocation(ctx->outline_program, "u_offset_radius");

    // Create SSBO for sorted outline points (max 16 per microbe)
    glGenBuffers(1, &ctx->outline_points_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->outline_points_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * sizeof(float) * 4, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create VAO for curve rendering
    // We'll generate a continuous line strip that wraps around all points
    int max_points = 16;  // Max membrane particles per microbe
    int subdivisions = 16;  // Subdivisions per segment for smoothness
    int total_verts = max_points * subdivisions;

    // Struct for vertex data: (float t, int point_index)
    typedef struct { float t; int point_index; } CurveVertex;
    CurveVertex *vert_data = (CurveVertex*)malloc(total_verts * sizeof(CurveVertex));
    int idx = 0;

    for (int seg = 0; seg < max_points; seg++) {
        for (int sub = 0; sub < subdivisions; sub++) {
            vert_data[idx].t = (float)sub / subdivisions;
            vert_data[idx].point_index = seg;
            idx++;
        }
    }

    glGenVertexArrays(1, &ctx->outline_vao);
    glGenBuffers(1, &ctx->outline_vbo);

    glBindVertexArray(ctx->outline_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->outline_vbo);
    glBufferData(GL_ARRAY_BUFFER, total_verts * sizeof(CurveVertex), vert_data, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(CurveVertex), (void*)0);  // t

    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_INT, sizeof(CurveVertex), (void*)offsetof(CurveVertex, point_index));  // point_index

    glBindVertexArray(0);
    free(vert_data);

    return true;
}

XpbdContext *xpbd_create(int max_microbes) {
    if (max_microbes <= 0) {
        return NULL;
    }
    if (!xpbd_supported()) {
        fprintf(stderr, "xpbd: OpenGL 4.3+ required for compute path\n");
        return NULL;
    }
    if (!xpbd_validate_limits(max_microbes)) {
        fprintf(stderr, "xpbd: requested capacity exceeds SSBO limits\n");
        return NULL;
    }

    XpbdContext *ctx = (XpbdContext*)calloc(1, sizeof(XpbdContext));
    if (!ctx) return NULL;

    ctx->max_microbes = max_microbes;
    ctx->microbe_count = 0;

    int max_particles = max_microbes * XPBD_PARTICLES_PER_MICROBE;
    int max_constraints = max_microbes * XPBD_CONSTRAINTS_PER_MICROBE;

    // Allocate CPU staging buffers
    ctx->particles_cpu = (XpbdParticle*)calloc(max_particles, sizeof(XpbdParticle));
    ctx->constraints_cpu = (XpbdConstraint*)calloc(max_constraints, sizeof(XpbdConstraint));
    ctx->microbes_cpu = (XpbdMicrobe*)calloc(max_microbes, sizeof(XpbdMicrobe));
    if (!ctx->particles_cpu || !ctx->constraints_cpu || !ctx->microbes_cpu) {
        fprintf(stderr, "xpbd: failed to allocate staging buffers\n");
        goto fail;
    }

    // Create SSBOs
    glGenBuffers(1, &ctx->particle_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->particle_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, max_particles * sizeof(XpbdParticle), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &ctx->constraint_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->constraint_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, max_constraints * sizeof(XpbdConstraint), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &ctx->microbe_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->microbe_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, max_microbes * sizeof(XpbdMicrobe), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &ctx->grid_head_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->grid_head_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, GRID_CELLS * sizeof(int), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &ctx->grid_next_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->grid_next_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, max_particles * sizeof(int), NULL, GL_DYNAMIC_DRAW);

    // Compile compute shaders
    ctx->predict_program = compile_compute_program("xpbd_predict.comp");
    ctx->grid_insert_program = compile_compute_program("xpbd_grid_insert.comp");
    ctx->collide_program = compile_compute_program("xpbd_collide.comp");
    ctx->solve_program = compile_compute_program("xpbd_solve.comp");
    ctx->pressure_program = compile_compute_program("xpbd_pressure.comp");
    ctx->finalize_program = compile_compute_program("xpbd_finalize.comp");
    ctx->microbe_update_program = compile_compute_program("xpbd_microbe_update.comp");
    ctx->bounds_program = compile_compute_program("xpbd_bounds.comp");

    if (!ctx->predict_program || !ctx->grid_insert_program || !ctx->collide_program ||
        !ctx->solve_program || !ctx->pressure_program || !ctx->finalize_program ||
        !ctx->microbe_update_program || !ctx->bounds_program) {
        fprintf(stderr, "xpbd: failed to compile compute programs\n");
        goto fail;
    }

    if (!init_metaball_pipeline(ctx)) {
        fprintf(stderr, "xpbd: failed to init metaball rendering pipeline\n");
        goto fail;
    }

    if (!init_outline_pipeline(ctx)) {
        fprintf(stderr, "xpbd: failed to init outline rendering pipeline\n");
        goto fail;
    }

    // Get uniform locations
    ctx->loc_predict_dt = glGetUniformLocation(ctx->predict_program, "u_dt");
    ctx->loc_predict_count = glGetUniformLocation(ctx->predict_program, "u_particle_count");
    ctx->loc_predict_ppm = glGetUniformLocation(ctx->predict_program, "u_particles_per_microbe");
    ctx->loc_predict_time = glGetUniformLocation(ctx->predict_program, "u_time");
    ctx->loc_predict_cursor = glGetUniformLocation(ctx->predict_program, "u_cursor");
    ctx->loc_predict_bounds = glGetUniformLocation(ctx->predict_program, "u_bounds");

    ctx->loc_grid_bounds = glGetUniformLocation(ctx->grid_insert_program, "u_bounds");
    ctx->loc_grid_cell = glGetUniformLocation(ctx->grid_insert_program, "u_cell");
    ctx->loc_grid_dim = glGetUniformLocation(ctx->grid_insert_program, "u_grid_dim");
    ctx->loc_grid_count = glGetUniformLocation(ctx->grid_insert_program, "u_particle_count");

    ctx->loc_collide_dt = glGetUniformLocation(ctx->collide_program, "u_dt");
    ctx->loc_collide_bounds = glGetUniformLocation(ctx->collide_program, "u_bounds");
    ctx->loc_collide_cell = glGetUniformLocation(ctx->collide_program, "u_cell");
    ctx->loc_collide_dim = glGetUniformLocation(ctx->collide_program, "u_grid_dim");
    ctx->loc_collide_count = glGetUniformLocation(ctx->collide_program, "u_particle_count");
    ctx->loc_collide_radius = glGetUniformLocation(ctx->collide_program, "u_collision_radius");

    ctx->loc_solve_dt = glGetUniformLocation(ctx->solve_program, "u_dt");
    ctx->loc_solve_mcount = glGetUniformLocation(ctx->solve_program, "u_microbe_count");
    ctx->loc_solve_cpm = glGetUniformLocation(ctx->solve_program, "u_constraints_per_microbe");

    ctx->loc_pressure_dt = glGetUniformLocation(ctx->pressure_program, "u_dt");
    ctx->loc_pressure_count = glGetUniformLocation(ctx->pressure_program, "u_microbe_count");
    ctx->loc_pressure_ppm = glGetUniformLocation(ctx->pressure_program, "u_particles_per_microbe");

    ctx->loc_finalize_dt = glGetUniformLocation(ctx->finalize_program, "u_dt");
    ctx->loc_finalize_pcount = glGetUniformLocation(ctx->finalize_program, "u_particle_count");

    ctx->loc_microbe_count = glGetUniformLocation(ctx->microbe_update_program, "u_microbe_count");
    ctx->loc_microbe_ppm = glGetUniformLocation(ctx->microbe_update_program, "u_particles_per_microbe");
    ctx->loc_microbe_cpm = glGetUniformLocation(ctx->microbe_update_program, "u_constraints_per_microbe");

    ctx->loc_bounds_count = glGetUniformLocation(ctx->bounds_program, "u_microbe_count");
    ctx->loc_bounds_ppm = glGetUniformLocation(ctx->bounds_program, "u_particles_per_microbe");
    ctx->loc_bounds_bounds = glGetUniformLocation(ctx->bounds_program, "u_bounds");

    ctx->ready = true;
    fprintf(stderr, "xpbd: initialized with capacity for %d microbes\n", max_microbes);
    return ctx;

fail:
    xpbd_destroy(ctx);
    return NULL;
}

void xpbd_destroy(XpbdContext *ctx) {
    if (!ctx) return;

    if (ctx->particle_ssbo) glDeleteBuffers(1, &ctx->particle_ssbo);
    if (ctx->constraint_ssbo) glDeleteBuffers(1, &ctx->constraint_ssbo);
    if (ctx->microbe_ssbo) glDeleteBuffers(1, &ctx->microbe_ssbo);
    if (ctx->grid_head_ssbo) glDeleteBuffers(1, &ctx->grid_head_ssbo);
    if (ctx->grid_next_ssbo) glDeleteBuffers(1, &ctx->grid_next_ssbo);

    if (ctx->predict_program) glDeleteProgram(ctx->predict_program);
    if (ctx->grid_insert_program) glDeleteProgram(ctx->grid_insert_program);
    if (ctx->collide_program) glDeleteProgram(ctx->collide_program);
    if (ctx->solve_program) glDeleteProgram(ctx->solve_program);
    if (ctx->pressure_program) glDeleteProgram(ctx->pressure_program);
    if (ctx->finalize_program) glDeleteProgram(ctx->finalize_program);
    if (ctx->microbe_update_program) glDeleteProgram(ctx->microbe_update_program);
    if (ctx->bounds_program) glDeleteProgram(ctx->bounds_program);
    if (ctx->field_shader) glDeleteProgram(ctx->field_shader);
    if (ctx->surface_shader) glDeleteProgram(ctx->surface_shader);
    if (ctx->field_texture) glDeleteTextures(1, &ctx->field_texture);
    if (ctx->field_fbo) glDeleteFramebuffers(1, &ctx->field_fbo);
    if (ctx->billboard_vbo) glDeleteBuffers(1, &ctx->billboard_vbo);
    if (ctx->billboard_vao) glDeleteVertexArrays(1, &ctx->billboard_vao);
    if (ctx->quad_vbo) glDeleteBuffers(1, &ctx->quad_vbo);
    if (ctx->quad_vao) glDeleteVertexArrays(1, &ctx->quad_vao);

    free(ctx->particles_cpu);
    free(ctx->constraints_cpu);
    free(ctx->microbes_cpu);
    free(ctx);
}

// Unified amoeba appearance - all cells look identical for consistent EC&M model
static const float AMOEBA_COLOR[4] = {0.46f, 0.92f, 0.74f, 0.85f};  // Translucent mint
static const float AMOEBA_STIFFNESS = 200.0f;  // Stiff constraints to balance strong pseudopod forces
static const float AMOEBA_PARTICLE_RADIUS = 0.35f;  // Particle spacing in filled disc

void xpbd_spawn_microbe(XpbdContext *ctx, float x, float z, int type, float seed) {
    if (!ctx || !ctx->ready) return;
    if (ctx->microbe_count >= ctx->max_microbes) return;

    int m_id = ctx->microbe_count;
    int p_start = m_id * XPBD_PARTICLES_PER_MICROBE;
    int c_start = m_id * XPBD_CONSTRAINTS_PER_MICROBE;

    // Two-layer structure: skeleton (internal) + membrane (external)
    const int SKELETON_COUNT = 16;  // First 16 particles: skeleton
    const int MEMBRANE_COUNT = 16;  // Last 16 particles: membrane
    const float TWO_PI = 2.0f * 3.14159265358979323846f;

    int particle_idx = 0;

    // Layer 1: SKELETON (internal structure, invisible)
    // Distributed in concentric rings for structural support
    const int skel_ring_counts[3] = {1, 5, 10};
    const float skel_ring_radii[3] = {0.0f, 0.5f, 1.0f};

    for (int ring = 0; ring < 3; ring++) {
        const int count = skel_ring_counts[ring];
        const float radius = skel_ring_radii[ring];

        for (int i = 0; i < count; i++) {
            if (particle_idx >= SKELETON_COUNT) break;

            float angle = (count > 1) ? ((float)i / (float)count * TWO_PI) : 0.0f;
            float hash_seed = seed * 1000.0f + (float)particle_idx;
            float hash1 = fmodf(sinf(hash_seed * 127.1f) * 43758.5453123f, 1.0f);
            angle += (hash1 - 0.5f) * 0.3f;  // Small jitter

            const float px = x + cosf(angle) * radius;
            const float pz = z + sinf(angle) * radius;

            XpbdParticle *p = &ctx->particles_cpu[p_start + particle_idx];
            p->pos[0] = px;
            p->pos[1] = 0.0f;
            p->pos[2] = pz;
            p->pos[3] = 1.0f;  // inverse mass = 1

            p->pos_prev[0] = px;
            p->pos_prev[1] = 0.0f;
            p->pos_prev[2] = pz;
            p->pos_prev[3] = 0.0f;

            p->vel[0] = 0.0f;
            p->vel[1] = 0.0f;
            p->vel[2] = 0.0f;
            p->vel[3] = (float)m_id;  // microbe ID

            p->data[0] = (float)particle_idx;  // particle index in microbe
            p->data[1] = (float)c_start;
            p->data[2] = (float)XPBD_CONSTRAINTS_PER_MICROBE;
            p->data[3] = 0.0f;

            particle_idx++;
        }
    }

    // Layer 2: MEMBRANE (external elastic skin, visible)
    // Evenly distributed circle around skeleton
    const float membrane_radius = 1.5f;  // Outer radius
    for (int i = 0; i < MEMBRANE_COUNT; i++) {
        float angle = ((float)i / (float)MEMBRANE_COUNT) * TWO_PI;
        const float px = x + cosf(angle) * membrane_radius;
        const float pz = z + sinf(angle) * membrane_radius;

        XpbdParticle *p = &ctx->particles_cpu[p_start + particle_idx];
        p->pos[0] = px;
        p->pos[1] = 0.0f;
        p->pos[2] = pz;
        p->pos[3] = 1.0f;  // inverse mass = 1

        p->pos_prev[0] = px;
        p->pos_prev[1] = 0.0f;
        p->pos_prev[2] = pz;
        p->pos_prev[3] = 0.0f;

        p->vel[0] = 0.0f;
        p->vel[1] = 0.0f;
        p->vel[2] = 0.0f;
        p->vel[3] = (float)m_id;  // microbe ID

        p->data[0] = (float)particle_idx;  // particle index in microbe
        p->data[1] = (float)c_start;
        p->data[2] = (float)XPBD_CONSTRAINTS_PER_MICROBE;
        p->data[3] = 1.0f;  // Mark as membrane particle (data.w = 1.0)

        particle_idx++;
    }

    // Constraint network: skeleton shape + membrane elasticity + coupling
    // Pure approach: deterministic constraint creation without conditional side effects
    const float stiffness = AMOEBA_STIFFNESS;
    const float base_compliance = 1.0f / (stiffness * 100.0f);
    int c_idx = 0;

    // Helper macro for constraint creation
    #define ADD_CONSTRAINT(i1, i2, compl_mult) do { \
        if (c_idx < XPBD_CONSTRAINTS_PER_MICROBE) { \
            const XpbdParticle *p1 = &ctx->particles_cpu[p_start + (i1)]; \
            const XpbdParticle *p2 = &ctx->particles_cpu[p_start + (i2)]; \
            const float dx = p1->pos[0] - p2->pos[0]; \
            const float dz = p1->pos[2] - p2->pos[2]; \
            const float rest_len = sqrtf(dx * dx + dz * dz); \
            XpbdConstraint *c = &ctx->constraints_cpu[c_start + c_idx]; \
            c->p1 = p_start + (i1); \
            c->p2 = p_start + (i2); \
            c->rest_length = rest_len; \
            c->compliance = base_compliance * (compl_mult); \
            c->lambda = 0.0f; \
            c_idx++; \
        } \
    } while(0)

    // 1. SKELETON constraints (shape maintenance) - stiff
    int skel_ring_start[3] = {0, 1, 6};
    for (int ring = 0; ring < 3; ring++) {
        int start = skel_ring_start[ring];
        int count = skel_ring_counts[ring];
        if (count > 1) {
            // Ring adjacency
            for (int i = 0; i < count; i++) {
                int next = (i + 1) % count;
                ADD_CONSTRAINT(start + i, start + next, 0.5f);  // Stiff skeleton
            }
        }
    }
    // Radial skeleton connections
    for (int i = 0; i < 5; i++) {
        ADD_CONSTRAINT(0, 1 + i, 0.5f);  // Center to inner
    }
    for (int i = 0; i < 5; i++) {
        int outer_idx = (i * 2) % 10;  // Connect to outer ring
        ADD_CONSTRAINT(1 + i, 6 + outer_idx, 0.7f);
    }

    // 2. MEMBRANE constraints (elasticity) - soft springs
    const int MEMBRANE_START = SKELETON_COUNT;
    for (int i = 0; i < MEMBRANE_COUNT; i++) {
        int next = (i + 1) % MEMBRANE_COUNT;
        ADD_CONSTRAINT(MEMBRANE_START + i, MEMBRANE_START + next, 3.0f);  // Soft elastic membrane
    }

    // 3. ATTACHMENT constraints (skeleton to membrane coupling)
    // Each skeleton particle attaches to nearest membrane particles
    for (int i = 0; i < SKELETON_COUNT; i++) {
        // Find nearest 2 membrane particles (simplified: use modulo mapping)
        int mem_idx = (i * MEMBRANE_COUNT) / SKELETON_COUNT;
        ADD_CONSTRAINT(i, MEMBRANE_START + mem_idx, 2.0f);  // Moderate coupling
    }

    #undef ADD_CONSTRAINT

    // Fill remaining constraint slots with dummy constraints
    while (c_idx < XPBD_CONSTRAINTS_PER_MICROBE) {
        XpbdConstraint *c = &ctx->constraints_cpu[c_start + c_idx];
        c->p1 = p_start;
        c->p2 = p_start;
        c->rest_length = 0.0f;
        c->compliance = 1.0f;
        c->lambda = 0.0f;
        c_idx++;
    }

    // Microbe metadata - unified amoeba appearance
    XpbdMicrobe *m = &ctx->microbes_cpu[m_id];
    m->center[0] = x;
    m->center[1] = 0.0f;
    m->center[2] = z;
    m->center[3] = 1.05f;  // Radius (matches outer ring radius)

    m->color[0] = AMOEBA_COLOR[0];
    m->color[1] = AMOEBA_COLOR[1];
    m->color[2] = AMOEBA_COLOR[2];
    m->color[3] = AMOEBA_COLOR[3];

    m->params[0] = 0.0f;  // type (unified)
    m->params[1] = stiffness;
    m->params[2] = seed;
    m->params[3] = 0.0f;  // squish amount

    m->aabb[0] = x - 1.5f;
    m->aabb[1] = z - 1.5f;
    m->aabb[2] = x + 1.5f;
    m->aabb[3] = z + 1.5f;

    ctx->microbe_count++;
    ctx->particle_count = ctx->microbe_count * XPBD_PARTICLES_PER_MICROBE;
    ctx->constraint_count = ctx->microbe_count * XPBD_CONSTRAINTS_PER_MICROBE;

    // Debug: verify membrane particles are spread out in CPU buffer
    if (m_id == 0) {
        printf("After spawn - CPU buffer membrane particles:\n");
        for (int i = 16; i < 19; i++) {  // First 3 membrane particles
            XpbdParticle *p = &ctx->particles_cpu[i];
            printf("  Membrane %d: (%.2f, %.2f)\n", i-16, p->pos[0], p->pos[2]);
        }
    }

    // Upload to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->particle_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_count * sizeof(XpbdParticle), ctx->particles_cpu);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->constraint_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ctx->constraint_count * sizeof(XpbdConstraint), ctx->constraints_cpu);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->microbe_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ctx->microbe_count * sizeof(XpbdMicrobe), ctx->microbes_cpu);
}

void xpbd_clear(XpbdContext *ctx) {
    if (!ctx) return;
    ctx->microbe_count = 0;
    ctx->particle_count = 0;
    ctx->constraint_count = 0;
}

// Full XPBD physics pipeline with collisions
void xpbd_update(XpbdContext *ctx, float dt, float bounds_x, float bounds_y, float cursor_x, float cursor_z) {
    if (!ctx || !ctx->ready || ctx->particle_count == 0) return;

    const int groups_p = (ctx->particle_count + 255) / 256;
    const int groups_m = (ctx->microbe_count + 63) / 64;
    const float cell_size = 2.0f;  // Grid cell size for collision detection

    // Step 1: Predict - apply forces and velocities
    glUseProgram(ctx->predict_program);
    glUniform1f(ctx->loc_predict_dt, dt);
    glUniform1i(ctx->loc_predict_count, ctx->particle_count);
    glUniform1i(ctx->loc_predict_ppm, XPBD_PARTICLES_PER_MICROBE);
    glUniform1f(ctx->loc_predict_time, (float)GetTime());
    glUniform2f(ctx->loc_predict_cursor, cursor_x, cursor_z);
    glUniform2f(ctx->loc_predict_bounds, bounds_x, bounds_y);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->microbe_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 2: Build spatial grid for collision detection
    glUseProgram(ctx->grid_insert_program);
    glUniform2f(ctx->loc_grid_bounds, bounds_x, bounds_y);
    glUniform1f(ctx->loc_grid_cell, cell_size);
    glUniform2i(ctx->loc_grid_dim, GRID_W, GRID_H);
    glUniform1i(ctx->loc_grid_count, ctx->particle_count);

    // Clear grid heads
    int neg_one[GRID_CELLS];
    for (int i = 0; i < GRID_CELLS; i++) neg_one[i] = -1;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->grid_head_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(neg_one), neg_one);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->grid_head_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctx->grid_next_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 3: Generate collision constraints
    glUseProgram(ctx->collide_program);
    glUniform1f(ctx->loc_collide_dt, dt);
    glUniform2f(ctx->loc_collide_bounds, bounds_x, bounds_y);
    glUniform1f(ctx->loc_collide_cell, cell_size);
    glUniform2i(ctx->loc_collide_dim, GRID_W, GRID_H);
    glUniform1i(ctx->loc_collide_count, ctx->particle_count);
    glUniform1f(ctx->loc_collide_radius, 2.5f);  // Collision radius matching visual size
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->grid_head_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctx->grid_next_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 4: Solve - enforce shape constraints (iterative)
    glUseProgram(ctx->solve_program);
    glUniform1f(ctx->loc_solve_dt, dt);
    glUniform1i(ctx->loc_solve_mcount, ctx->microbe_count);
    glUniform1i(ctx->loc_solve_cpm, XPBD_CONSTRAINTS_PER_MICROBE);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->constraint_ssbo);
    for (int iter = 0; iter < XPBD_SOLVER_ITERATIONS; iter++) {
        glDispatchCompute(groups_m, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Step 5: Finalize - update velocities from position changes
    glUseProgram(ctx->finalize_program);
    glUniform1f(ctx->loc_finalize_dt, dt);
    glUniform1i(ctx->loc_finalize_pcount, ctx->particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 6: Update - recalculate microbe centers
    glUseProgram(ctx->microbe_update_program);
    glUniform1i(ctx->loc_microbe_count, ctx->microbe_count);
    glUniform1i(ctx->loc_microbe_ppm, XPBD_PARTICLES_PER_MICROBE);
    glUniform1i(ctx->loc_microbe_cpm, XPBD_CONSTRAINTS_PER_MICROBE);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->constraint_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctx->microbe_ssbo);
    glDispatchCompute(groups_m, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 7: Bounds enforcement - bounce off screen edges
    glUseProgram(ctx->bounds_program);
    glUniform1i(ctx->loc_bounds_count, ctx->microbe_count);
    glUniform1i(ctx->loc_bounds_ppm, XPBD_PARTICLES_PER_MICROBE);
    glUniform2f(ctx->loc_bounds_bounds, bounds_x, bounds_y);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->microbe_ssbo);
    glDispatchCompute(groups_m, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void xpbd_render(const XpbdContext *ctx, Camera3D camera) {
    if (!ctx || !ctx->ready || ctx->microbe_count <= 0) return;

    rlDrawRenderBatchActive();

    int width = GetRenderWidth();
    int height = GetRenderHeight();

    Matrix view = GetCameraMatrix(camera);
    Matrix proj = MatrixPerspective(DEG2RAD * camera.fovy,
                                    (float)width / (float)height,
                                    0.1f, 200.0f);
    Matrix vp = MatrixMultiply(view, proj);

    // Read particle data to sort membrane points
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->particle_ssbo);
    XpbdParticle *particles = (XpbdParticle*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->microbe_ssbo);
    XpbdMicrobe *microbes = (XpbdMicrobe*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

    if (particles && microbes) {
        // Use outline curve shader
        glUseProgram(ctx->outline_program);
        glUniformMatrix4fv(ctx->loc_outline_vp, 1, GL_FALSE, MatrixToFloatV(vp).v);
        glUniform1f(ctx->loc_outline_offset_radius, 0.0f);  // Offset done on CPU now

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->microbe_ssbo);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(4.0f);

        // Render each microbe's outline
        for (int m = 0; m < ctx->microbe_count; m++) {
            // Extract and sort membrane particles by angle
            typedef struct { float pos[4]; float angle; } OutlinePoint;
            OutlinePoint points[16];
            int point_count = 0;

            Vector2 center = {microbes[m].center[0], microbes[m].center[2]};

            for (int i = 0; i < XPBD_PARTICLES_PER_MICROBE; i++) {
                int p_idx = m * XPBD_PARTICLES_PER_MICROBE + i;
                XpbdParticle *p = &particles[p_idx];

                if (p->data[3] > 0.5f && point_count < 16) {
                    // Debug: print raw particle positions
                    static int printed_raw = 0;
                    if (m == 0 && point_count < 3 && !printed_raw) {
                        printf("Raw particle %d pos: (%.2f, %.2f)\n", point_count, p->pos[0], p->pos[2]);
                        if (point_count == 2) printed_raw = 1;
                    }

                    // Calculate direction from center to particle
                    float dx = p->pos[0] - center.x;
                    float dz = p->pos[2] - center.y;
                    float angle = atan2f(dz, dx);
                    float len = sqrtf(dx*dx + dz*dz);

                    // Normalize and offset outward by sphere radius
                    float offset = 0.3f;  // Sphere radius to wrap around particle
                    float nx = len > 0.001f ? dx / len : 1.0f;
                    float nz = len > 0.001f ? dz / len : 0.0f;

                    // Offset from particle position, not center
                    points[point_count].pos[0] = p->pos[0] + nx * offset;
                    points[point_count].pos[1] = 0;
                    points[point_count].pos[2] = p->pos[2] + nz * offset;
                    points[point_count].pos[3] = 0;
                    points[point_count].angle = angle;
                    point_count++;
                }
            }

            // Sort by angle
            for (int i = 0; i < point_count - 1; i++) {
                for (int j = i + 1; j < point_count; j++) {
                    if (points[j].angle < points[i].angle) {
                        OutlinePoint temp = points[i];
                        points[i] = points[j];
                        points[j] = temp;
                    }
                }
            }

            if (point_count > 3) {
                // Debug: print first microbe's points
                static int printed = 0;
                if (m == 0 && !printed) {
                    printf("Microbe 0 points:\n");
                    for (int i = 0; i < 3 && i < point_count; i++) {
                        printf("  Point %d: (%.2f, %.2f, %.2f)\n", i,
                               points[i].pos[0], points[i].pos[1], points[i].pos[2]);
                    }
                    printed = 1;
                }

                // Upload sorted points to SSBO
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->outline_points_ssbo);
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, point_count * sizeof(float) * 4, points);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // Ensure GPU sees updated data
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->outline_points_ssbo);

                // Set uniforms for this microbe
                glUniform1i(ctx->loc_outline_point_count, point_count);
                glUniform1i(ctx->loc_outline_microbe_index, m);

                // Draw curve as line loop (closed curve)
                glBindVertexArray(ctx->outline_vao);
                int subdivisions = 16;
                int total_verts = point_count * subdivisions;
                glDrawArrays(GL_LINE_LOOP, 0, total_verts);
                glBindVertexArray(0);
            }
        }

        glUseProgram(0);
        glLineWidth(1.0f);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->particle_ssbo);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->microbe_ssbo);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

unsigned int xpbd_get_particle_ssbo(const XpbdContext *ctx) {
    return ctx ? ctx->particle_ssbo : 0;
}

unsigned int xpbd_get_microbe_ssbo(const XpbdContext *ctx) {
    return ctx ? ctx->microbe_ssbo : 0;
}

int xpbd_get_microbe_count(const XpbdContext *ctx) {
    return ctx ? ctx->microbe_count : 0;
}

int xpbd_get_particle_count(const XpbdContext *ctx) {
    return ctx ? ctx->particle_count : 0;
}

void xpbd_debug_print(const XpbdContext *ctx) {
    if (!ctx) return;

    // Download particle data from GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->particle_ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_count * sizeof(XpbdParticle), ctx->particles_cpu);

    fprintf(stderr, "\n=== XPBD DEBUG (frame snapshot) ===\n");
    for (int m = 0; m < ctx->microbe_count; m++) {
        fprintf(stderr, "Microbe %d:\n", m);
        int p_start = m * XPBD_PARTICLES_PER_MICROBE;

        // Center particle
        XpbdParticle *p0 = &ctx->particles_cpu[p_start];
        fprintf(stderr, "  p0(center): (%.2f, %.2f, %.2f)\n", p0->pos[0], p0->pos[1], p0->pos[2]);

        // Check distances from center
        float min_dist = 1e10f, max_dist = 0.0f;
        for (int i = 1; i < XPBD_PARTICLES_PER_MICROBE; i++) {
            XpbdParticle *p = &ctx->particles_cpu[p_start + i];
            float dx = p->pos[0] - p0->pos[0];
            float dz = p->pos[2] - p0->pos[2];
            float dist = sqrtf(dx*dx + dz*dz);
            if (dist < min_dist) min_dist = dist;
            if (dist > max_dist) max_dist = dist;
        }
        fprintf(stderr, "  Particle distance from center: min=%.2f max=%.2f\n", min_dist, max_dist);
    }
    fprintf(stderr, "=====================================\n\n");
    fflush(stderr);
}
