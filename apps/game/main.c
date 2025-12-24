#include "raylib.h"
#include "raymath.h"

#include "engine/engine.h"
#include "game/game.h"

static void update_camera(Camera3D *camera, float dt) {
    const float move = 8.0f * dt;
    Vector3 delta = {0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_A)) delta.x -= move;
    if (IsKeyDown(KEY_D)) delta.x += move;
    if (IsKeyDown(KEY_W)) delta.z -= move;
    if (IsKeyDown(KEY_S)) delta.z += move;
    camera->target = Vector3Add(camera->target, delta);
    camera->position = Vector3Add(camera->position, delta);

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        Vector3 dir = Vector3Subtract(camera->position, camera->target);
        float len = Vector3Length(dir);
        float next = Clamp(len - wheel * 1.5f, 6.0f, 30.0f);
        if (len > 0.001f) {
            dir = Vector3Scale(dir, next / len);
            camera->position = Vector3Add(camera->target, dir);
        }
    }
}

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
    camera.position = (Vector3){0.0f, 16.0f, 14.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    GameState *game = game_create(0xC0FFEEu);
    if (!game) {
        CloseWindow();
        return 1;
    }

    while (!WindowShouldClose()) {
        float real_dt = GetFrameTime();
        update_camera(&camera, real_dt);

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
