#ifndef MICRO_IDLE_QUAT_H
#define MICRO_IDLE_QUAT_H

#include "Vec3.h"
#include <cmath>
#include <cstdint>
#include "raylib.h"

namespace math {

struct Quat {
    float x, y, z, w;

    // Constructors
    constexpr Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    constexpr Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr Quat(float w, const Vec3& v) : x(v.x), y(v.y), z(v.z), w(w) {}

    // Copy constructor and assignment
    constexpr Quat(const Quat&) = default;
    constexpr Quat& operator=(const Quat&) = default;

    // Conversion from/to Raylib Quaternion for compatibility
    Quat(const Quaternion& q) : x(q.x), y(q.y), z(q.z), w(q.w) {}
    operator Quaternion() const { return {x, y, z, w}; }

    // Basic arithmetic
    constexpr Quat operator+(const Quat& other) const {
        return Quat(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    constexpr Quat operator-(const Quat& other) const {
        return Quat(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    Quat operator*(const Quat& other) const {
        return Quat(
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        );
    }

    constexpr Quat operator*(float s) const {
        return Quat(x * s, y * s, z * s, w * s);
    }

    // Compound assignment
    constexpr Quat& operator+=(const Quat& other) {
        x += other.x; y += other.y; z += other.z; w += other.w;
        return *this;
    }

    constexpr Quat& operator-=(const Quat& other) {
        x -= other.x; y -= other.y; z -= other.z; w -= other.w;
        return *this;
    }

    Quat& operator*=(const Quat& other) {
        *this = *this * other;
        return *this;
    }

    constexpr Quat& operator*=(float s) {
        x *= s; y *= s; z *= s; w *= s;
        return *this;
    }

    // Unary operators
    constexpr Quat operator-() const {
        return Quat(-x, -y, -z, -w);
    }

    // Comparison
    constexpr bool operator==(const Quat& other) const {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }

    constexpr bool operator!=(const Quat& other) const {
        return !(*this == other);
    }

    // Quaternion operations
    float length_squared() const {
        return x*x + y*y + z*z + w*w;
    }

    float length() const {
        return std::sqrt(length_squared());
    }

    Quat normalized() const {
        float len = length();
        return len > 0.0f ? *this * (1.0f / len) : Quat(0.0f, 0.0f, 0.0f, 1.0f);
    }

    void normalize() {
        float len = length();
        if (len > 0.0f) {
            *this *= (1.0f / len);
        }
    }

    Quat conjugate() const {
        return Quat(-x, -y, -z, w);
    }

    Quat inverse() const {
        float len_sq = length_squared();
        if (len_sq > 0.0f) {
            float inv_len_sq = 1.0f / len_sq;
            return Quat(-x * inv_len_sq, -y * inv_len_sq, -z * inv_len_sq, w * inv_len_sq);
        }
        return Quat(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Rotate a vector by this quaternion
    Vec3 rotate(const Vec3& v) const {
        Quat qv(0.0f, v);
        Quat result = *this * qv * conjugate();
        return Vec3(result.x, result.y, result.z);
    }

    // Get the axis-angle representation
    void to_axis_angle(Vec3& axis, float& angle) const {
        float len = std::sqrt(x*x + y*y + z*z);
        if (len > 0.0f) {
            axis = Vec3(x, y, z) / len;
            angle = 2.0f * std::atan2(len, w);
        } else {
            axis = Vec3::unit_y();
            angle = 0.0f;
        }
    }

    // Static factory methods
    static Quat from_axis_angle(const Vec3& axis, float angle) {
        float half_angle = angle * 0.5f;
        float sin_half = std::sin(half_angle);
        return Quat(
            axis.x * sin_half,
            axis.y * sin_half,
            axis.z * sin_half,
            std::cos(half_angle)
        );
    }

    static Quat from_euler_angles(float yaw, float pitch, float roll) {
        // Tait-Bryan angles: yaw (Y), pitch (X), roll (Z)
        float cy = std::cos(yaw * 0.5f);
        float sy = std::sin(yaw * 0.5f);
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cr = std::cos(roll * 0.5f);
        float sr = std::sin(roll * 0.5f);

        return Quat(
            cy * cp * sr - sy * sp * cr,
            sy * cp * sr + cy * sp * cr,
            sy * cp * cr - cy * sp * sr,
            cy * cp * cr + sy * sp * sr
        );
    }

    static Quat look_at(const Vec3& forward, const Vec3& up = Vec3::unit_y()) {
        Vec3 f = forward.normalized();
        Vec3 r = up.cross(f).normalized();
        Vec3 u = f.cross(r);

        float trace = r.x + u.y + f.z;
        if (trace > 0.0f) {
            float s = 0.5f / std::sqrt(trace + 1.0f);
            return Quat(
                (u.z - f.y) * s,
                (f.x - r.z) * s,
                (r.y - u.x) * s,
                0.25f / s
            );
        } else if (r.x > u.y && r.x > f.z) {
            float s = 2.0f * std::sqrt(1.0f + r.x - u.y - f.z);
            return Quat(
                0.25f * s,
                (r.y + u.x) / s,
                (f.x + r.z) / s,
                (u.z - f.y) / s
            );
        } else if (u.y > f.z) {
            float s = 2.0f * std::sqrt(1.0f + u.y - r.x - f.z);
            return Quat(
                (r.y + u.x) / s,
                0.25f * s,
                (u.z + f.y) / s,
                (f.x - r.z) / s
            );
        } else {
            float s = 2.0f * std::sqrt(1.0f + f.z - r.x - u.y);
            return Quat(
                (f.x + r.z) / s,
                (u.z + f.y) / s,
                0.25f * s,
                (r.y - u.x) / s
            );
        }
    }

    // Spherical linear interpolation
    static Quat slerp(const Quat& a, const Quat& b, float t) {
        float cos_half_theta = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        // If quaternions are very close, use linear interpolation
        if (std::abs(cos_half_theta) >= 1.0f) {
            return a;
        }

        float half_theta = std::acos(cos_half_theta);
        float sin_half_theta = std::sqrt(1.0f - cos_half_theta * cos_half_theta);

        // If theta is very small, use linear interpolation
        if (std::abs(sin_half_theta) < 0.001f) {
            return Quat(
                a.x * (1.0f - t) + b.x * t,
                a.y * (1.0f - t) + b.y * t,
                a.z * (1.0f - t) + b.z * t,
                a.w * (1.0f - t) + b.w * t
            ).normalized();
        }

        float ratio_a = std::sin((1.0f - t) * half_theta) / sin_half_theta;
        float ratio_b = std::sin(t * half_theta) / sin_half_theta;

        return Quat(
            a.x * ratio_a + b.x * ratio_b,
            a.y * ratio_a + b.y * ratio_b,
            a.z * ratio_a + b.z * ratio_b,
            a.w * ratio_a + b.w * ratio_b
        );
    }

    // Static constants
    static constexpr Quat identity() { return Quat(0.0f, 0.0f, 0.0f, 1.0f); }
};

// Free functions for symmetry
constexpr Quat operator*(float s, const Quat& q) {
    return q * s;
}

} // namespace math

#endif // MICRO_IDLE_QUAT_H
