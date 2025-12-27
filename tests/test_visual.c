// Visual test - runs game and captures screenshot for verification
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "game/game.h"
#include "engine/platform/engine.h"
#include "tests/test_env.h"

#include <stdio.h>
#include <stdlib.h>

int test_visual_run(void) {
#if defined(__linux__)
    test_set_env("MESA_LOADER_DRIVER_OVERRIDE", "zink");
#endif

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "visual_test");
    if (!IsWindowReady()) {
        printf("visual test window failed to init\n");
        return 1;
    }

    EngineConfig cfg = {
        .window_w = 1280,
        .window_h = 720,
        .target_fps = 60,
        .tick_hz = 60,
        .vsync = false,
        .dev_mode = false
    };

    EngineContext engine = {0};
    engine_init(&engine, cfg);

    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    GameState *game = game_create(0xC0FFEEu);
    if (!game) {
        printf("visual test game create failed\n");
        CloseWindow();
        return 1;
    }

    // Run for 120 frames to let physics settle
    for (int frame = 0; frame < 120; frame++) {
        int screen_w = GetRenderWidth();
        int screen_h = GetRenderHeight();
        int steps = engine_time_update(&engine, 1.0f / 60.0f);
        game_handle_input(game, camera, 1.0f / 60.0f, screen_w, screen_h);
        for (int i = 0; i < steps; ++i) {
            game_update_fixed(game, (float)engine.time.tick_dt);
        }

        BeginDrawing();
        rlViewport(0, 0, screen_w, screen_h);
        ClearBackground((Color){10, 20, 30, 255});
        game_render(game, camera, engine_time_alpha(&engine));
        game_render_ui(game, screen_w, screen_h);
        EndDrawing();
    }

    // Capture screenshot
    TakeScreenshot("test_output.png");

    game_destroy(game);
    CloseWindow();
    return 0;
}
