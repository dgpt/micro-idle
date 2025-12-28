// Test microbe volume stability over time
#include "raylib.h"
#include "game/game.h"
#include "engine/platform/engine.h"
#include "tests/test_env.h"
#include <stdio.h>

int test_deflation_run(void) {
    printf("test_deflation: starting\n");

    // Test with just 1 microbe to isolate the deflation issue
    test_set_env("MICRO_IDLE_INITIAL_ENTITIES", "1");

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "test_deflation");

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
        printf("test_deflation: game create failed\n");
        CloseWindow();
        return 1;
    }

    printf("test_deflation: running for 180 frames\n");

    // Run for 3 seconds and check particle count stability
    int initial_particles = 0;
    for (int frame = 0; frame < 180; frame++) {
        int screen_w = GetRenderWidth();
        int screen_h = GetRenderHeight();
        int steps = engine_time_update(&engine, 1.0f / 60.0f);
        game_handle_input(game, camera, 1.0f / 60.0f, screen_w, screen_h);
        for (int i = 0; i < steps; ++i) {
            game_update_fixed(game, (float)engine.time.tick_dt);
        }

        // Record initial particle count
        if (frame == 0) {
            initial_particles = game_get_particle_count(game);
            int microbe_count = game_get_microbe_count(game);
            float volume = game_get_microbe_volume(game, 0);
            float radius = game_get_microbe_radius(game, 0);
            float cx, cy, cz;
            game_get_microbe_position(game, 0, &cx, &cy, &cz);
            printf("test_deflation: FRAME 0 - particles = %d, microbes = %d, volume = %.3f, radius = %.3f, pos = (%.3f, %.3f, %.3f)\n",
                   initial_particles, microbe_count, volume, radius, cx, cy, cz);
        }

        // Check at 1 second, 2 seconds, 3 seconds
        if (frame == 60 || frame == 120 || frame == 179) {
            int current_particles = game_get_particle_count(game);
            float volume = game_get_microbe_volume(game, 0);
            float radius = game_get_microbe_radius(game, 0);

            // CRITICAL: Check center of mass position
            float cx, cy, cz;
            game_get_microbe_position(game, 0, &cx, &cy, &cz);

            printf("test_deflation: frame %d, particles = %d, volume = %.3f, radius = %.3f, pos = (%.3f, %.3f, %.3f)\n",
                   frame, current_particles, volume, radius, cx, cy, cz);

            if (current_particles != initial_particles) {
                printf("FAIL: Particle count changed! %d -> %d\n", initial_particles, current_particles);
                game_destroy(game);
                CloseWindow();
                return 1;
            }
        }

        BeginDrawing();
        ClearBackground((Color){10, 20, 30, 255});
        BeginMode3D(camera);
        game_render(game, camera, engine_time_alpha(&engine));
        EndMode3D();
        game_render_ui(game, screen_w, screen_h);
        EndDrawing();

        if (frame == 60) {
            TakeScreenshot("test_deflation.png");
        }
    }

    printf("test_deflation: PASS - microbe volume stable\n");
    game_destroy(game);
    CloseWindow();
    return 0;
}
