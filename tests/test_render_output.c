#include "raylib.h"
#include "game/game.h"

#include <stdio.h>
#include <stdlib.h>

static int pixel_matches(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int test_render_output_run(void) {
#if defined(__linux__)
    setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 0);
#endif

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE);
    InitWindow(256, 256, "render_output_test");
    if (!IsWindowReady()) {
        printf("render output window failed to init\n");
        return 1;
    }

    setenv("MICRO_IDLE_ENTITY_MAX", "512", 1);
    setenv("MICRO_IDLE_ENTITY_COUNT", "512", 1);
    setenv("MICRO_IDLE_INITIAL_ENTITIES", "128", 1);

    Camera3D camera = {0};
    camera.position = (Vector3){0.0f, 22.0f, 0.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    GameState *game = game_create(0x12345678u);
    if (!game) {
        printf("render output game create failed\n");
        CloseWindow();
        return 1;
    }

    RenderTexture2D target = LoadRenderTexture(256, 256);
    Color clear = (Color){8, 12, 18, 255};
    Color plane = (Color){10, 20, 30, 255};

    BeginTextureMode(target);
    ClearBackground(clear);
    game_render(game, camera, 0.0f);
    EndTextureMode();

    Image img = LoadImageFromTexture(target.texture);
    Color *pixels = LoadImageColors(img);
    int found = 0;
    if (pixels) {
        int total = img.width * img.height;
        for (int i = 0; i < total; i += 97) {
            if (!pixel_matches(pixels[i], plane)) {
                found = 1;
                break;
            }
        }
        UnloadImageColors(pixels);
    }
    UnloadImage(img);
    UnloadRenderTexture(target);

    int resized_ok = 1;

    game_destroy(game);
    CloseWindow();

    if (!found) {
        printf("render output missing entity pixels\n");
        return 1;
    }

    if (!resized_ok) {
        printf("render output resize did not update screen size\n");
        return 1;
    }

    return 0;
}
