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
    unsigned int finalize_program;
    unsigned int microbe_update_program;

    // Rendering
    unsigned int render_shader;
    unsigned int render_vao;
    unsigned int render_vbo;
    unsigned int render_ebo;
    int render_index_count;

    // Render uniform locations
    int loc_render_vp;
    int loc_render_time;
    int loc_render_ppm;

    // Uniform locations
    int loc_predict_dt;
    int loc_predict_count;
    int loc_predict_ppm;
    int loc_predict_time;

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
    int loc_solve_count;

    int loc_finalize_dt;
    int loc_finalize_pcount;
    int loc_finalize_ccount;
    int loc_finalize_mode;

    int loc_microbe_count;
    int loc_microbe_ppm;
    int loc_microbe_cpm;

    // CPU staging buffers for initialization
    XpbdParticle *particles_cpu;
    XpbdConstraint *constraints_cpu;
    XpbdMicrobe *microbes_cpu;

    bool ready;
};

static bool init_render_pipeline(XpbdContext *ctx) {
    unsigned int vs = compile_shader(GL_VERTEX_SHADER, "xpbd_microbe.vert");
    unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, "xpbd_microbe.frag");
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    ctx->render_shader = link_program_pair(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ctx->render_shader) {
        return false;
    }

    ctx->loc_render_vp = glGetUniformLocation(ctx->render_shader, "u_vp");
    ctx->loc_render_time = glGetUniformLocation(ctx->render_shader, "u_time");
    ctx->loc_render_ppm = glGetUniformLocation(ctx->render_shader, "u_particles_per_microbe");

    // High-resolution mesh for smooth amoeba-like deformation
    const int radial_segments = 128;  // Much higher resolution around the perimeter
    const int radial_rings = 8;       // Multiple rings for smoother radial interpolation
    const int vert_count = radial_rings * radial_segments + 1;  // +1 for center
    const int tri_count = (radial_rings - 1) * radial_segments * 2 + radial_segments;
    ctx->render_index_count = tri_count * 3;

    float *verts = (float *)malloc(sizeof(float) * 3 * (size_t)vert_count);
    unsigned int *indices = (unsigned int *)malloc(sizeof(unsigned int) * (size_t)ctx->render_index_count);
    if (!verts || !indices) {
        free(verts);
        free(indices);
        return false;
    }

    // Center vertex
    verts[0] = 0.0f;
    verts[1] = 0.0f;
    verts[2] = 0.0f;

    // Create concentric rings
    int vert_idx = 1;
    for (int ring = 0; ring < radial_rings; ring++) {
        float radius = (float)(ring + 1) / (float)radial_rings;  // 0 to 1
        for (int seg = 0; seg < radial_segments; seg++) {
            float angle = (float)seg * 6.28318530718f / (float)radial_segments;
            verts[vert_idx * 3 + 0] = cosf(angle) * radius;
            verts[vert_idx * 3 + 1] = 0.0f;
            verts[vert_idx * 3 + 2] = sinf(angle) * radius;
            vert_idx++;
        }
    }

    // Build indices
    int idx = 0;

    // Center fan to first ring
    for (int seg = 0; seg < radial_segments; seg++) {
        indices[idx++] = 0;
        indices[idx++] = 1 + seg;
        indices[idx++] = 1 + ((seg + 1) % radial_segments);
    }

    // Connect rings
    for (int ring = 0; ring < radial_rings - 1; ring++) {
        int ring_start = 1 + ring * radial_segments;
        int next_ring_start = 1 + (ring + 1) * radial_segments;

        for (int seg = 0; seg < radial_segments; seg++) {
            int next_seg = (seg + 1) % radial_segments;

            // First triangle
            indices[idx++] = ring_start + seg;
            indices[idx++] = next_ring_start + seg;
            indices[idx++] = ring_start + next_seg;

            // Second triangle
            indices[idx++] = ring_start + next_seg;
            indices[idx++] = next_ring_start + seg;
            indices[idx++] = next_ring_start + next_seg;
        }
    }

    glGenVertexArrays(1, &ctx->render_vao);
    glBindVertexArray(ctx->render_vao);

    glGenBuffers(1, &ctx->render_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->render_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * (size_t)vert_count, verts, GL_STATIC_DRAW);

    glGenBuffers(1, &ctx->render_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->render_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * (size_t)ctx->render_index_count, indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    free(verts);
    free(indices);
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

    XpbdContext *ctx = calloc(1, sizeof(XpbdContext));
    if (!ctx) return NULL;

    ctx->max_microbes = max_microbes;
    ctx->microbe_count = 0;

    int max_particles = max_microbes * XPBD_PARTICLES_PER_MICROBE;
    int max_constraints = max_microbes * XPBD_CONSTRAINTS_PER_MICROBE;

    // Allocate CPU staging buffers
    ctx->particles_cpu = calloc(max_particles, sizeof(XpbdParticle));
    ctx->constraints_cpu = calloc(max_constraints, sizeof(XpbdConstraint));
    ctx->microbes_cpu = calloc(max_microbes, sizeof(XpbdMicrobe));
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
    ctx->finalize_program = compile_compute_program("xpbd_finalize.comp");
    ctx->microbe_update_program = compile_compute_program("xpbd_microbe_update.comp");

    if (!ctx->predict_program || !ctx->grid_insert_program || !ctx->collide_program ||
        !ctx->solve_program || !ctx->finalize_program || !ctx->microbe_update_program) {
        fprintf(stderr, "xpbd: failed to compile compute programs\n");
        goto fail;
    }

    if (!init_render_pipeline(ctx)) {
        fprintf(stderr, "xpbd: failed to init render pipeline\n");
        goto fail;
    }

    // Get uniform locations
    ctx->loc_predict_dt = glGetUniformLocation(ctx->predict_program, "u_dt");
    ctx->loc_predict_count = glGetUniformLocation(ctx->predict_program, "u_particle_count");
    ctx->loc_predict_ppm = glGetUniformLocation(ctx->predict_program, "u_particles_per_microbe");
    ctx->loc_predict_time = glGetUniformLocation(ctx->predict_program, "u_time");

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
    ctx->loc_solve_count = glGetUniformLocation(ctx->solve_program, "u_constraint_count");

    ctx->loc_finalize_dt = glGetUniformLocation(ctx->finalize_program, "u_dt");
    ctx->loc_finalize_pcount = glGetUniformLocation(ctx->finalize_program, "u_particle_count");
    ctx->loc_finalize_ccount = glGetUniformLocation(ctx->finalize_program, "u_constraint_count");
    ctx->loc_finalize_mode = glGetUniformLocation(ctx->finalize_program, "u_mode");

    ctx->loc_microbe_count = glGetUniformLocation(ctx->microbe_update_program, "u_microbe_count");
    ctx->loc_microbe_ppm = glGetUniformLocation(ctx->microbe_update_program, "u_particles_per_microbe");
    ctx->loc_microbe_cpm = glGetUniformLocation(ctx->microbe_update_program, "u_constraints_per_microbe");

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
    if (ctx->finalize_program) glDeleteProgram(ctx->finalize_program);
    if (ctx->microbe_update_program) glDeleteProgram(ctx->microbe_update_program);
    if (ctx->render_shader) glDeleteProgram(ctx->render_shader);
    if (ctx->render_vbo) glDeleteBuffers(1, &ctx->render_vbo);
    if (ctx->render_ebo) glDeleteBuffers(1, &ctx->render_ebo);
    if (ctx->render_vao) glDeleteVertexArrays(1, &ctx->render_vao);

    free(ctx->particles_cpu);
    free(ctx->constraints_cpu);
    free(ctx->microbes_cpu);
    free(ctx);
}

// Color palettes for microbe types
static const float TYPE_COLORS[4][4] = {
    {0.46f, 0.92f, 0.74f, 0.85f},  // Coccus - mint
    {0.47f, 0.78f, 0.97f, 0.85f},  // Bacillus - cyan
    {0.97f, 0.78f, 0.33f, 0.85f},  // Vibrio - amber
    {0.55f, 0.46f, 0.98f, 0.85f},  // Spirillum - indigo
};

// Stiffness per type (compliance = 1/stiffness)
static const float TYPE_STIFFNESS[4] = {
    500.0f,   // Coccus - quite stiff (spherical)
    300.0f,   // Bacillus - medium (rod)
    200.0f,   // Vibrio - soft (curved, flexible)
    250.0f,   // Spirillum - medium-soft (spiral)
};

// Base radius per type
static const float TYPE_RADIUS[4] = {
    0.9f,    // Coccus
    1.3f,    // Bacillus (elongated)
    1.05f,   // Vibrio
    1.05f,   // Spirillum
};

void xpbd_spawn_microbe(XpbdContext *ctx, float x, float z, int type, float seed) {
    if (!ctx || !ctx->ready) return;
    if (ctx->microbe_count >= ctx->max_microbes) return;

    int m_id = ctx->microbe_count;
    int p_start = m_id * XPBD_PARTICLES_PER_MICROBE;
    int c_start = m_id * XPBD_CONSTRAINTS_PER_MICROBE;

    type = type % 4;
    float base_radius = TYPE_RADIUS[type] * (0.8f + seed * 0.4f);
    float stiffness = TYPE_STIFFNESS[type];
    float compliance = 1.0f / stiffness;

    // Elongation for rod-shaped bacteria
    float elongation = 1.0f;
    if (type == 1) elongation = 1.8f;       // Bacillus
    else if (type == 2) elongation = 1.4f;  // Vibrio
    else if (type == 3) elongation = 1.6f;  // Spirillum

    // Create particles in an ellipse/ring
    for (int i = 0; i < XPBD_PARTICLES_PER_MICROBE; i++) {
        float angle = (float)i / XPBD_PARTICLES_PER_MICROBE * 2.0f * 3.14159f;
        float px = x + cosf(angle) * base_radius * elongation;
        float pz = z + sinf(angle) * base_radius;

        XpbdParticle *p = &ctx->particles_cpu[p_start + i];
        p->pos[0] = px;
        p->pos[1] = 0.0f;
        p->pos[2] = pz;
        p->pos[3] = 1.0f;  // inverse mass = 1

        p->pos_prev[0] = px;
        p->pos_prev[1] = 0.0f;
        p->pos_prev[2] = pz;
        p->pos_prev[3] = 0.0f;

        // Initial velocity (random drift)
        float vx = (seed - 0.5f) * 0.5f + sinf(seed * 10.0f + i) * 0.2f;
        float vz = (seed - 0.3f) * 0.5f + cosf(seed * 10.0f + i) * 0.2f;
        p->vel[0] = vx;
        p->vel[1] = 0.0f;
        p->vel[2] = vz;
        p->vel[3] = (float)m_id;  // microbe ID

        p->data[0] = (float)i;        // particle index in microbe
        p->data[1] = (float)c_start;  // constraint start
        p->data[2] = (float)XPBD_CONSTRAINTS_PER_MICROBE;
        p->data[3] = 0.0f;
    }

    // Create constraints
    int c_idx = 0;

    // Ring constraints (adjacent particles)
    for (int i = 0; i < XPBD_PARTICLES_PER_MICROBE; i++) {
        int j = (i + 1) % XPBD_PARTICLES_PER_MICROBE;
        XpbdParticle *p1 = &ctx->particles_cpu[p_start + i];
        XpbdParticle *p2 = &ctx->particles_cpu[p_start + j];

        float dx = p1->pos[0] - p2->pos[0];
        float dz = p1->pos[2] - p2->pos[2];
        float rest_len = sqrtf(dx * dx + dz * dz);

        XpbdConstraint *c = &ctx->constraints_cpu[c_start + c_idx];
        c->p1 = p_start + i;
        c->p2 = p_start + j;
        c->rest_length = rest_len;
        c->compliance = compliance;
        c->lambda = 0.0f;
        c_idx++;
    }

    // Cross constraints (opposite particles for shape stability)
    for (int i = 0; i < XPBD_PARTICLES_PER_MICROBE / 2; i++) {
        int j = i + XPBD_PARTICLES_PER_MICROBE / 2;
        XpbdParticle *p1 = &ctx->particles_cpu[p_start + i];
        XpbdParticle *p2 = &ctx->particles_cpu[p_start + j];

        float dx = p1->pos[0] - p2->pos[0];
        float dz = p1->pos[2] - p2->pos[2];
        float rest_len = sqrtf(dx * dx + dz * dz);

        XpbdConstraint *c = &ctx->constraints_cpu[c_start + c_idx];
        c->p1 = p_start + i;
        c->p2 = p_start + j;
        c->rest_length = rest_len;
        c->compliance = compliance * 0.5f;  // Stiffer cross-bracing
        c->lambda = 0.0f;
        c_idx++;
    }

    // Fill remaining constraint slots (if any) with dummy constraints
    while (c_idx < XPBD_CONSTRAINTS_PER_MICROBE) {
        XpbdConstraint *c = &ctx->constraints_cpu[c_start + c_idx];
        c->p1 = p_start;
        c->p2 = p_start;
        c->rest_length = 0.0f;
        c->compliance = 1.0f;
        c->lambda = 0.0f;
        c_idx++;
    }

    // Microbe metadata
    XpbdMicrobe *m = &ctx->microbes_cpu[m_id];
    m->center[0] = x;
    m->center[1] = 0.0f;
    m->center[2] = z;
    m->center[3] = base_radius;

    m->color[0] = TYPE_COLORS[type][0];
    m->color[1] = TYPE_COLORS[type][1];
    m->color[2] = TYPE_COLORS[type][2];
    m->color[3] = TYPE_COLORS[type][3];

    m->params[0] = (float)type;
    m->params[1] = stiffness;
    m->params[2] = seed;
    m->params[3] = 0.0f;  // squish amount

    m->aabb[0] = x - base_radius * elongation;
    m->aabb[1] = z - base_radius;
    m->aabb[2] = x + base_radius * elongation;
    m->aabb[3] = z + base_radius;

    ctx->microbe_count++;
    ctx->particle_count = ctx->microbe_count * XPBD_PARTICLES_PER_MICROBE;
    ctx->constraint_count = ctx->microbe_count * XPBD_CONSTRAINTS_PER_MICROBE;

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

void xpbd_update(XpbdContext *ctx, float dt, float bounds_x, float bounds_y) {
    if (!ctx || !ctx->ready) return;
    if (ctx->particle_count == 0) return;

    float cell = (bounds_x * 2.0f) / (float)GRID_W;
    int grid_dim[2] = {GRID_W, GRID_H};
    int groups_p = (ctx->particle_count + 255) / 256;
    int groups_c = (ctx->constraint_count + 255) / 256;
    int groups_m = (ctx->microbe_count + 63) / 64;

    // Step 1: Reset constraint lambdas
    glUseProgram(ctx->finalize_program);
    glUniform1f(ctx->loc_finalize_dt, dt);
    glUniform1i(ctx->loc_finalize_pcount, ctx->particle_count);
    glUniform1i(ctx->loc_finalize_ccount, ctx->constraint_count);
    glUniform1i(ctx->loc_finalize_mode, 1);  // reset lambdas
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->constraint_ssbo);
    glDispatchCompute(groups_c, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 2: Predict positions
    glUseProgram(ctx->predict_program);
    glUniform1f(ctx->loc_predict_dt, dt);
    glUniform1i(ctx->loc_predict_count, ctx->particle_count);
    glUniform1i(ctx->loc_predict_ppm, XPBD_PARTICLES_PER_MICROBE);
    glUniform1f(ctx->loc_predict_time, (float)GetTime());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->microbe_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 3: Build spatial grid
    // Clear head buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctx->grid_head_ssbo);
    int clear_val = -1;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32I, GL_RED_INTEGER, GL_INT, &clear_val);

    glUseProgram(ctx->grid_insert_program);
    glUniform2f(ctx->loc_grid_bounds, bounds_x, bounds_y);
    glUniform1f(ctx->loc_grid_cell, cell);
    glUniform2i(ctx->loc_grid_dim, grid_dim[0], grid_dim[1]);
    glUniform1i(ctx->loc_grid_count, ctx->particle_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->grid_head_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctx->grid_next_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 4: Inter-microbe collisions
    glUseProgram(ctx->collide_program);
    glUniform1f(ctx->loc_collide_dt, dt);
    glUniform2f(ctx->loc_collide_bounds, bounds_x, bounds_y);
    glUniform1f(ctx->loc_collide_cell, cell);
    glUniform2i(ctx->loc_collide_dim, grid_dim[0], grid_dim[1]);
    glUniform1i(ctx->loc_collide_count, ctx->particle_count);
    glUniform1f(ctx->loc_collide_radius, 0.4f);  // collision radius
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->grid_head_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctx->grid_next_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 5: Solve internal constraints (multiple iterations)
    glUseProgram(ctx->solve_program);
    glUniform1f(ctx->loc_solve_dt, dt);
    glUniform1i(ctx->loc_solve_count, ctx->constraint_count);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->constraint_ssbo);
    for (int iter = 0; iter < XPBD_SOLVER_ITERATIONS; iter++) {
        glDispatchCompute(groups_c, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Step 6: Update velocities
    glUseProgram(ctx->finalize_program);
    glUniform1f(ctx->loc_finalize_dt, dt);
    glUniform1i(ctx->loc_finalize_pcount, ctx->particle_count);
    glUniform1i(ctx->loc_finalize_ccount, ctx->constraint_count);
    glUniform1i(ctx->loc_finalize_mode, 0);  // update velocities
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->constraint_ssbo);
    glDispatchCompute(groups_p, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 7: Update microbe metadata
    glUseProgram(ctx->microbe_update_program);
    glUniform1i(ctx->loc_microbe_count, ctx->microbe_count);
    glUniform1i(ctx->loc_microbe_ppm, XPBD_PARTICLES_PER_MICROBE);
    glUniform1i(ctx->loc_microbe_cpm, XPBD_CONSTRAINTS_PER_MICROBE);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->constraint_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctx->microbe_ssbo);
    glDispatchCompute(groups_m, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void xpbd_render(const XpbdContext *ctx, Camera3D camera) {
    if (!ctx || !ctx->ready || ctx->microbe_count <= 0 || ctx->render_shader == 0) return;

    rlDrawRenderBatchActive();
    glViewport(0, 0, GetRenderWidth(), GetRenderHeight());

    Matrix view = GetCameraMatrix(camera);
    Matrix proj = MatrixPerspective(DEG2RAD * camera.fovy,
                                    (float)GetRenderWidth() / (float)GetRenderHeight(),
                                    0.1f, 200.0f);
    Matrix vp = MatrixMultiply(view, proj);

    glUseProgram(ctx->render_shader);
    if (ctx->loc_render_vp >= 0) {
        glUniformMatrix4fv(ctx->loc_render_vp, 1, GL_FALSE, MatrixToFloatV(vp).v);
    }
    if (ctx->loc_render_time >= 0) {
        glUniform1f(ctx->loc_render_time, (float)GetTime());
    }
    if (ctx->loc_render_ppm >= 0) {
        glUniform1i(ctx->loc_render_ppm, XPBD_PARTICLES_PER_MICROBE);
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(ctx->render_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->render_ebo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ctx->particle_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ctx->microbe_ssbo);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDrawElementsInstanced(GL_TRIANGLES,
                            ctx->render_index_count,
                            GL_UNSIGNED_INT,
                            (void *)0,
                            ctx->microbe_count);
    glBindVertexArray(0);
    glUseProgram(0);
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
    fprintf(stderr, "xpbd: %d microbes, %d particles, %d constraints\n",
            ctx->microbe_count, ctx->particle_count, ctx->constraint_count);
}
