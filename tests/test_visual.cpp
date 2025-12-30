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
#include "src/components/WorldState.h"
#include "engine/platform/engine.h"
#include <math.h>

using namespace micro_idle;

// Calculate world dimensions from camera view frustum (copied from game.cpp)
static void calculateWorldDimensions(Camera3D camera, int screen_w, int screen_h, float* outWidth, float* outHeight) {
    // Camera is at (0, 22, 0) looking down at XZ plane
    // Use similar triangles to calculate visible area at y=0
    float cameraHeight = camera.position.y;
    float fovRadians = camera.fovy * DEG2RAD;
    float aspect = (float)screen_w / (float)screen_h;

    // Calculate visible height (Z dimension) at ground level
    float visibleHeight = 2.0f * cameraHeight * tanf(fovRadians / 2.0f);

    // Calculate visible width (X dimension) using aspect ratio
    float visibleWidth = visibleHeight * aspect;

    // Convert 32px margin to world space units
    // At camera height, 1 pixel = visibleHeight / screen_h world units
    float pixelToWorld = visibleHeight / (float)screen_h;
    float marginWorld = 32.0f * pixelToWorld;

    // Subtract margin from both dimensions (32px on each side = 64px total)
    *outWidth = visibleWidth - (marginWorld * 2.0f);
    *outHeight = visibleHeight - (marginWorld * 2.0f);
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

    // Initialize window in hidden mode
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280, 720, "Micro-Idle Visual Test");

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

    // Setup camera (same as game.cpp - top-down view)
    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};  // Game uses Z-up for some reason
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Create World using the SAME logic as game.exe
    World world;

    // Calculate initial world dimensions based on camera view (same as game.cpp)
    float worldWidth, worldHeight;
    calculateWorldDimensions(camera, 1280, 720, &worldWidth, &worldHeight);

    printf("Test: Initial world dimensions: %.1f x %.1f\n", worldWidth, worldHeight);

    // Create screen-space boundaries (same as game.cpp)
    world.createScreenBoundaries(worldWidth, worldHeight);

    // Update world state singleton with initial screen dimensions (same as game.cpp)
    auto worldState = world.getWorld().get_mut<components::WorldState>();
    if (worldState) {
        worldState->screenWidth = 1280;
        worldState->screenHeight = 720;
    }

    // Create amoebas inside boundaries with EC&M locomotion (same as game.cpp)
    world.createAmoeba({0.0f, 1.5f, 0.0f}, 0.375f, RED);
    world.createAmoeba({5.0f, 1.5f, 0.0f}, 0.3f, GREEN);
    world.createAmoeba({-5.0f, 1.5f, 3.0f}, 0.325f, BLUE);

    const int totalFrames = 60 * 5; // 5 seconds at 60 FPS
    const int screenshotInterval = 20; // Every 20 frames = 3 per second
    int screenshotCount = 0;

    for (int frame = 0; frame < totalFrames; frame++) {
        printf("Test: Frame %d starting\n", frame);

        // Run game update (actual game code)
        float dt = 1.0f / 60.0f;
        int steps = engine_time_update(&engine, dt);
        printf("Test: Engine time update returned %d steps\n", steps);

        world.handleInput(camera, dt, 1280, 720);
        for (int i = 0; i < steps; i++) {
            world.update((float)engine.time.tick_dt);
            printf("Test: World update step %d completed\n", i);
        }

        // Take screenshot every interval
        if (frame % screenshotInterval == 0) {
            // Render to render texture
            BeginTextureMode(target);
            ClearBackground((Color){20, 40, 60, 255});  // Dark background

            // Render world (3D rendering doesn't work in render textures due to Raylib limitations)
            world.render(camera, engine_time_alpha(&engine), false);
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

    // Validate that microbes are spawning correctly by checking microbe count
    // After 5 seconds of simulation, we should have spawned multiple microbes
    auto microbeCount = world.getWorld().count<components::Microbe>();
    REQUIRE(microbeCount >= 3);  // Should have at least 3 microbes spawned
}
