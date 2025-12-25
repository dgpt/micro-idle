#include "game/game.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "game/gpu_sim.h"

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
    GameConfig config;
    float sim_time;
    float spawn_accum;
    int active_count;
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
    if (game->gpu.ready) {
        gpu_sim_shutdown(&game->gpu);
    }
    free(game);
}

bool game_init(GameState *game, uint64_t seed) {
    (void)seed;
    memset(game, 0, sizeof(*game));
    game->config = game_load_config();
    game->active_count = game->config.spawn_initial;
    if (!gpu_sim_init(&game->gpu, game->config.max_count)) {
        return false;
    }
    gpu_sim_set_active_count(&game->gpu, game->active_count);
    return true;
}

void game_handle_input(GameState *game, Camera3D camera, float dt, int screen_w, int screen_h) {
    (void)game;
    (void)camera;
    (void)dt;
    (void)screen_w;
    (void)screen_h;
}

void game_update_fixed(GameState *game, float dt) {
    if (!game || !game->gpu.ready) {
        return;
    }
    game->sim_time += dt;
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
    }
    gpu_sim_update(&game->gpu, dt, (Vector2){game->config.bounds_x, game->config.bounds_z});
}

void game_render(const GameState *game, Camera3D camera, float alpha) {
    (void)alpha;
    if (!game || !game->gpu.ready) {
        return;
    }
    BeginMode3D(camera);
    DrawPlane((Vector3){0.0f, -0.2f, 0.0f},
              (Vector2){game->config.plane_width, game->config.plane_height},
              (Color){10, 20, 30, 255});
    gpu_sim_render(&game->gpu, camera);
    EndMode3D();
}

void game_render_ui(GameState *game, int screen_w, int screen_h) {
    (void)game;
    DrawRectangle(12, 12, 280, 80, (Color){10, 10, 20, 200});
    DrawText("Micro-Idle (GPU)", 20, 20, 16, RAYWHITE);
    DrawText(TextFormat("Entities: %d", game && game->gpu.ready ? game->gpu.active_count : GAME_GPU_ENTITY_COUNT),
             20, 42, 12, GRAY);
    DrawText(TextFormat("Screen: %dx%d", screen_w, screen_h), 20, 60, 12, GRAY);
}
