#include "engine/util/rng.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("RNG - deterministic sequence", "[rng]") {
    Rng a, b;
    rng_seed(&a, 1234u);
    rng_seed(&b, 1234u);

    // Generate same sequence from same seed
    for (int i = 0; i < 10; i++) {
        REQUIRE(rng_next_u32(&a) == rng_next_u32(&b));
    }
}

TEST_CASE("RNG - int range bounds", "[rng]") {
    Rng rng;
    rng_seed(&rng, 42u);

    for (int i = 0; i < 100; i++) {
        int v = rng_range_i(&rng, -3, 7);
        REQUIRE(v >= -3);
        REQUIRE(v <= 7);
    }
}

TEST_CASE("RNG - seed zero sets default state", "[rng]") {
    Rng rng;
    rng_seed(&rng, 0u);
    REQUIRE(rng.state != 0u);
}

TEST_CASE("RNG - float 0-1 range", "[rng]") {
    Rng rng;
    rng_seed(&rng, 0u);

    float f01 = rng_next_f01(&rng);
    REQUIRE(f01 >= 0.0f);
    REQUIRE(f01 < 1.0f);
}

TEST_CASE("RNG - float range bounds", "[rng]") {
    Rng rng;
    rng_seed(&rng, 0u);

    float fr = rng_range(&rng, -2.0f, 2.0f);
    REQUIRE(fr >= -2.0f);
    REQUIRE(fr <= 2.0f);
}

TEST_CASE("RNG - inverted int range returns min", "[rng]") {
    Rng rng;
    rng_seed(&rng, 0u);

    int same = rng_range_i(&rng, 5, 4);
    REQUIRE(same == 5);
}
