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

    return fails;
}
