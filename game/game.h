#ifndef MICRO_IDLE_GAME_H
#define MICRO_IDLE_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

#define MAX_MICROBES 12000
#define MAX_ZONES 32

typedef struct GameState GameState;

GameState *game_create(uint64_t seed);
void game_destroy(GameState *game);
void game_init(GameState *game, uint64_t seed);
void game_handle_input(GameState *game, Camera3D camera, float dt, int screen_w, int screen_h);
void game_update_fixed(GameState *game, float dt);
void game_render(const GameState *game, Camera3D camera, float alpha);
void game_render_ui(GameState *game, int screen_w, int screen_h);

#endif
