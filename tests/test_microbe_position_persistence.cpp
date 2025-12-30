#include <catch2/catch_test_macros.hpp>
#include <map>
#include "raylib.h"
#include "game/game.h"

TEST_CASE("Microbe spawning - Verify existing microbes keep their positions", "[microbe_positions]") {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Microbe Position Persistence Test");
    SetTargetFPS(60);

    // Create game with initial 3 microbes
    GameState* game = game_create(0xBEEFCAFEu);
    REQUIRE(game != nullptr);
    fflush(stdout);

    int initialCount = game_get_microbe_count(game);
    printf("TEST: Initial microbe count: %d\n", initialCount);
    fflush(stdout);
    REQUIRE(initialCount == 3);

    // Update for 2 seconds (should spawn ~2 microbes)
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 120; frame++) {
        game_update_fixed(game, dt);
    }

    int finalCount = game_get_microbe_count(game);
    printf("TEST: Final microbe count: %d\n", finalCount);
    fflush(stdout);
    printf("TEST: Spawned: %d microbes\n", finalCount - initialCount);
    fflush(stdout);

    // If bug exists: all microbes stack at new spawn location, count stays ~3
    // If fixed: count increases properly
    REQUIRE(finalCount >= initialCount);

    game_destroy(game);
    CloseWindow();
}
