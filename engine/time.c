#include "engine/time.h"

void time_init(TimeState *state, int tick_hz) {
    state->real_dt = 0.0;
    state->accumulator = 0.0;
    state->tick_dt = (tick_hz > 0) ? (1.0 / (double)tick_hz) : (1.0 / 60.0);
    state->tick = 0;
}

int time_update(TimeState *state, double real_dt) {
    if (real_dt < 0.0) {
        real_dt = 0.0;
    }
    state->real_dt = real_dt;
    state->accumulator += real_dt;

    int steps = 0;
    while (state->accumulator >= state->tick_dt) {
        state->accumulator -= state->tick_dt;
        state->tick++;
        steps++;
        if (steps > 8) {
            state->accumulator = 0.0;
            break;
        }
    }
    return steps;
}

float time_alpha(const TimeState *state) {
    if (state->tick_dt <= 0.0) {
        return 0.0f;
    }
    return (float)(state->accumulator / state->tick_dt);
}
