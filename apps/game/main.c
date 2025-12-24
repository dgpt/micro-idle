#include "raylib.h"
#include "raymath.h"

#include "engine/engine.h"
#include "game/game.h"

int main(void) {
    EngineConfig cfg = {
        .window_w = 1280,
        .window_h = 720,
        .target_fps = 60,
        .tick_hz = 60,
        .vsync = true,
        .dev_mode = true
    };

    InitWindow(cfg.window_w, cfg.window_h, "Micro-Idle");
    if (cfg.vsync) {
        SetWindowState(FLAG_VSYNC_HINT);
    }
    SetTargetFPS(cfg.target_fps);

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
        CloseWindow();
        return 1;
    }

    while (!WindowShouldClose()) {
        float real_dt = GetFrameTime();
        (void)real_dt;

        int steps = engine_time_update(&engine, real_dt);
        game_handle_input(game, camera, real_dt, GetScreenWidth(), GetScreenHeight());
        for (int i = 0; i < steps; ++i) {
            game_update_fixed(game, (float)engine.time.tick_dt);
        }

        BeginDrawing();
        ClearBackground((Color){8, 12, 18, 255});
        game_render(game, camera, engine_time_alpha(&engine));
        game_render_ui(game, GetScreenWidth(), GetScreenHeight());
        EndDrawing();
    }

    game_destroy(game);
    CloseWindow();
    return 0;
}
