#include "game/game.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "game/gpu_sim.h"
#ifdef ENABLE_VULKAN
#include "game/vk_compute.h"
#endif
#include "game/xpbd.h"
#include "external/glad.h"

typedef struct GameConfig {
    int max_count;
    int spawn_initial;
    int spawn_per_sec;
    float bounds_x;
    float bounds_z;
    float plane_width;
    float plane_height;
} GameConfig;

typedef struct GameState {
    GpuSim gpu;
#ifdef ENABLE_VULKAN
    VkComputeContext *vk;  // Vulkan compute for GPU simulation
#endif
    XpbdContext *xpbd;     // XPBD soft-body physics
    GameConfig config;
    float sim_time;
    float spawn_accum;
    int active_count;
    int screen_w;
    int screen_h;
    bool use_vk_compute;   // True if using Vulkan for simulation
    bool use_xpbd;         // True if using XPBD soft-body physics
} GameState;

static int parse_env_int(const char *name, int fallback) {
    const char *env = getenv(name);
    if (!env || *env == '\0') {
        return fallback;
    }
    char *end = NULL;
    long value = strtol(env, &end, 10);
    if (end == env || *end != '\0' || value <= 0 || value > INT_MAX) {
        return fallback;
    }
    return (int)value;
}

static float parse_env_float(const char *name, float fallback) {
    const char *env = getenv(name);
    if (!env || *env == '\0') {
        return fallback;
    }
    char *end = NULL;
    float value = strtof(env, &end);
    if (end == env || *end != '\0' || !isfinite(value) || value <= 0.0f) {
        return fallback;
    }
    return value;
}

static int game_max_entities(void) {
    int count = parse_env_int("MICRO_IDLE_ENTITY_COUNT", GAME_GPU_ENTITY_COUNT);
    int cap = parse_env_int("MICRO_IDLE_ENTITY_MAX", count);
    return (cap < count) ? cap : count;
}

static int game_initial_entities(int max_entities) {
    int initial = parse_env_int("MICRO_IDLE_INITIAL_ENTITIES", 100);
    if (initial > max_entities) {
        initial = max_entities;
    }
    return initial;
}

static int game_spawn_rate(void) {
    return parse_env_int("MICRO_IDLE_ENTITIES_PER_SEC", 1);
}

static GameConfig game_load_config(void) {
    GameConfig config = {0};
    config.max_count = game_max_entities();
    config.spawn_initial = game_initial_entities(config.max_count);
    config.spawn_per_sec = game_spawn_rate();
    config.bounds_x = parse_env_float("MICRO_IDLE_BOUNDS_X", 14.0f);
    config.bounds_z = parse_env_float("MICRO_IDLE_BOUNDS_Z", 12.0f);
    config.plane_width = parse_env_float("MICRO_IDLE_PLANE_WIDTH", 30.0f);
    config.plane_height = parse_env_float("MICRO_IDLE_PLANE_HEIGHT", 24.0f);
    return config;
}

GameState *game_create(uint64_t seed) {
    GameState *game = (GameState *)malloc(sizeof(GameState));
    if (!game) {
        return NULL;
    }
    if (!game_init(game, seed)) {
        free(game);
        return NULL;
    }
    return game;
}

void game_destroy(GameState *game) {
    if (!game) {
        return;
    }
    if (game->xpbd) {
        xpbd_destroy(game->xpbd);
    }
#ifdef ENABLE_VULKAN
    if (game->vk) {
        vk_compute_destroy(game->vk);
    }
#endif
    if (game->gpu.ready) {
        gpu_sim_shutdown(&game->gpu);
    }
    free(game);
}

bool game_init(GameState *game, uint64_t seed) {
    memset(game, 0, sizeof(*game));
    game->config = game_load_config();
    game->active_count = game->config.spawn_initial;

    // Check if XPBD mode is enabled
    const char *xpbd_env = getenv("USE_XPBD");
    game->use_xpbd = (xpbd_env && (xpbd_env[0] == '1' || xpbd_env[0] == 'y' || xpbd_env[0] == 'Y'));

    if (game->use_xpbd) {
        // XPBD soft-body mode - limited entity count for now
        int xpbd_max = 500;  // Start with fewer entities for XPBD testing
        game->xpbd = xpbd_create(xpbd_max);
        if (!game->xpbd) {
            fprintf(stderr, "game: failed to create XPBD context\n");
            return false;
        }
        fprintf(stderr, "game: using XPBD soft-body physics (max %d microbes)\n", xpbd_max);

        // Spawn initial microbes
        for (int i = 0; i < game->config.spawn_initial && i < xpbd_max; i++) {
            float x = ((float)(seed % 1000) / 500.0f - 1.0f) * game->config.bounds_x * 0.8f;
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            float z = ((float)(seed % 1000) / 500.0f - 1.0f) * game->config.bounds_z * 0.8f;
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            int type = (int)(seed % 4);
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            float s = (float)(seed % 1000) / 1000.0f;
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            xpbd_spawn_microbe(game->xpbd, x, z, type, s);
        }
        game->active_count = xpbd_get_microbe_count(game->xpbd);
    } else {
        // Legacy particle mode
#ifdef ENABLE_VULKAN
        // Try Vulkan compute first (runs on real GPU)
        game->vk = vk_compute_create(game->config.max_count);
        if (game->vk && vk_compute_ready(game->vk)) {
            game->use_vk_compute = true;
            fprintf(stderr, "game: using Vulkan compute for simulation (GPU accelerated)\n");
        } else {
            game->use_vk_compute = false;
            if (game->vk) {
                vk_compute_destroy(game->vk);
                game->vk = NULL;
            }
            fprintf(stderr, "game: Vulkan compute unavailable, using OpenGL compute\n");
        }
#else
        game->use_vk_compute = false;
        fprintf(stderr, "game: Vulkan compute disabled; using OpenGL compute\n");
#endif
    }

    // Always init OpenGL for rendering (and fallback compute)
    if (!gpu_sim_init(&game->gpu, game->config.max_count)) {
#ifdef ENABLE_VULKAN
        if (game->vk) {
            vk_compute_destroy(game->vk);
            game->vk = NULL;
        }
#endif
        if (game->xpbd) {
            xpbd_destroy(game->xpbd);
            game->xpbd = NULL;
        }
        return false;
    }
    gpu_sim_set_active_count(&game->gpu, game->active_count);
    game->screen_w = GetRenderWidth();
    game->screen_h = GetRenderHeight();
    // Calculate plane size based on camera frustum (camera at y=22, FOV=50)
    // visible_half_height = camera_height * tan(fov/2)
    const float camera_height = 22.0f;
    const float fov_rad = 50.0f * (3.14159265f / 180.0f);
    float visible_half_h = camera_height * tanf(fov_rad * 0.5f);
    float aspect = (float)game->screen_w / (float)game->screen_h;
    float plane_h = visible_half_h * 2.0f * 1.15f;  // 15% padding
    float plane_w = plane_h * aspect * 1.15f;
    game->config.plane_width = plane_w;
    game->config.plane_height = plane_h;
    game->config.bounds_x = plane_w * 0.42f;  // Keep microbes slightly inward
    game->config.bounds_z = plane_h * 0.42f;
    return true;
}

void game_handle_input(GameState *game, Camera3D camera, float dt, int screen_w, int screen_h) {
    (void)camera;
    (void)dt;
    if (!game) return;

    if (game->screen_w != screen_w || game->screen_h != screen_h) {
        game->screen_w = screen_w;
        game->screen_h = screen_h;
        // Recalculate plane size based on camera frustum
        const float camera_height = 22.0f;
        const float fov_rad = 50.0f * (3.14159265f / 180.0f);
        float visible_half_h = camera_height * tanf(fov_rad * 0.5f);
        float aspect = (float)screen_w / (float)screen_h;
        float plane_h = visible_half_h * 2.0f * 1.15f;
        float plane_w = plane_h * aspect * 1.15f;
        game->config.plane_width = plane_w;
        game->config.plane_height = plane_h;
        game->config.bounds_x = plane_w * 0.42f;
        game->config.bounds_z = plane_h * 0.42f;
    }
}

void game_update_fixed(GameState *game, float dt) {
    if (!game) {
        return;
    }
    game->sim_time += dt;

    if (game->use_xpbd && game->xpbd) {
        // XPBD soft-body physics update
        xpbd_update(game->xpbd, dt, game->config.bounds_x, game->config.bounds_z);
        game->active_count = xpbd_get_microbe_count(game->xpbd);
        return;
    }

    // Legacy mode
    if (!game->gpu.ready) {
        return;
    }

    if (game->active_count < game->config.max_count && game->config.spawn_per_sec > 0) {
        game->spawn_accum += dt;
        while (game->spawn_accum >= 1.0f && game->active_count < game->config.max_count) {
            game->spawn_accum -= 1.0f;
            game->active_count += game->config.spawn_per_sec;
            if (game->active_count > game->config.max_count) {
                game->active_count = game->config.max_count;
            }
        }
        gpu_sim_set_active_count(&game->gpu, game->active_count);
        if (game->use_vk_compute) {
#ifdef ENABLE_VULKAN
            vk_compute_set_active_count(game->vk, game->active_count);
#endif
        }
    }

#ifdef ENABLE_VULKAN
    if (game->use_vk_compute && game->vk) {
        // Run simulation on real GPU via Vulkan
        vk_compute_update(game->vk, dt, game->config.bounds_x, game->config.bounds_z, game->active_count);
        return;
    }
#endif
    gpu_sim_update(&game->gpu, dt, (Vector2){game->config.bounds_x, game->config.bounds_z});
}

void game_render(const GameState *game, Camera3D camera, float alpha) {
    (void)alpha;
    if (!game) {
        return;
    }

    BeginMode3D(camera);
    DrawPlane((Vector3){0.0f, -0.2f, 0.0f},
              (Vector2){game->config.plane_width, game->config.plane_height},
              (Color){10, 20, 30, 255});

    if (game->use_xpbd && game->xpbd) {
        // XPBD soft-body rendering
        xpbd_render(game->xpbd, camera);
    } else if (game->gpu.ready) {
        // Legacy rendering
        // Sync Vulkan compute results to OpenGL for rendering
#ifdef ENABLE_VULKAN
        if (game->use_vk_compute && game->vk) {
            static VkSimData *sync_buffer = NULL;
            static int sync_buffer_size = 0;
            if (sync_buffer_size < game->active_count) {
                free(sync_buffer);
                sync_buffer = malloc(sizeof(VkSimData) * (size_t)game->config.max_count);
                sync_buffer_size = game->config.max_count;
            }
            if (sync_buffer) {
                vk_compute_read_entities((VkComputeContext*)game->vk, sync_buffer, game->active_count);
                gpu_sim_upload_entities((GpuSim*)&game->gpu, sync_buffer, game->active_count);
            }
        }
#endif
        gpu_sim_render(&game->gpu, camera);
    }

    EndMode3D();
}

void game_render_ui(GameState *game, int screen_w, int screen_h) {
    (void)game;
    DrawRectangle(12, 12, 280, 80, (Color){10, 10, 20, 200});

    if (game && game->use_xpbd) {
        DrawText("Micro-Idle (XPBD)", 20, 20, 16, RAYWHITE);
        DrawText(TextFormat("Microbes: %d", xpbd_get_microbe_count(game->xpbd)), 20, 42, 12, GRAY);
    } else {
        DrawText("Micro-Idle (GPU)", 20, 20, 16, RAYWHITE);
        DrawText(TextFormat("Entities: %d", game && game->gpu.ready ? game->gpu.active_count : GAME_GPU_ENTITY_COUNT),
                 20, 42, 12, GRAY);
    }
    DrawText(TextFormat("Screen: %dx%d", screen_w, screen_h), 20, 60, 12, GRAY);
}
