#include <catch2/catch_test_macros.hpp>
#include "raylib.h"
#include "src/World.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"

TEST_CASE("SpawnSystem - Microbes spawn at different positions", "[spawn_positions]") {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Spawn Position Test");
    SetTargetFPS(60);

    micro_idle::World world;

    // Update for enough time to spawn many microbes
    float dt = 1.0f / 60.0f;
    int steps = 300;  // 5 seconds at 60fps

    for (int i = 0; i < steps; i++) {
        world.update(dt);
    }

    // Get all microbes and their positions
    std::vector<Vector3> positions;
    world.getWorld().each([&](flecs::entity e, const components::Transform& transform) {
        positions.push_back(transform.position);
    });

    printf("Total entities with Transform: %zu\n", positions.size());

    // Get just microbes
    std::vector<Vector3> microbePositions;
    world.getWorld().each([&](flecs::entity e, const components::Microbe& microbe, const components::Transform& transform) {
        microbePositions.push_back(transform.position);
    });

    printf("Total microbes spawned: %zu\n", microbePositions.size());
    REQUIRE(microbePositions.size() > 3);  // Should have more than initial 3

    // Check if all microbes are at the SAME position (the bug)
    Vector3 firstPos = microbePositions[0];
    int samePositionCount = 0;

    for (size_t i = 0; i < microbePositions.size(); i++) {
        float dx = microbePositions[i].x - firstPos.x;
        float dz = microbePositions[i].z - firstPos.z;
        float distXZ = sqrtf(dx*dx + dz*dz);

        printf("Microbe %zu: (%.2f, %.2f, %.2f) - XZ distance from first: %.2f\n",
               i, microbePositions[i].x, microbePositions[i].y, microbePositions[i].z, distXZ);

        if (distXZ < 0.1f) {
            samePositionCount++;
        }
    }

    printf("Microbes at same XZ position as first: %d/%zu\n", samePositionCount, microbePositions.size());

    // If all microbes are stacking, this will fail
    if (samePositionCount == (int)microbePositions.size()) {
        printf("FAILURE: All microbes spawned at identical position!\n");
        FAIL("All microbes stacked at same position - spawn randomization broken");
    }

    CloseWindow();
}
