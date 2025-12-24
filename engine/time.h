#ifndef MICRO_IDLE_TIME_H
#define MICRO_IDLE_TIME_H

#include <stdint.h>

typedef struct TimeState {
    double real_dt;
    double accumulator;
    double tick_dt;
    uint64_t tick;
} TimeState;

void time_init(TimeState *state, int tick_hz);
int time_update(TimeState *state, double real_dt);
float time_alpha(const TimeState *state);

#endif
