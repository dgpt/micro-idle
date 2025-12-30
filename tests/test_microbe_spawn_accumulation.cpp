#include <catch2/catch_test_macros.hpp>
#include "raylib.h"
#include "src/World.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"
#include "engine/platform/engine.h"

using namespace micro_idle;

TEST_CASE("MicrobeSpawn - Microbes accumulate without being destroyed", "[microbe_spawn]") {
    // Initialize window in hidden mode
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Microbe Spawn Test");
    SetTargetFPS(60);

    World world;

    // Get initial microbe count
    int initialCount = world.getWorld().count<components::Microbe>();
    printf("Initial microbe count: %d\n", initialCount);

    // Update for several seconds with SpawnSystem active (spawn rate = 1 microbe/sec)
    float dt = 1.0f / 60.0f;
    int totalFrames = 60 * 3;  // 3 seconds
    int maxMicrobes = 0;

    for (int frame = 0; frame < totalFrames; frame++) {
        world.update(dt);

        int currentCount = world.getWorld().count<components::Microbe>();
        if (currentCount > maxMicrobes) {
            maxMicrobes = currentCount;
        }

        // Log every 60 frames (1 second)
        if (frame % 60 == 0) {
            printf("Frame %d (%.1f sec): %d microbes\n", frame, frame * dt, currentCount);
        }
    }

    int finalCount = world.getWorld().count<components::Microbe>();
    printf("Final microbe count: %d, Max: %d\n", finalCount, maxMicrobes);

    // Verify microbes are accumulating, not disappearing and respawning
    // At spawn rate 1/sec for 3 seconds, we should have ~3 microbes (initial 3 + 3 spawned)
    REQUIRE(finalCount >= initialCount);  // Microbes should not disappear
    REQUIRE(finalCount > initialCount);   // At least some should be spawned
    REQUIRE(maxMicrobes >= finalCount);   // Peak should be higher than or equal to final

    CloseWindow();
}

TEST_CASE("MicrobeSpawn - Entity count increases correctly", "[microbe_spawn]") {
    // Initialize window in hidden mode
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Entity Count Test");
    SetTargetFPS(60);

    World world;

    // Count initial entities (all types, not just microbes)
    int initialEntities = world.getWorld().count<components::Transform>();
    printf("Initial entity count (all Transform): %d\n", initialEntities);

    // Update for 1 second
    float dt = 1.0f / 60.0f;
    int totalFrames = 60;  // 1 second at 60fps

    for (int frame = 0; frame < totalFrames; frame++) {
        world.update(dt);
    }

    int finalEntities = world.getWorld().count<components::Transform>();
    printf("Final entity count (all Transform): %d\n", finalEntities);

    // After 1 second, with SpawnSystem at 1 microbe/sec, we should have added at least 1
    REQUIRE(finalEntities > initialEntities);

    CloseWindow();
}

TEST_CASE("MicrobeSpawn - Check for physics body accumulation", "[microbe_spawn]") {
    // This test verifies that physics bodies are being created and not destroyed
    // If old microbes are being destroyed, we'd see the physics body count not increasing

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Physics Body Test");
    SetTargetFPS(60);

    World world;

    // Update for 2 seconds
    float dt = 1.0f / 60.0f;
    int totalFrames = 120;  // 2 seconds

    for (int frame = 0; frame < totalFrames; frame++) {
        world.update(dt);
    }

    // Count microbes
    int microbeCount = world.getWorld().count<components::Microbe>();
    printf("Microbe count after 2 seconds: %d\n", microbeCount);

    // We started with 3 + should have spawned ~2 more = ~5 microbes
    // If old ones are being destroyed, count would stay at 3 or less
    REQUIRE(microbeCount >= 4);  // At least the original 3 plus 1 spawned

    CloseWindow();
}
