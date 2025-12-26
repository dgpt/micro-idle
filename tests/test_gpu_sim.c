#include "raylib.h"
#include "game/gpu_sim.h"
#include "tests/test_env.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int test_entity_count(void) {
    const char *env = getenv("MICRO_IDLE_TEST_ENTITIES");
    if (!env || *env == '\0') {
        return 20000;
    }

    char *end = NULL;
    long value = strtol(env, &end, 10);
    if (end == env || *end != '\0' || value <= 0 || value > INT_MAX) {
        return 20000;
    }

    return (int)value;
}

int test_gpu_sim_run(void) {
#if defined(__linux__)
    test_set_env("MESA_LOADER_DRIVER_OVERRIDE", "zink");
#endif

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 360, "gpu_sim_test");
    if (!IsWindowReady()) {
        printf("gpu sim window failed to init\n");
        return 1;
    }

    if (!gpu_sim_supported()) {
        printf("gpu sim unsupported on this renderer (requires real GPU)\n");
        CloseWindow();
        return 1;
    }

#ifdef GPU_SIM_TESTING
    {
        GpuSim sim = {0};
        gpu_sim_test_set_fail_mode(1);
        if (gpu_sim_init(&sim, 16)) {
            printf("gpu sim expected compile failure\n");
            gpu_sim_shutdown(&sim);
            CloseWindow();
            return 1;
        }

        gpu_sim_test_set_fail_mode(2);
        if (gpu_sim_init(&sim, 16)) {
            printf("gpu sim expected link failure\n");
            gpu_sim_shutdown(&sim);
            CloseWindow();
            return 1;
        }

        gpu_sim_test_set_fail_mode(3);
        if (gpu_sim_init(&sim, 16)) {
            printf("gpu sim expected alloc failure\n");
            gpu_sim_shutdown(&sim);
            CloseWindow();
            return 1;
        }

        gpu_sim_test_set_fail_mode(4);
        if (gpu_sim_init(&sim, 16)) {
            gpu_sim_shutdown(&sim);
        }

        gpu_sim_test_set_fail_mode(0);
    }
#endif

    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    GpuSim sim = {0};
    if (!gpu_sim_supported()) {
        printf("gpu sim not supported\n");
        CloseWindow();
        return 1;
    }
    if (gpu_sim_init(&sim, INT_MAX)) {
        printf("gpu sim should fail on excessive entity count\n");
        gpu_sim_shutdown(&sim);
        CloseWindow();
        return 1;
    }
    if (!gpu_sim_init(&sim, test_entity_count())) {
        printf("gpu sim init failed\n");
        CloseWindow();
        return 1;
    }

    gpu_sim_set_active_count(NULL, 10);
    gpu_sim_set_active_count(&sim, -5);
    if (sim.active_count != 0) {
        printf("gpu sim active count did not clamp to zero\n");
        gpu_sim_shutdown(&sim);
        CloseWindow();
        return 1;
    }
    gpu_sim_set_active_count(&sim, sim.entity_count + 10);
    if (sim.active_count != sim.entity_count) {
        printf("gpu sim active count did not clamp to max\n");
        gpu_sim_shutdown(&sim);
        CloseWindow();
        return 1;
    }

    for (int i = 0; i < 3; ++i) {
        gpu_sim_update(&sim, 1.0f / 60.0f, (Vector2){14.0f, 12.0f});
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        gpu_sim_render(&sim, camera);
        EndMode3D();
        EndDrawing();
    }

    RenderTexture2D target = LoadRenderTexture(128, 128);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    gpu_sim_render(&sim, camera);
    EndMode3D();
    EndTextureMode();
    UnloadRenderTexture(target);

    gpu_sim_shutdown(&sim);
    CloseWindow();
    return 0;
}
