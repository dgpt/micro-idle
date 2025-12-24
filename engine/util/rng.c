#include "engine/util/rng.h"

static uint64_t xorshift64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

void rng_seed(Rng *rng, uint64_t seed) {
    if (seed == 0) {
        seed = 0x9E3779B97F4A7C15ULL;
    }
    rng->state = seed;
}

uint32_t rng_next_u32(Rng *rng) {
    return (uint32_t)(xorshift64(&rng->state) & 0xFFFFFFFFu);
}

float rng_next_f01(Rng *rng) {
    return (float)(rng_next_u32(rng) / 4294967296.0);
}

float rng_range(Rng *rng, float min, float max) {
    return min + (max - min) * rng_next_f01(rng);
}

int rng_range_i(Rng *rng, int min, int max_inclusive) {
    if (max_inclusive <= min) {
        return min;
    }
    uint32_t span = (uint32_t)(max_inclusive - min + 1);
    return min + (int)(rng_next_u32(rng) % span);
}
