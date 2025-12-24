#include "game/game.h"

#include <stdio.h>

int test_game_constants_run(void) {
    if (MAX_MICROBES < 10000) {
        printf("game MAX_MICROBES too low: %d\n", MAX_MICROBES);
        return 1;
    }
    return 0;
}
