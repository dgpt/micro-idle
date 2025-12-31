#include <catch2/catch_test_macros.hpp>
#include "raylib.h"
#include "game/game.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"

TEST_CASE("Game - Initial microbes persist during spawn", "[game_microbe]") {
    // Initialize window in hidden mode
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Game Microbe Test");
    SetTargetFPS(60);

    // Create game (should create 2 initial amoebas)
    GameState* game = game_create(0xC0FFEEu);
    REQUIRE(game != nullptr);

    // Check microbe count immediately after creation
    int initialCount = game_get_microbe_count(game);
    printf("Initial microbe count: %d (should be 2)\n", initialCount);
    REQUIRE(initialCount == 2);

    // Update for 3 seconds (spawn disabled in game mode)
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 180; frame++) {  // 3 seconds at 60fps
        game_update_fixed(game, dt);

        if (frame % 60 == 0) {
            int count = game_get_microbe_count(game);
            printf("Frame %d (%.1f sec): %d microbes\n", frame, frame * dt, count);
        }
    }

    int finalCount = game_get_microbe_count(game);
    printf("Final microbe count: %d (should remain 2)\n", finalCount);

    // Verify initial microbes are NOT destroyed
    REQUIRE(finalCount == initialCount);

    game_destroy(game);
    CloseWindow();
}
