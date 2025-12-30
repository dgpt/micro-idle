#ifndef MICRO_IDLE_VEC3_H
#define MICRO_IDLE_VEC3_H

#include <cmath>
#include <cstdint>
#include "raylib.h"

namespace math {

struct Vec3 {
    float x, y, z;

    // Constructors
    constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    constexpr Vec3(float s) : x(s), y(s), z(s) {}

    // Copy constructor and assignment
    constexpr Vec3(const Vec3&) = default;
    constexpr Vec3& operator=(const Vec3&) = default;

    // Conversion from/to Raylib Vector3 for compatibility
    Vec3(const Vector3& v) : x(v.x), y(v.y), z(v.z) {}
    operator Vector3() const { return {x, y, z}; }

    // Basic arithmetic
    constexpr Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    constexpr Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    constexpr Vec3 operator*(float s) const {
        return Vec3(x * s, y * s, z * s);
    }

    constexpr Vec3 operator/(float s) const {
        return Vec3(x / s, y / s, z / s);
    }

    // Compound assignment
    constexpr Vec3& operator+=(const Vec3& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    constexpr Vec3& operator-=(const Vec3& other) {
        x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    constexpr Vec3& operator*=(float s) {
        x *= s; y *= s; z *= s;
        return *this;
    }

    constexpr Vec3& operator/=(float s) {
        x /= s; y /= s; z /= s;
        return *this;
    }

    // Unary operators
    constexpr Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    // Comparison
    constexpr bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    constexpr bool operator!=(const Vec3& other) const {
        return !(*this == other);
    }

    // Vector operations
    constexpr float dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    constexpr Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    constexpr float length_squared() const {
        return x*x + y*y + z*z;
    }

    float length() const {
        return std::sqrt(length_squared());
    }

    Vec3 normalized() const {
        float len = length();
        return len > 0.0f ? *this / len : Vec3(0.0f);
    }

    void normalize() {
        float len = length();
        if (len > 0.0f) {
            *this /= len;
        }
    }

    // Distance functions
    float distance_squared(const Vec3& other) const {
        Vec3 diff = *this - other;
        return diff.length_squared();
    }

    float distance(const Vec3& other) const {
        return std::sqrt(distance_squared(other));
    }

    // Utility functions
    Vec3 abs() const {
        return Vec3(std::abs(x), std::abs(y), std::abs(z));
    }

    Vec3 min(const Vec3& other) const {
        return Vec3(
            std::fmin(x, other.x),
            std::fmin(y, other.y),
            std::fmin(z, other.z)
        );
    }

    Vec3 max(const Vec3& other) const {
        return Vec3(
            std::fmax(x, other.x),
            std::fmax(y, other.y),
            std::fmax(z, other.z)
        );
    }

    // Linear interpolation
    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * t;
    }

    // Static constants
    static constexpr Vec3 zero() { return Vec3(0.0f, 0.0f, 0.0f); }
    static constexpr Vec3 one() { return Vec3(1.0f, 1.0f, 1.0f); }
    static constexpr Vec3 unit_x() { return Vec3(1.0f, 0.0f, 0.0f); }
    static constexpr Vec3 unit_y() { return Vec3(0.0f, 1.0f, 0.0f); }
    static constexpr Vec3 unit_z() { return Vec3(0.0f, 0.0f, 1.0f); }
};

// Free functions for symmetry
constexpr Vec3 operator*(float s, const Vec3& v) {
    return v * s;
}

} // namespace math

#endif // MICRO_IDLE_VEC3_H
