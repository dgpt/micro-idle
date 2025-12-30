#ifndef MICRO_IDLE_RANDOM_H
#define MICRO_IDLE_RANDOM_H

#include <cstdint>
#include <limits>
#include <cmath>
#include "Vec3.h"
#include "Quat.h"

namespace math {

// Simple, fast PCG32 random number generator
// Based on Melissa O'Neill's PCG algorithm
class Random {
private:
    uint64_t state;
    uint64_t increment;

    uint32_t pcg32() {
        uint64_t oldstate = state;
        state = oldstate * 6364136223846793005ULL + increment;
        uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
        uint32_t rot = oldstate >> 59u;
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

public:
    // Initialize with seed
    Random(uint64_t seed = 0x853c49e6748fea9bULL) {
        state = seed;
        increment = 0xda3e39cb94b95bdbULL;
        // Warm up the generator
        for (int i = 0; i < 10; ++i) {
            pcg32();
        }
    }

    // Generate random uint32
    uint32_t next_u32() {
        return pcg32();
    }

    // Generate random uint32 in range [0, max)
    uint32_t next_u32(uint32_t max) {
        if (max == 0) return 0;
        return pcg32() % max;
    }

    // Generate random uint32 in range [min, max)
    uint32_t next_u32(uint32_t min, uint32_t max) {
        if (min >= max) return min;
        return min + next_u32(max - min);
    }

    // Generate random float in range [0, 1)
    float next_f01() {
        return static_cast<float>(pcg32()) / static_cast<float>(std::numeric_limits<uint32_t>::max());
    }

    // Generate random float in range [0, 1]
    float next_f01_inclusive() {
        return next_f01() + (next_u32() & 1) * (1.0f / static_cast<float>(std::numeric_limits<uint32_t>::max()));
    }

    // Generate random float in range [min, max)
    float next_f(float min, float max) {
        return min + (max - min) * next_f01();
    }

    // Generate random float in range [min, max]
    float next_f_inclusive(float min, float max) {
        return min + (max - min) * next_f01_inclusive();
    }

    // Generate random int in range [min, max_inclusive]
    int next_i(int min, int max_inclusive) {
        if (min > max_inclusive) return min;
        return min + static_cast<int>(next_u32(static_cast<uint32_t>(max_inclusive - min + 1)));
    }

    // Generate random bool with given probability
    bool next_bool(float probability = 0.5f) {
        return next_f01() < probability;
    }

    // Generate random sign (-1 or +1)
    int next_sign() {
        return (next_u32() & 1) ? 1 : -1;
    }

    // Generate random unit vector on unit sphere
    Vec3 next_unit_vector() {
        float theta = next_f(0.0f, 2.0f * 3.14159265359f);
        float phi = std::acos(2.0f * next_f01() - 1.0f);
        float sin_phi = std::sin(phi);

        return Vec3(
            sin_phi * std::cos(theta),
            sin_phi * std::sin(theta),
            std::cos(phi)
        );
    }

    // Generate random vector inside unit sphere
    Vec3 next_vector_in_sphere(float radius = 1.0f) {
        Vec3 v;
        do {
            v = Vec3(next_f(-1.0f, 1.0f), next_f(-1.0f, 1.0f), next_f(-1.0f, 1.0f));
        } while (v.length_squared() > 1.0f);
        return v * radius;
    }

    // Generate random vector on unit sphere surface
    Vec3 next_vector_on_sphere(float radius = 1.0f) {
        return next_unit_vector() * radius;
    }

    // Generate random point inside unit disk (2D)
    Vec3 next_point_in_disk(float radius = 1.0f) {
        float r = radius * std::sqrt(next_f01());
        float theta = next_f(0.0f, 2.0f * 3.14159265359f);
        return Vec3(r * std::cos(theta), 0.0f, r * std::sin(theta));
    }

    // Generate random point on unit disk edge (2D)
    Vec3 next_point_on_disk(float radius = 1.0f) {
        float theta = next_f(0.0f, 2.0f * 3.14159265359f);
        return Vec3(radius * std::cos(theta), 0.0f, radius * std::sin(theta));
    }

    // Generate random quaternion (uniform distribution on 4-sphere)
    Quat next_quaternion() {
        float u1 = next_f01();
        float u2 = next_f01();
        float u3 = next_f01();

        float sqrt_u1 = std::sqrt(u1);
        float sqrt_u1_inv = std::sqrt(1.0f - u1);

        return Quat(
            sqrt_u1_inv * std::sin(2.0f * 3.14159265359f * u2),
            sqrt_u1_inv * std::cos(2.0f * 3.14159265359f * u2),
            sqrt_u1 * std::sin(2.0f * 3.14159265359f * u3),
            sqrt_u1 * std::cos(2.0f * 3.14159265359f * u3)
        );
    }

    // Seed the generator with a new value
    void seed(uint64_t new_seed) {
        state = new_seed;
        increment = 0xda3e39cb94b95bdbULL;
        // Warm up again
        for (int i = 0; i < 10; ++i) {
            pcg32();
        }
    }

    // Get current seed (for reproducibility)
    uint64_t get_seed() const {
        return state;
    }
};

// Global random instance for convenience
extern Random g_random;

} // namespace math

#endif // MICRO_IDLE_RANDOM_H
