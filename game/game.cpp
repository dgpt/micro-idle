// Micro-Idle - Main Game Logic
// FLECS + Jolt integration (Phase 2)

#include "game/game.h"
#include "src/World.h"
#include "src/components/Transform.h"
#include "src/components/WorldState.h"
#include <stdio.h>
#include <stdint.h>
#include "raymath.h"

struct GameState {
    micro_idle::World* world;
    uint64_t seed;
};

// Calculate world dimensions from camera view frustum
// Accounts for 32px margin from viewport edges
static void calculateWorldDimensions(Camera3D camera, int screen_w, int screen_h, float* outWidth, float* outHeight) {
    // Camera is at (0, 22, 0) looking down at XZ plane
    // Use similar triangles to calculate visible area at y=0
    float cameraHeight = camera.position.y;
    float fovRadians = camera.fovy * DEG2RAD;
    float aspect = (float)screen_w / (float)screen_h;

    // Calculate visible height (Z dimension) at ground level
    float visibleHeight = 2.0f * cameraHeight * tanf(fovRadians / 2.0f);

    // Calculate visible width (X dimension) using aspect ratio
    float visibleWidth = visibleHeight * aspect;

    // Convert 32px margin to world space units
    // At camera height, 1 pixel = visibleHeight / screen_h world units
    float pixelToWorld = visibleHeight / (float)screen_h;
    float marginWorld = 32.0f * pixelToWorld;

    // Subtract margin from both dimensions (32px on each side = 64px total)
    *outWidth = visibleWidth - (marginWorld * 2.0f);
    *outHeight = visibleHeight - (marginWorld * 2.0f);
}

GameState* game_create(uint64_t seed) {
    printf("game: Initializing with FLECS+Jolt, seed=%lu\n", seed);
    fflush(stdout);
    srand((unsigned int)seed);
    GameState* state = new GameState();
    state->seed = seed;
    state->world = new micro_idle::World();

    // Calculate initial world dimensions based on camera view
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.fovy = 50.0f;

    float worldWidth, worldHeight;
    calculateWorldDimensions(camera, 1280, 720, &worldWidth, &worldHeight);

    printf("game: Initial world dimensions: %.1f x %.1f\n", worldWidth, worldHeight);

    // Create screen-space boundaries (rectangular container)
    state->world->createScreenBoundaries(worldWidth, worldHeight);

    // Update world state singleton with initial screen dimensions
    auto worldState = state->world->getWorld().get_mut<components::WorldState>();
    if (worldState) {
        worldState->screenWidth = 1280;
        worldState->screenHeight = 720;
    }

    // Create amoebas inside boundaries with EC&M locomotion
    state->world->createAmoeba({0.0f, 1.5f, 0.0f}, 0.375f, RED);
    state->world->createAmoeba({5.0f, 1.5f, 0.0f}, 0.3f, GREEN);
    state->world->createAmoeba({-5.0f, 1.5f, 3.0f}, 0.325f, BLUE);

    printf("game: Ready (3 amoebas with screen boundaries and EC&M locomotion)\n");
    fflush(stdout);
    return state;
}

void game_destroy(GameState* state) {
    delete state->world;
    delete state;
}

bool game_init(GameState* game, uint64_t seed) {
    // Initialization already done in game_create
    return true;
}

void game_handle_input(GameState* game, Camera3D camera, float dt, int screen_w, int screen_h) {
    game->world->handleInput(camera, dt, screen_w, screen_h);
}

void game_handle_resize(GameState* game, int screen_w, int screen_h, Camera3D camera) {
    // Recalculate world dimensions (accounts for 32px margin)
    float worldWidth, worldHeight;
    calculateWorldDimensions(camera, screen_w, screen_h, &worldWidth, &worldHeight);

    printf("game: Window resized (%dx%d) - updating world dimensions to %.1f x %.1f\n",
           screen_w, screen_h, worldWidth, worldHeight);

    // Update boundaries and reposition out-of-bounds amoebas
    game->world->updateScreenBoundaries(worldWidth, worldHeight);

    // Update world state singleton with screen dimensions
    auto worldState = game->world->getWorld().get_mut<components::WorldState>();
    if (worldState) {
        worldState->screenWidth = screen_w;
        worldState->screenHeight = screen_h;
    }
}

void game_update_fixed(GameState* game, float dt) {
    game->world->update(dt);
}

void game_render(const GameState* game, Camera3D camera, float alpha) {
    game->world->render(camera, alpha);
}

void game_render_ui(GameState* game, int screen_w, int screen_h) {
    game->world->renderUI(screen_w, screen_h);
}

// Test helpers
int game_get_particle_count(const GameState* game) {
    return 0; // TODO: implement with Jolt physics
}

int game_get_microbe_count(const GameState* game) {
    // Count entities with Transform component
    return game->world->getWorld().count<components::Transform>();
}

float game_get_microbe_volume(const GameState* game, int index) {
    return 0.0f; // TODO: implement
}

float game_get_microbe_radius(const GameState* game, int index) {
    return 0.0f; // TODO: implement
}

void game_get_microbe_position(const GameState* game, int index, float* x, float* y, float* z) {
    *x = 0.0f;
    *y = 0.0f;
    *z = 0.0f;
    // TODO: implement
}
