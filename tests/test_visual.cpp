#include <stdio.h>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include "raylib.h"
#include "rlgl.h"
#include "game/game.h"
#include "engine/platform/engine.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

// Delete all PNG files in screenshots directory
static void clearScreenshots() {
#ifdef _WIN32
    system("del /Q screenshots\\*.png 2>nul");
#else
    system("rm -f screenshots/*.png 2>/dev/null");
#endif
}

// Headless visual test - runs actual game code with screenshot output
int test_visual_run(void) {
    printf("\n=== Visual Test (Headless Game Run) ===\n");

    // Create screenshots directory
    mkdir("screenshots", 0755);
    clearScreenshots();

    // Initialize window in hidden mode
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Micro-Idle Visual Test (Headless)");
    SetTargetFPS(60);

    // Create render texture for offscreen rendering
    RenderTexture2D target = LoadRenderTexture(1280, 720);

    // Initialize engine (same as main.cpp)
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

    // Setup camera (same as main.cpp)
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Create game state (actual game code)
    GameState *game = game_create(0xC0FFEEu);
    if (!game) {
        printf("✗ Failed to create game state\n");
        UnloadRenderTexture(target);
        CloseWindow();
        return 1;
    }

    printf("Running headless game simulation (5 seconds, 3 screenshots/sec)...\n");

    const int totalFrames = 60 * 5; // 5 seconds at 60 FPS
    const int screenshotInterval = 20; // Every 20 frames = 3 per second
    int screenshotCount = 0;

    for (int frame = 0; frame < totalFrames; frame++) {
        // Run game update (actual game code)
        float dt = 1.0f / 60.0f;
        int steps = engine_time_update(&engine, dt);
        game_handle_input(game, camera, dt, 1280, 720);
        for (int i = 0; i < steps; i++) {
            game_update_fixed(game, (float)engine.time.tick_dt);
        }

        // Take screenshot every interval
        if (frame % screenshotInterval == 0) {
            BeginTextureMode(target);
            ClearBackground((Color){10, 20, 30, 255});
            game_render(game, camera, engine_time_alpha(&engine));
            game_render_ui(game, 1280, 720);
            EndTextureMode();

            // Save screenshot
            Image image = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&image);

            char filename[256];
            snprintf(filename, sizeof(filename), "screenshots/frame_%03d.png", screenshotCount);
            if (ExportImage(image, filename)) {
                printf("  Frame %d/300: %s\n", frame, filename);
                screenshotCount++;
            }
            UnloadImage(image);
        }
    }

    printf("✓ Saved %d screenshots to screenshots/\n", screenshotCount);

    game_destroy(game);
    UnloadRenderTexture(target);
    CloseWindow();

    printf("✓ Visual test passed (game ran headless for 5 seconds)\n");
    return 0;
}
