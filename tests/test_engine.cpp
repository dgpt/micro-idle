#include "engine/platform/engine.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Engine - initialization and time update", "[engine]") {
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
    REQUIRE(steps == 1);
}

TEST_CASE("Engine - alpha value range", "[engine]") {
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

    float alpha = engine_time_alpha(&ctx);
    REQUIRE(alpha >= 0.0f);
    REQUIRE(alpha <= 1.0f);
}
