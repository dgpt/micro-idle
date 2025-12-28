#include "engine/platform/engine.h"

#include <stdio.h>

int test_engine_run(void) {
    EngineContext ctx = {0};
    EngineConfig cfg = {
        .window_w = 640,
        .window_h = 360,
        .target_fps = 60,
        .tick_hz = 60,
        .vsync = false,
        .dev_mode = false
    };

    engine_init(&ctx, cfg);

    int steps = engine_time_update(&ctx, 1.0 / 60.0);
    if (steps != 1) {
        printf("engine expected 1 step got %d\n", steps);
        return 1;
    }

    float alpha = engine_time_alpha(&ctx);
    if (alpha < 0.0f || alpha > 1.0f) {
        printf("engine alpha out of range: %.3f\n", alpha);
        return 1;
    }

    return 0;
}
