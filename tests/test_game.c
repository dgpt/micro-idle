#include "game/game.h"
#include "game/gpu_sim.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>

int test_game_run(void) {
#if defined(__linux__)
    setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 0);
#endif

    game_destroy(NULL);
    game_update_fixed(NULL, 1.0f / 60.0f);
    game_render(NULL, (Camera3D){0}, 0.0f);

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 360, "game_test");
    if (!IsWindowReady()) {
        printf("game window failed to init\n");
        return 1;
    }

#ifdef GPU_SIM_TESTING
    gpu_sim_test_set_fail_mode(1);
    GameState *fail = game_create(0xBADC0DEu);
    if (fail != NULL) {
        printf("game create should fail with gpu sim compile error\n");
        game_destroy(fail);
        CloseWindow();
        return 1;
    }
    gpu_sim_test_set_fail_mode(0);
#endif

    unsetenv("MICRO_IDLE_ENTITY_MAX");
    unsetenv("MICRO_IDLE_ENTITY_COUNT");
    unsetenv("MICRO_IDLE_INITIAL_ENTITIES");
    unsetenv("MICRO_IDLE_ENTITIES_PER_SEC");
    unsetenv("MICRO_IDLE_BOUNDS_X");
    unsetenv("MICRO_IDLE_BOUNDS_Z");
    unsetenv("MICRO_IDLE_PLANE_WIDTH");
    unsetenv("MICRO_IDLE_PLANE_HEIGHT");
    GameState *defaults = game_create(0x1234u);
    if (!defaults) {
        printf("game create failed for defaults\n");
        CloseWindow();
        return 1;
    }
    game_destroy(defaults);

    setenv("MICRO_IDLE_ENTITY_MAX", "5", 1);
    setenv("MICRO_IDLE_ENTITY_COUNT", "5", 1);
    setenv("MICRO_IDLE_INITIAL_ENTITIES", "100", 1);
    setenv("MICRO_IDLE_ENTITIES_PER_SEC", "3", 1);
    GameState *clamped = game_create(0xFA17u);
    if (!clamped) {
        printf("game create failed for clamp test\n");
        CloseWindow();
        return 1;
    }
    game_update_fixed(clamped, 1.1f);
    game_destroy(clamped);

    setenv("MICRO_IDLE_ENTITY_MAX", "5", 1);
    setenv("MICRO_IDLE_ENTITY_COUNT", "5", 1);
    setenv("MICRO_IDLE_INITIAL_ENTITIES", "2", 1);
    setenv("MICRO_IDLE_ENTITIES_PER_SEC", "4", 1);
    GameState *spawn = game_create(0xABCDu);
    if (!spawn) {
        printf("game create failed for spawn test\n");
        CloseWindow();
        return 1;
    }
    game_update_fixed(spawn, 1.2f);
    game_destroy(spawn);

    setenv("MICRO_IDLE_ENTITY_MAX", "500", 1);
    setenv("MICRO_IDLE_ENTITY_COUNT", "bad", 1);
    setenv("MICRO_IDLE_INITIAL_ENTITIES", "100", 1);
    setenv("MICRO_IDLE_ENTITIES_PER_SEC", "2", 1);
    setenv("MICRO_IDLE_BOUNDS_X", "-1", 1);
    setenv("MICRO_IDLE_BOUNDS_Z", "8.5", 1);
    setenv("MICRO_IDLE_PLANE_WIDTH", "40", 1);
    setenv("MICRO_IDLE_PLANE_HEIGHT", "0", 1);
    GameState *probe = game_create(0xFACEB00Cu);
    if (!probe) {
        printf("game create failed for probe\n");
        CloseWindow();
        return 1;
    }
    game_destroy(probe);

    setenv("MICRO_IDLE_ENTITY_MAX", "20000", 1);
    setenv("MICRO_IDLE_ENTITY_COUNT", "20000", 1);
    setenv("MICRO_IDLE_INITIAL_ENTITIES", "200", 1);
    setenv("MICRO_IDLE_ENTITIES_PER_SEC", "5", 1);
    setenv("MICRO_IDLE_BOUNDS_X", "18.0", 1);
    setenv("MICRO_IDLE_BOUNDS_Z", "15.0", 1);
    setenv("MICRO_IDLE_PLANE_WIDTH", "36.0", 1);
    setenv("MICRO_IDLE_PLANE_HEIGHT", "28.0", 1);

    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    GameState *game = game_create(0xC0FFEEu);
    if (!game) {
        printf("game create failed\n");
        CloseWindow();
        return 1;
    }

    for (int i = 0; i < 3; ++i) {
        game_handle_input(game, camera, 1.0f / 60.0f, 640, 360);
        game_update_fixed(game, 1.0f / 60.0f);
        BeginDrawing();
        ClearBackground(BLACK);
        game_render(game, camera, 0.0f);
        game_render_ui(NULL, 640, 360);
        game_render_ui(game, 640, 360);
        EndDrawing();
    }

    game_destroy(game);
    CloseWindow();
    return 0;
}
