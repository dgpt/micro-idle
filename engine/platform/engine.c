#include "engine/platform/engine.h"
#include "engine/platform/time.h"

void engine_init(EngineContext *ctx, EngineConfig cfg) {
    ctx->cfg = cfg;
    time_init(&ctx->time, cfg.tick_hz);
}

int engine_time_update(EngineContext *ctx, double real_dt) {
    return time_update(&ctx->time, real_dt);
}

float engine_time_alpha(const EngineContext *ctx) {
    return time_alpha(&ctx->time);
}
