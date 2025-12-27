#include "game/game.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "game/xpbd.h"

typedef struct GameConfig {
    int max_count;
    int spawn_initial;
    float bounds_x;
    float bounds_z;
    float plane_width;
    float plane_height;
} GameConfig;

typedef struct GameState {
    XpbdContext *xpbd;
    GameConfig config;
    int screen_w;
    int screen_h;
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

static GameConfig game_load_config(void) {
    GameConfig config = {0};
    config.max_count = parse_env_int("MICRO_IDLE_ENTITY_COUNT", 500);
    config.spawn_initial = parse_env_int("MICRO_IDLE_INITIAL_ENTITIES", 100);
    if (config.spawn_initial > config.max_count) {
        config.spawn_initial = config.max_count;
    }
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
    free(game);
}

bool game_init(GameState *game, uint64_t seed) {
    memset(game, 0, sizeof(*game));
    game->config = game_load_config();

    game->screen_w = GetRenderWidth();
    game->screen_h = GetRenderHeight();

    // Calculate plane size based on camera frustum (camera at y=22, FOV=50)
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

    // Create XPBD physics system
    game->xpbd = xpbd_create(game->config.max_count);
    if (!game->xpbd) {
        fprintf(stderr, "game: failed to create XPBD context\n");
        return false;
    }

    // Spawn initial microbes
    for (int i = 0; i < game->config.spawn_initial; i++) {
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
    if (!game || !game->xpbd) {
        return;
    }
    xpbd_update(game->xpbd, dt, game->config.bounds_x, game->config.bounds_z);
}

void game_render(const GameState *game, Camera3D camera, float alpha) {
    (void)alpha;
    if (!game || !game->xpbd) {
        return;
    }

    BeginMode3D(camera);
    DrawPlane((Vector3){0.0f, -0.2f, 0.0f},
              (Vector2){game->config.plane_width, game->config.plane_height},
              (Color){10, 20, 30, 255});

    xpbd_render(game->xpbd, camera);

    EndMode3D();
}

void game_render_ui(GameState *game, int screen_w, int screen_h) {
    if (!game || !game->xpbd) {
        return;
    }

    DrawRectangle(12, 12, 280, 80, (Color){10, 10, 20, 200});
    DrawText("Micro-Idle", 20, 20, 16, RAYWHITE);
    DrawText(TextFormat("Microbes: %d", xpbd_get_microbe_count(game->xpbd)), 20, 42, 12, GRAY);
    DrawText(TextFormat("Screen: %dx%d", screen_w, screen_h), 20, 60, 12, GRAY);
}
