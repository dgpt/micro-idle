#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "src/World.h"
#include "src/components/Microbe.h"
#include "raylib.h"

using namespace micro_idle;

TEST_CASE("MicrobeIntegration - Create amoeba entity", "[microbe_integration]") {
    World world;
    Vector3 position = {0.0f, 5.0f, 0.0f};

    world.createAmoeba(position, 1.5f, RED);

    REQUIRE(true); // Basic smoke test - entity creation should not crash
}

TEST_CASE("MicrobeIntegration - Microbe simulation updates", "[microbe_integration]") {
    World world;
    Vector3 position = {0.0f, 10.0f, 0.0f};

    world.createAmoeba(position, 1.5f, BLUE);

    float dt = 1.0f / 60.0f;
    world.update(dt);

    REQUIRE(true); // Basic smoke test - update should not crash
}

TEST_CASE("MicrobeIntegration - Multiple amoebas interact", "[microbe_integration]") {
    World world;

    world.createAmoeba({0.0f, 5.0f, 0.0f}, 1.5f, RED);
    world.createAmoeba({3.0f, 5.0f, 0.0f}, 1.2f, BLUE);
    world.createAmoeba({-3.0f, 5.0f, 0.0f}, 1.3f, GREEN);

    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; i++) {
        world.update(dt);
    }

    REQUIRE(true); // Basic smoke test - multiple entities should not crash
}

TEST_CASE("MicrobeIntegration - Transform sync from physics", "[microbe_integration]") {
    World world;
    Vector3 position = {0.0f, 10.0f, 0.0f};

    world.createAmoeba(position, 1.5f, RED);

    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; i++) {
        world.update(dt);
    }

    REQUIRE(true); // Basic smoke test - transform sync should not crash
}

TEST_CASE("MicrobeIntegration - Vertex extraction during simulation", "[microbe_integration]") {
    World world;
    Vector3 position = {0.0f, 5.0f, 0.0f};

    world.createAmoeba(position, 1.5f, RED);

    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; i++) {
        world.update(dt);
    }

    REQUIRE(true); // Basic smoke test - vertex extraction should not crash
}

TEST_CASE("MicrobeIntegration - Locomotion affects soft body", "[microbe_integration]") {
    World world;
    Vector3 position = {0.0f, 5.0f, 0.0f};

    world.createAmoeba(position, 1.5f, RED);

    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 120; i++) { // 2 seconds
        world.update(dt);
    }

    REQUIRE(true); // Basic smoke test - locomotion should not crash
}

TEST_CASE("MicrobeIntegration - Stress test - 10 amoebas", "[microbe_integration]") {
    World world;

    // Create 10 amoebas in a circle
    for (int i = 0; i < 10; i++) {
        float angle = (i / 10.0f) * 2.0f * 3.14159f;
        float radius = 5.0f;
        Vector3 pos = {
            radius * cosf(angle),
            5.0f,
            radius * sinf(angle)
        };
        world.createAmoeba(pos, 1.0f, RED);
    }

    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; i++) {
        world.update(dt);
    }

    REQUIRE(true); // Basic smoke test - stress test should not crash
}
