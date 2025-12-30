#include <catch2/catch_test_macros.hpp>
#include "raylib.h"
#include "src/World.h"
#include "src/components/Microbe.h"
#include "src/components/Rendering.h"
#include "engine/platform/engine.h"

using namespace micro_idle;

TEST_CASE("Rendering Debug - Check microbe rendering setup", "[rendering][debug]") {
    // Initialize window in hidden mode
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Rendering Debug Test");
    SetTargetFPS(60);

    World world;

    // Create a test amoeba
    auto amoeba = world.createAmoeba((Vector3){0.0f, 5.0f, 0.0f}, 2.0f, RED);

    // Check if microbe has SDFRenderComponent
    auto sdf = amoeba.get<components::SDFRenderComponent>();
    REQUIRE(sdf != nullptr);

    // Update a few times to let systems run
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; i++) {
        world.update(dt);
    }

    // Check shader loading
    auto sdfAfter = amoeba.get<components::SDFRenderComponent>();
    printf("After update: shader.id=%d, vertexCount=%d, shaderLoaded=%d\n",
           sdfAfter->shader.id, sdfAfter->vertexCount, sdfAfter->shaderLoaded);

    // Setup camera
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Try to render
    BeginDrawing();
    ClearBackground(BLACK);
    BeginMode3D(camera);

    world.render(camera, 0.0f);

    EndMode3D();
    EndDrawing();

    // Check final state
    auto sdfFinal = amoeba.get<components::SDFRenderComponent>();
    printf("After render: shader.id=%d, vertexCount=%d, shaderLoaded=%d\n",
           sdfFinal->shader.id, sdfFinal->vertexCount, sdfFinal->shaderLoaded);

    // Basic checks
    REQUIRE(sdfFinal->shader.id != 0);  // Shader should be loaded
    REQUIRE(sdfFinal->vertexCount > 0);  // Should have vertices

    CloseWindow();
}
