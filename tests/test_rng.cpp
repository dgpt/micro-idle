#include "engine/util/rng.h"

#include <stdio.h>

static int expect_equal_u32(const char *label, uint32_t a, uint32_t b) {
    if (a != b) {
        printf("rng %s mismatch %u vs %u\n", label, a, b);
        return 1;
    }
    return 0;
}

int test_rng_run(void) {
    int fails = 0;
    Rng a;
    Rng b;
    rng_seed(&a, 1234u);
    rng_seed(&b, 1234u);

    for (int i = 0; i < 10; ++i) {
        fails += expect_equal_u32("seq", rng_next_u32(&a), rng_next_u32(&b));
    }

    rng_seed(&a, 42u);
    for (int i = 0; i < 100; ++i) {
        int v = rng_range_i(&a, -3, 7);
        if (v < -3 || v > 7) {
            printf("rng range out of bounds %d\n", v);
            fails++;
            break;
        }
    }

    rng_seed(&a, 0u);
    if (a.state == 0u) {
        printf("rng seed zero did not set default state\n");
        fails++;
    }

    float f01 = rng_next_f01(&a);
    if (f01 < 0.0f || f01 >= 1.0f) {
        printf("rng f01 out of range %.4f\n", f01);
        fails++;
    }

    float fr = rng_range(&a, -2.0f, 2.0f);
    if (fr < -2.0f || fr > 2.0f) {
        printf("rng range out of bounds %.4f\n", fr);
        fails++;
    }

    int same = rng_range_i(&a, 5, 4);
    if (same != 5) {
        printf("rng range_i expected min on inverted bounds\n");
        fails++;
    }

    return fails;
}
