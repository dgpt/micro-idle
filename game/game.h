#ifndef MICRO_IDLE_GAME_H
#define MICRO_IDLE_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

#define GAME_GPU_ENTITY_COUNT 1000000

typedef struct GameState GameState;

GameState *game_create(uint64_t seed);
void game_destroy(GameState *game);
bool game_init(GameState *game, uint64_t seed);
void game_handle_input(GameState *game, Camera3D camera, float dt, int screen_w, int screen_h);
void game_update_fixed(GameState *game, float dt);
void game_render(const GameState *game, Camera3D camera, float alpha);
void game_render_ui(GameState *game, int screen_w, int screen_h);

// Test helpers
int game_get_particle_count(const GameState *game);
int game_get_microbe_count(const GameState *game);
float game_get_microbe_volume(const GameState *game, int index);
float game_get_microbe_radius(const GameState *game, int index);
void game_get_microbe_position(const GameState *game, int index, float* x, float* y, float* z);

#endif
