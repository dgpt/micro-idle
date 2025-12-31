#include <catch2/catch_test_macros.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

// Include raylib first to avoid Windows.h conflicts
#include "raylib.h"
#include "rlgl.h"

#include "src/systems/SpawnSystem.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "game/game.h"
#include "engine/platform/engine.h"
#include <math.h>

// Calculate world dimensions from camera view frustum (copied from game.cpp)
static void calculateWorldDimensions(Camera3D camera, int screen_w, int screen_h, float* outWidth, float* outHeight) {
    float aspect = (float)screen_w / (float)screen_h;
    float visibleHeight = 0.0f;
    float visibleWidth = 0.0f;

    if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        visibleHeight = camera.fovy;
        visibleWidth = visibleHeight * aspect;
    } else {
        // Camera is at (0, 22, 0) looking down at XZ plane
        // Use similar triangles to calculate visible area at y=0
        float cameraHeight = camera.position.y;
        float fovRadians = camera.fovy * DEG2RAD;

        // Calculate visible height (Z dimension) at ground level
        visibleHeight = 2.0f * cameraHeight * tanf(fovRadians / 2.0f);

        // Calculate visible width (X dimension) using aspect ratio
        visibleWidth = visibleHeight * aspect;
    }

    // Convert 32px margin to world space units
    // At camera height, 1 pixel = visibleHeight / screen_h world units
    float pixelToWorld = visibleHeight / (float)screen_h;
    float marginWorld = 32.0f * pixelToWorld;

    constexpr float microbeViewPadding = 2.2f;
    // Subtract margin from both dimensions (32px on each side = 64px total)
    *outWidth = visibleWidth - (marginWorld * 2.0f) - (microbeViewPadding * 2.0f);
    *outHeight = visibleHeight - (marginWorld * 2.0f) - (microbeViewPadding * 2.0f);
}

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

    // Initialize window for testing (on-screen for debugging)
    // SetWindowPosition(-2000, -2000);  // Position far off-screen
    InitWindow(1280, 720, "Micro-Idle Visual Test");

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

    // TODO: Speed up spawning for visual test (spawnRate is private)

    // Setup camera (same as game.cpp - top-down view)
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 9.0f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    // Create game state using EXACTLY the same API as game.exe
    GameState* game = game_create(0xC0FFEEu);
    REQUIRE(game != nullptr);

    const int totalFrames = 60 * 6; // 6 seconds at 60 FPS to ensure we get all screenshots
    const int burstCount = 5;
    int screenshotCount = 0;
    bool screenshotsTaken[burstCount] = {false, false, false, false, false};
    float screenshotTimes[burstCount];
    const float burstStart = 3.0f;
    const float burstDuration = 1.0f;
    const float burstInterval = burstDuration / (float)(burstCount - 1);
    for (int i = 0; i < burstCount; i++) {
        screenshotTimes[i] = burstStart + burstInterval * (float)i;
    }

    for (int frame = 0; frame < totalFrames; frame++) {
        // Run update using EXACTLY the same API as game.exe
        float dt = 1.0f / 60.0f;
        int steps = engine_time_update(&engine, dt);

        // Handle input (same as game.exe)
        game_handle_input(game, camera, dt, 1280, 720);

        // Update physics (same as game.exe)
        for (int i = 0; i < steps; i++) {
            game_update_fixed(game, (float)engine.time.tick_dt);
        }

        // Take screenshots at specific time intervals
        double currentTime = engine.time.tick * engine.time.tick_dt;
        for (int i = 0; i < burstCount; i++) {
            if (!screenshotsTaken[i] && currentTime >= screenshotTimes[i]) {
                printf("Test: Taking screenshot %d at %.2f seconds\n", i, currentTime);

                // Use game API for everything - this should match game.exe exactly
                BeginDrawing();
                ClearBackground((Color){18, 44, 52, 255});

                // Render using game API (same as game.exe)
                game_render(game, camera, engine_time_alpha(&engine));
                game_render_ui(game, 1280, 720);

                EndDrawing();

                // Take screenshot of the hidden window
                TakeScreenshot("temp_screenshot.png");
                if (FileExists("temp_screenshot.png")) {
                    // Load and save as properly named file
                    Image screenshot = LoadImage("temp_screenshot.png");

                    char filename[256];
                    snprintf(filename, sizeof(filename), "screenshots/frame_%03d.png", screenshotCount);
                    mkdir("screenshots", 0755);

                    if (ExportImage(screenshot, filename)) {
                        printf("Test: Screenshot exported to %s\n", filename);
                        screenshotCount++;
                        screenshotsTaken[i] = true;
                    } else {
                        printf("Test: Screenshot export failed\n");
                    }
                    UnloadImage(screenshot);
                    remove("temp_screenshot.png");
                }
            }
        }

        // Exit early if we've taken all screenshots
        if (screenshotsTaken[burstCount - 1]) {
            break;
        }
    }

    CloseWindow();

    REQUIRE(screenshotCount == burstCount);

    // Validate that microbes are spawning correctly using game API
    int finalMicrobeCount = game_get_microbe_count(game);
    REQUIRE(finalMicrobeCount >= 2);  // Should have at least 2 microbes spawned

    // Clean up game state
    game_destroy(game);
}
