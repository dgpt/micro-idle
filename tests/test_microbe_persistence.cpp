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

    // Create game (should create 3 initial amoebas)
    GameState* game = game_create(0xC0FFEEu);
    REQUIRE(game != nullptr);

    // Check microbe count immediately after creation
    int initialCount = game_get_microbe_count(game);
    printf("Initial microbe count: %d (should be 3)\n", initialCount);
    REQUIRE(initialCount == 3);

    // Update for 3 seconds with SpawnSystem active
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 180; frame++) {  // 3 seconds at 60fps
        game_update_fixed(game, dt);

        if (frame % 60 == 0) {
            int count = game_get_microbe_count(game);
            printf("Frame %d (%.1f sec): %d microbes\n", frame, frame * dt, count);
        }
    }

    int finalCount = game_get_microbe_count(game);
    printf("Final microbe count: %d (should be >= 6: 3 initial + 3 spawned)\n", finalCount);

    // Verify initial microbes are NOT destroyed
    // At spawn rate 1/sec for 3 seconds, we should have ~3-6 microbes total
    REQUIRE(finalCount >= 5);  // At least initial 3 + 2 spawned (accounting for timing)

    game_destroy(game);
    CloseWindow();
}
