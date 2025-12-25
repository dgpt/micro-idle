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
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

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

static const char *kComputeInsertShader =
    "#version 460 core\n"
    "layout(local_size_x = " STR(GPU_WORKGROUP_SIZE) ", local_size_y = 1, local_size_z = 1) in;\n"
    "struct Entity { vec4 pos; vec4 vel; vec4 color; vec4 params; };\n"
    "layout(std430, binding = 0) buffer Entities { Entity entities[]; };\n"
    "uniform int u_active;\n"
    "uniform vec2 u_bounds;\n"
    "uniform float u_cell;\n"
    "uniform ivec2 u_grid_dim;\n"
    "layout(std430, binding = 1) buffer GridHead { int head[]; };\n"
    "layout(std430, binding = 2) buffer GridNext { int next[]; };\n"
    "void main() {\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    "    if (idx >= uint(u_active) || idx >= entities.length()) return;\n"
    "    vec3 p = entities[idx].pos.xyz;\n"
    "    int gx = int((p.x + u_bounds.x) / (u_cell));\n"
    "    int gz = int((p.z + u_bounds.y) / (u_cell));\n"
    "    gx = clamp(gx, 0, u_grid_dim.x - 1);\n"
    "    gz = clamp(gz, 0, u_grid_dim.y - 1);\n"
    "    int cell = gz * u_grid_dim.x + gx;\n"
    "    next[idx] = atomicExchange(head[cell], int(idx));\n"
    "}\n";

static const char *kComputeCollideShader =
    "#version 460 core\n"
    "layout(local_size_x = " STR(GPU_WORKGROUP_SIZE) ", local_size_y = 1, local_size_z = 1) in;\n"
    "struct Entity { vec4 pos; vec4 vel; vec4 color; vec4 params; };\n"
    "layout(std430, binding = 0) buffer Entities { Entity entities[]; };\n"
    "layout(std430, binding = 1) buffer GridHead { int head[]; };\n"
    "layout(std430, binding = 2) buffer GridNext { int next[]; };\n"
    "uniform float u_dt;\n"
    "uniform vec2 u_bounds;\n"
    "uniform float u_cell;\n"
    "uniform ivec2 u_grid_dim;\n"
    "uniform int u_active;\n"
    "void main() {\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    "    if (idx >= uint(u_active) || idx >= entities.length()) return;\n"
    "    vec3 p = entities[idx].pos.xyz;\n"
    "    vec3 v = entities[idx].vel.xyz;\n"
    "    float radius = entities[idx].params.x;\n"
    "    int gx = int((p.x + u_bounds.x) / (u_cell));\n"
    "    int gz = int((p.z + u_bounds.y) / (u_cell));\n"
    "    gx = clamp(gx, 0, u_grid_dim.x - 1);\n"
    "    gz = clamp(gz, 0, u_grid_dim.y - 1);\n"
    "    vec3 push = vec3(0.0);\n"
    "    float squish = entities[idx].params.y;\n"
    "    for (int dz = -1; dz <= 1; ++dz) {\n"
    "        int nz = gz + dz;\n"
    "        if (nz < 0 || nz >= u_grid_dim.y) continue;\n"
    "        for (int dx = -1; dx <= 1; ++dx) {\n"
    "            int nx = gx + dx;\n"
    "            if (nx < 0 || nx >= u_grid_dim.x) continue;\n"
    "            int cell = nz * u_grid_dim.x + nx;\n"
    "            for (int j = head[cell]; j != -1; j = next[j]) {\n"
    "                if (j == int(idx)) continue;\n"
    "                vec3 op = entities[j].pos.xyz;\n"
    "                float orad = entities[j].params.x;\n"
    "                vec3 d = p - op;\n"
    "                float dist2 = dot(d, d);\n"
    "                float r = radius + orad;\n"
    "                if (dist2 < r * r && dist2 > 0.00001) {\n"
    "                    float dist = sqrt(dist2);\n"
    "                    vec3 n = d / dist;\n"
    "                    float overlap = r - dist;\n"
    "                    push += n * overlap * 0.7;\n"
    "                    squish = max(squish, overlap / r);\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    v += push * 4.2;\n"
    "    v *= 0.965;\n"
    "    p += v * u_dt;\n"
    "    if (p.x < -u_bounds.x || p.x > u_bounds.x) v.x *= -0.8;\n"
    "    if (p.z < -u_bounds.y || p.z > u_bounds.y) v.z *= -0.8;\n"
    "    p = clamp(p, vec3(-u_bounds.x, 0.0, -u_bounds.y), vec3(u_bounds.x, 0.0, u_bounds.y));\n"
    "    entities[idx].pos = vec4(p, 1.0);\n"
    "    entities[idx].vel = vec4(v, 0.0);\n"
    "    entities[idx].params.y = clamp(max(0.0, squish - u_dt * 0.7), 0.0, 1.2);\n"
    "}\n";

static const char *kVertexShader =
    "#version 460 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "struct Entity { vec4 pos; vec4 vel; vec4 color; vec4 params; };\n"
    "layout(std430, binding = 0) readonly buffer Entities { Entity entities[]; };\n"
    "layout(location = 0) uniform mat4 u_vp;\n"
    "uniform float u_time;\n"
    "out vec4 vColor;\n"
    "out vec2 vLocal;\n"
    "out float vType;\n"
    "out float vSeed;\n"
    "out float vSquish;\n"
    "void main() {\n"
    "    uint idx = uint(gl_InstanceID);\n"
    "    vec3 center = entities[idx].pos.xyz;\n"
    "    float scale = entities[idx].params.x;\n"
    "    float squish = entities[idx].params.y;\n"
    "    float t = floor(entities[idx].params.z + 0.5);\n"
    "    float seed = entities[idx].params.w;\n"
    "    float isBac = 1.0 - step(0.5, abs(t - 1.0));\n"
    "    float isVib = 1.0 - step(0.5, abs(t - 2.0));\n"
    "    float isSpi = 1.0 - step(0.5, abs(t - 3.0));\n"
    "    float elong = 1.0 + 0.7 * (isBac + isVib) + 1.1 * isSpi;\n"
    "    float height = 1.0 - 0.15 * isSpi;\n"
    "    vec2 dir = normalize(entities[idx].vel.xz + vec2(0.0001));\n"
    "    vec2 perp = vec2(-dir.y, dir.x);\n"
    "    float stretch = 1.0 + squish * 0.6;\n"
    "    float squash = 1.0 - squish * 0.4;\n"
    "    vec2 local = vec2(aPos.x * elong, aPos.z * height);\n"
    "    vec2 warped = dir * (dot(local, dir) * stretch) + perp * (dot(local, perp) * squash);\n"
    "    vec2 local01 = local * 2.0;\n"
    "    float dome = max(0.0, 1.0 - dot(local01, local01));\n"
    "    float undulate = 1.0 + 0.05 * sin(u_time * (1.2 + seed * 1.7) + dot(local01, vec2(6.0, 4.0)));\n"
    "    vec3 world = center + vec3(warped.x, dome * 0.35 * undulate, warped.y) * scale;\n"
    "    gl_Position = u_vp * vec4(world, 1.0);\n"
    "    vColor = entities[idx].color;\n"
    "    vLocal = local01;\n"
    "    vType = t;\n"
    "    vSeed = seed;\n"
    "    vSquish = squish;\n"
    "}\n";

static const char *kFragmentShader =
    "#version 460 core\n"
    "in vec4 vColor;\n"
    "in vec2 vLocal;\n"
    "in float vType;\n"
    "in float vSeed;\n"
    "in float vSquish;\n"
    "uniform float u_time;\n"
    "out vec4 fragColor;\n"
    "const float PI = 3.14159265;\n"
    "float hash21(vec2 p) {\n"
    "    return fract(sin(dot(p, vec2(127.1, 311.7)) + vSeed * 13.7) * 43758.5453);\n"
    "}\n"
    "float sdf_capsule(vec2 p, float halfLen, float radius) {\n"
    "    vec2 d = vec2(abs(p.x) - halfLen, p.y);\n"
    "    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0) - radius;\n"
    "}\n"
    "float sdf_blob(vec2 p, float base, float freq, float amp, float seed) {\n"
    "    float ang = atan(p.y, p.x);\n"
    "    float wobble = sin(ang * freq + seed) * amp;\n"
    "    return length(p) - (base + wobble);\n"
    "}\n"
    "void main() {\n"
    "    vec2 p = vLocal;\n"
    "    float seed = vSeed * 6.2831853;\n"
    "    float isC = 1.0 - step(0.5, abs(vType - 0.0));\n"
    "    float isB = 1.0 - step(0.5, abs(vType - 1.0));\n"
    "    float isV = 1.0 - step(0.5, abs(vType - 2.0));\n"
    "    float isS = 1.0 - step(0.5, abs(vType - 3.0));\n"
    "    vec2 pB = vec2(p.x, p.y * 0.8);\n"
    "    vec2 pV = pB;\n"
    "    pV.y += 0.35 * sin(pV.x * 1.6 + seed + u_time * 0.6);\n"
    "    vec2 pS = p;\n"
    "    pS.y += 0.45 * sin(pS.x * 3.0 + seed + u_time * 0.9);\n"
    "    pS.x += 0.12 * sin(pS.y * 3.2 + seed * 0.7);\n"
    "    float angle = atan(p.y, p.x);\n"
    "    float coccusWobble = 0.08 * sin(angle * 7.0 + seed * 1.8);\n"
    "    float sdfC = length(p) - (0.95 + coccusWobble);\n"
    "    float sdfB = sdf_capsule(pB, 0.85, 0.52);\n"
    "    float sdfV = sdf_capsule(pV, 0.85, 0.5);\n"
    "    float sdfS = sdf_capsule(pS, 0.9, 0.44);\n"
    "    float sdf = sdfC * isC + sdfB * isB + sdfV * isV + sdfS * isS;\n"
    "    float body = smoothstep(0.14, -0.14, sdf);\n"
    "    vec2 dir = normalize(vec2(cos(seed), sin(seed)) + 0.0001);\n"
    "    float line = abs(dot(p, vec2(-dir.y, dir.x)));\n"
    "    float along = dot(p, dir);\n"
    "    float appendage = smoothstep(0.04, 0.0, line) * smoothstep(1.1, 0.2, along) * (isB + isV) * 0.25;\n"
    "    float ciliaBands = abs(fract((angle / (2.0 * PI)) * 16.0) - 0.5);\n"
    "    float cilia = smoothstep(0.22, 0.0, ciliaBands) * smoothstep(0.05, -0.05, sdf) * (isC + isB * 0.5 + isV * 0.5);\n"
    "    float alpha = max(body, appendage + cilia * 0.22);\n"
    "    if (alpha <= 0.001) discard;\n"
    "    float outline = smoothstep(0.11, -0.11, sdf);\n"
    "    float rim = smoothstep(0.02, -0.02, sdf + 0.08);\n"
    "    vec3 base = vColor.rgb;\n"
    "    vec3 fill = mix(base, vec3(1.0), 0.18);\n"
    "    float dotField = smoothstep(0.24, 0.0, length(fract((p + 1.0) * 5.0) - 0.5));\n"
    "    float dotMask = step(0.7, hash21(floor((p + 1.0) * 5.0)));\n"
    "    float stripe = smoothstep(0.1, 0.0, abs(sin(p.x * 5.0 + seed * 1.4))) * isB;\n"
    "    float zig = smoothstep(0.1, 0.0, abs(sin(p.x * 7.0 + p.y * 2.2 + seed))) * isS;\n"
    "    float speckle = dotField * dotMask * (isC + isV * 0.6);\n"
    "    vec3 spotColor = mix(base, vec3(0.98, 0.9, 0.8), 0.55);\n"
    "    vec3 color = mix(fill, spotColor, speckle * 0.9);\n"
    "    color = mix(color, base * 0.7, stripe * 0.35 + zig * 0.35);\n"
    "    vec2 nucPos = vec2(cos(seed * 0.7), sin(seed * 0.9)) * 0.22;\n"
    "    float nucleus = smoothstep(0.32, 0.0, length(p - nucPos) - 0.18);\n"
    "    color = mix(color, vec3(0.98, 0.72, 0.9), nucleus * 0.7);\n"
    "    vec3 outlineColor = base * 0.35;\n"
    "    color = mix(outlineColor, color, outline);\n"
    "    color = mix(color, vec3(1.0), rim * 0.08);\n"
    "    alpha = max(body, appendage + cilia * 0.1);\n"
    "    fragColor = vec4(color, alpha);\n"
    "}\n";

static unsigned int compile_shader(GLenum type, const char *source) {
#ifdef GPU_SIM_TESTING
    if (gpu_sim_fail_mode == 1) {
        source = "invalid shader";
    }
#endif
    unsigned int shader = glCreateShader(type);
    if (!shader) {
        return 0;
    }
    glShaderSource(shader, 1, &source, NULL);
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
                fprintf(stderr, "shader compile failed: %s\n", log);
                free(log);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
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

    unsigned int cs_insert = compile_shader(GL_COMPUTE_SHADER, kComputeInsertShader);
    if (cs_insert == 0) return false;
    sim->sim_insert_program = link_program(cs_insert);
    glDeleteShader(cs_insert);

    unsigned int cs_collide = compile_shader(GL_COMPUTE_SHADER, kComputeCollideShader);
    if (cs_collide == 0) return false;
    sim->sim_collide_program = link_program(cs_collide);
    glDeleteShader(cs_collide);

    sim->render_shader = LoadShaderFromMemory(kVertexShader, kFragmentShader);
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
