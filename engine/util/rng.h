#ifndef MICRO_IDLE_RNG_H
#define MICRO_IDLE_RNG_H

#include <stdint.h>

typedef struct Rng {
    uint64_t state;
} Rng;

void rng_seed(Rng *rng, uint64_t seed);
uint32_t rng_next_u32(Rng *rng);
float rng_next_f01(Rng *rng);
float rng_range(Rng *rng, float min, float max);
int rng_range_i(Rng *rng, int min, int max_inclusive);

#endif
