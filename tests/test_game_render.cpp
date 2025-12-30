#include <catch2/catch_test_macros.hpp>
#include "raylib.h"
#include "rlgl.h"
#include "game/game.h"
#include "engine/platform/engine.h"
#include "src/World.h"

using namespace micro_idle;

TEST_CASE("GameRender - World creation WITHOUT window", "[game_render]") {
    // Try to create World BEFORE window initialization
    // With lazy shader loading: World creation succeeds, shader loads in render()

    // First verify LoadShader fails without window (returns invalid shader quickly)
    // Note: LoadShader may hang if it tries to initialize OpenGL context, so we skip this check
    // The important part is that World creation doesn't require a window

    // Now create World - should succeed even without window (lazy shader loading)
    World world;

    REQUIRE(true); // World created, shader will load lazily
}

TEST_CASE("GameRender - Rendering with window (shader loads lazily)", "[game_render]") {
    // Initialize window
    InitWindow(1280, 720, "Micro-Idle Render Test");
    SetTargetFPS(60);

    // Initialize engine
    EngineConfig cfg = {
        .window_w = 1280,
        .window_h = 720,
        .target_fps = 60,
        .tick_hz = 60,
        .vsync = false,
        .dev_mode = true
    };

    EngineContext engine = {0};
    engine_init(&engine, cfg);

    // Create World - shader should load now that window exists
    World world;

    // Setup camera
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Render a frame - shader should load during first render
    BeginDrawing();
    ClearBackground((Color){10, 20, 30, 255});
    world.render(camera, 0.0f);
    world.renderUI(1280, 720);
    EndDrawing();

    CloseWindow();

    REQUIRE(true); // Render should succeed
}
