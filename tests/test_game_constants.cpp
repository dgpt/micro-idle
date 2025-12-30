#include "game/game.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Game constants - GPU entity count", "[game_constants]") {
    REQUIRE(GAME_GPU_ENTITY_COUNT >= 1000000);
    REQUIRE(GAME_GPU_ENTITY_COUNT >= 100);
}
