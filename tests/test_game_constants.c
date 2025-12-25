#include "game/game.h"

#include <stdio.h>

int test_game_constants_run(void) {
    if (GAME_GPU_ENTITY_COUNT < 1000000) {
        printf("game GAME_GPU_ENTITY_COUNT too low: %d\n", GAME_GPU_ENTITY_COUNT);
        return 1;
    }
    if (GAME_GPU_ENTITY_COUNT < 100) {
        printf("game GAME_GPU_ENTITY_COUNT below initial spawn default\n");
        return 1;
    }
    return 0;
}
