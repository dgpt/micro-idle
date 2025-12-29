// Micro-Idle - Main Game Logic
// FLECS + Jolt integration (Phase 2)

#include "game/game.h"
#include "src/World.h"
#include "src/components/Transform.h"
#include <stdio.h>
#include <stdint.h>
#include "raymath.h"

struct GameState {
    micro_idle::World* world;
    uint64_t seed;
};

GameState* game_create(uint64_t seed) {
    printf("game: Initializing with FLECS+Jolt, seed=%lu\n", seed);
    fflush(stdout);
    srand((unsigned int)seed);
    GameState* state = new GameState();
    state->seed = seed;
    state->world = new micro_idle::World();

    // Create petri dish (flat disc with raised edges)
    state->world->createPetriDish({0.0f, 0.0f, 0.0f}, 15.0f, 1.0f);

    // Create amoebas on petri dish surface with EC&M locomotion
    state->world->createAmoeba({0.0f, 2.0f, 0.0f}, 1.5f, RED);
    state->world->createAmoeba({5.0f, 2.0f, 0.0f}, 1.2f, GREEN);
    state->world->createAmoeba({-5.0f, 2.0f, 5.0f}, 1.3f, BLUE);

    printf("game: Ready (3 amoebas on petri dish with EC&M locomotion)\n");
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
