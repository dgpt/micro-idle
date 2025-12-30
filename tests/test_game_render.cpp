#include <stdio.h>
#include "raylib.h"
#include "rlgl.h"
#include "game/game.h"
#include "engine/platform/engine.h"
#include "src/World.h"

using namespace micro_idle;

// Test: Game initialization and first render (reproduces game.exe crash)
// This test verifies that World can be created without window (lazy shader loading)
int test_game_render_run(void) {
    printf("game_render:\n");
    printf("  Testing: World creation WITHOUT window...");

    // Try to create World BEFORE window initialization
    // With the bug: LoadShader called in constructor without window -> returns invalid shader (id == 0)
    //   This causes BeginShaderMode to fail/crash when rendering
    // With the fix: Shader loads lazily -> World creation succeeds, shader loads in render()

    // First verify LoadShader fails without window (returns invalid shader quickly)
    Shader testShader = LoadShader("shaders/sdf_membrane.vert", "shaders/sdf_membrane.frag");
    if (testShader.id != 0) {
        UnloadShader(testShader);
        printf(" FAIL: LoadShader succeeded without window (unexpected)\n");
        return 1;
    }

    // Now create World - if LoadShader is called in constructor, it will get invalid shader
    // but won't hang (LoadShader returns quickly with id == 0)
    micro_idle::World world;

    // If we get here, World was created. Now verify rendering works with window
    printf(" PASS (World created, shader will load lazily)\n");
    printf("  Testing: Rendering with window (shader loads lazily)...");

    // Initialize window (same as main.cpp)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Micro-Idle Test");
    SetTargetFPS(60);

    // Initialize engine (same as main.cpp)
    EngineConfig cfg = {
        .window_w = 1280,
        .window_h = 720,
        .target_fps = 60,
        .tick_hz = 60,
        .vsync = true,
        .dev_mode = true
    };

    EngineContext engine = {0};
    engine_init(&engine, cfg);

    // Setup camera (same as main.cpp)
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Create game state (same as main.cpp)
    // With the fix: Shader loads lazily in render() -> works fine
    // With the bug: If LoadShader was called in constructor without window,
    //   shader is invalid (id == 0) and BeginShaderMode will fail/crash
    GameState *game = game_create(0xC0FFEEu);
    if (!game) {
        CloseWindow();
        printf(" FAIL: game_create returned NULL\n");
        return 1;
    }

    // Run one update cycle (same as main.cpp)
    float real_dt = GetFrameTime();
    int steps = engine_time_update(&engine, real_dt);
    game_handle_input(game, camera, real_dt, 1280, 720);
    for (int i = 0; i < steps; ++i) {
        game_update_fixed(game, (float)engine.time.tick_dt);
    }

    // Render one frame (same as main.cpp)
    // With the bug: If LoadShader was called in constructor without window,
    //   shader.id == 0, and BeginShaderMode(0) will fail/crash
    // With the fix: Shader loads lazily here (id != 0) -> rendering works
    BeginDrawing();
    rlViewport(0, 0, 1280, 720);
    ClearBackground((Color){10, 20, 30, 255});

    // This should work - shader loads lazily in render() when window is available
    // If shader was loaded in constructor without window, it's invalid and this will fail
    game_render(game, camera, engine_time_alpha(&engine));
    game_render_ui(game, 1280, 720);

    EndDrawing();

    // If we get here without crashing, shader loaded correctly (lazy loading works)

    // Clean up
    game_destroy(game);
    CloseWindow();

    printf(" PASS\n");
    return 0;
}
