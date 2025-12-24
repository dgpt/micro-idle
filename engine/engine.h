#ifndef MICRO_IDLE_ENGINE_H
#define MICRO_IDLE_ENGINE_H

#include <stdbool.h>
#include "engine/time.h"

typedef struct EngineConfig {
    int window_w;
    int window_h;
    int target_fps;
    int tick_hz;
    bool vsync;
    bool dev_mode;
} EngineConfig;

typedef struct EngineContext {
    EngineConfig cfg;
    TimeState time;
} EngineContext;

void engine_init(EngineContext *ctx, EngineConfig cfg);
int engine_time_update(EngineContext *ctx, double real_dt);
float engine_time_alpha(const EngineContext *ctx);

#endif
