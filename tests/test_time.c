#include "engine/platform/time.h"

#include <math.h>
#include <stdio.h>

static int expect_close(const char *label, float value, float expected, float eps) {
    float diff = fabsf(value - expected);
    if (diff > eps) {
        printf("time %s expected %.4f got %.4f\n", label, expected, value);
        return 1;
    }
    return 0;
}

int test_time_run(void) {
    int fails = 0;
    TimeState state;
    time_init(&state, 60);

    int steps = time_update(&state, 1.0 / 60.0);
    if (steps != 1 || state.tick != 1) {
        printf("time step expected 1 got %d tick %llu\n", steps, (unsigned long long)state.tick);
        fails++;
    }

    steps = time_update(&state, 0.0);
    if (steps != 0) {
        printf("time step expected 0 got %d\n", steps);
        fails++;
    }

    steps = time_update(&state, 1.0 / 120.0);
    if (steps != 0) {
        printf("time step expected 0 for half tick got %d\n", steps);
        fails++;
    }
    fails += expect_close("alpha", time_alpha(&state), 0.5f, 0.05f);

    steps = time_update(&state, -1.0);
    if (steps != 0 || state.real_dt != 0.0) {
        printf("time negative dt not clamped\n");
        fails++;
    }

    steps = time_update(&state, state.tick_dt * 20.0);
    if (steps != 9) {
        printf("time expected max step clamp got %d\n", steps);
        fails++;
    }

    state.tick_dt = 0.0;
    fails += expect_close("alpha_zero_dt", time_alpha(&state), 0.0f, 0.0001f);

    return fails;
}
