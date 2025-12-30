#include <catch2/catch_test_macros.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

// Include raylib first to avoid Windows.h conflicts
#include "raylib.h"
#include "rlgl.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "src/World.h"
#include "src/components/Microbe.h"
#include "engine/platform/engine.h"

using namespace micro_idle;

// Cross-platform function to delete all PNG files in screenshots directory
static void clearScreenshots() {
#ifndef _WIN32
    DIR* dir = opendir("screenshots");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".png") != NULL) {
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "screenshots/%s", entry->d_name);
                unlink(filepath);
            }
        }
        closedir(dir);
    }
#endif
    // On Windows, screenshots will be overwritten, so clearing is optional
}

TEST_CASE("Visual Test (Headless Game Run)", "[visual]") {
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

    // Create World (FLECS + Jolt system)
    World world;

    // Create test amoebas
    world.createAmoeba((Vector3){0.0f, 5.0f, 0.0f}, 2.0f, RED);
    world.createAmoeba((Vector3){5.0f, 5.0f, 0.0f}, 1.5f, BLUE);
    world.createAmoeba((Vector3){-5.0f, 5.0f, 0.0f}, 1.8f, GREEN);

    const int totalFrames = 60 * 5; // 5 seconds at 60 FPS
    const int screenshotInterval = 20; // Every 20 frames = 3 per second
    int screenshotCount = 0;

    for (int frame = 0; frame < totalFrames; frame++) {
        // Run game update (actual game code)
        float dt = 1.0f / 60.0f;
        int steps = engine_time_update(&engine, dt);
        world.handleInput(camera, dt, 1280, 720);
        for (int i = 0; i < steps; i++) {
            world.update((float)engine.time.tick_dt);
        }

        // Take screenshot every interval
        if (frame % screenshotInterval == 0) {
            // Begin drawing to render texture (BeginTextureMode handles BeginDrawing internally)
            BeginTextureMode(target);
            ClearBackground((Color){10, 20, 30, 255});

            // Render world
            world.render(camera, engine_time_alpha(&engine));
            world.renderUI(1280, 720);

            EndTextureMode();

            // Save screenshot
            Image image = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&image);

            char filename[256];
            snprintf(filename, sizeof(filename), "screenshots/frame_%03d.png", screenshotCount);
            mkdir("screenshots", 0755);

            if (ExportImage(image, filename)) {
                screenshotCount++;
            }
            UnloadImage(image);
        }
    }

    UnloadRenderTexture(target);
    CloseWindow();

    REQUIRE(screenshotCount > 0);
}
