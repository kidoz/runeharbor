// SPDX-License-Identifier: MIT
#pragma once

#include <array>

#include <cmath>

namespace runeharbor::graphics
{

/**
 * 3D Vector class for position, direction, and color
 */
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    // Operators
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
    Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vec3& operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    // Vector operations
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }

    Vec3 cross(const Vec3& other) const
    {
        return Vec3(y * other.z - z * other.y, z * other.x - x * other.z,
                    x * other.y - y * other.x);
    }

    float length() const { return std::sqrt(x * x + y * y + z * z); }

    float lengthSquared() const { return x * x + y * y + z * z; }

    Vec3 normalized() const
    {
        float len = length();
        if (len > 0.0f)
        {
            return *this / len;
        }
        return Vec3(0.0f, 0.0f, 0.0f);
    }

    void normalize()
    {
        float len = length();
        if (len > 0.0f)
        {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    // Static helpers
    static Vec3 zero() { return Vec3(0.0f, 0.0f, 0.0f); }
    static Vec3 up() { return Vec3(0.0f, 1.0f, 0.0f); }
    static Vec3 forward() { return Vec3(0.0f, 0.0f, -1.0f); }
    static Vec3 right() { return Vec3(1.0f, 0.0f, 0.0f); }
};

inline Vec3 operator*(float scalar, const Vec3& v)
{
    return v * scalar;
}

/**
 * 4D Vector for homogeneous coordinates
 */
struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Vec4() = default;
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec3 xyz() const { return Vec3(x, y, z); }

    // Perspective divide
    Vec3 perspectiveDivide() const
    {
        if (std::abs(w) > 0.0001f)
        {
            return Vec3(x / w, y / w, z / w);
        }
        return Vec3(x, y, z);
    }
};

/**
 * 4x4 Matrix for transformations (column-major order)
 * Layout: m[column][row] or m[column * 4 + row]
 */
struct Mat4
{
    std::array<float, 16> m = {
        1.0f, 0.0f, 0.0f, 0.0f, // Column 0
        0.0f, 1.0f, 0.0f, 0.0f, // Column 1
        0.0f, 0.0f, 1.0f, 0.0f, // Column 2
        0.0f, 0.0f, 0.0f, 1.0f  // Column 3
    };

    Mat4() = default;

    // Access element at (row, col)
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }

    // Matrix multiplication
    Mat4 operator*(const Mat4& other) const
    {
        Mat4 result;
        for (int col = 0; col < 4; col++)
        {
            for (int row = 0; row < 4; row++)
            {
                result.at(row, col) = at(row, 0) * other.at(0, col) +
                                      at(row, 1) * other.at(1, col) +
                                      at(row, 2) * other.at(2, col) + at(row, 3) * other.at(3, col);
            }
        }
        return result;
    }

    // Transform a Vec4
    Vec4 operator*(const Vec4& v) const
    {
        return Vec4(at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z + at(0, 3) * v.w,
                    at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z + at(1, 3) * v.w,
                    at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z + at(2, 3) * v.w,
                    at(3, 0) * v.x + at(3, 1) * v.y + at(3, 2) * v.z + at(3, 3) * v.w);
    }

    // Transform a Vec3 (as position with w=1)
    Vec3 transformPoint(const Vec3& v) const
    {
        Vec4 result = *this * Vec4(v, 1.0f);
        return result.perspectiveDivide();
    }

    // Transform a Vec3 (as direction with w=0)
    Vec3 transformDirection(const Vec3& v) const
    {
        return Vec3(at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z,
                    at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z,
                    at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z);
    }

    // Static factory methods

    // Identity matrix
    static Mat4 identity() { return Mat4(); }

    // Translation matrix
    static Mat4 translation(float x, float y, float z)
    {
        Mat4 result;
        result.at(0, 3) = x;
        result.at(1, 3) = y;
        result.at(2, 3) = z;
        return result;
    }

    static Mat4 translation(const Vec3& v) { return translation(v.x, v.y, v.z); }

    // Scale matrix
    static Mat4 scale(float x, float y, float z)
    {
        Mat4 result;
        result.at(0, 0) = x;
        result.at(1, 1) = y;
        result.at(2, 2) = z;
        return result;
    }

    static Mat4 scale(float s) { return scale(s, s, s); }

    // Rotation around X axis (angle in radians)
    static Mat4 rotationX(float angle)
    {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Mat4 result;
        result.at(1, 1) = c;
        result.at(1, 2) = -s;
        result.at(2, 1) = s;
        result.at(2, 2) = c;
        return result;
    }

    // Rotation around Y axis (angle in radians)
    static Mat4 rotationY(float angle)
    {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Mat4 result;
        result.at(0, 0) = c;
        result.at(0, 2) = s;
        result.at(2, 0) = -s;
        result.at(2, 2) = c;
        return result;
    }

    // Rotation around Z axis (angle in radians)
    static Mat4 rotationZ(float angle)
    {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Mat4 result;
        result.at(0, 0) = c;
        result.at(0, 1) = -s;
        result.at(1, 0) = s;
        result.at(1, 1) = c;
        return result;
    }

    // Perspective projection matrix
    static Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane)
    {
        float tanHalfFov = std::tan(fovYRadians / 2.0f);
        Mat4 result;
        result.m.fill(0.0f);

        result.at(0, 0) = 1.0f / (aspect * tanHalfFov);
        result.at(1, 1) = 1.0f / tanHalfFov;
        result.at(2, 2) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        result.at(2, 3) = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
        result.at(3, 2) = -1.0f;

        return result;
    }

    // Orthographic projection matrix
    static Mat4 orthographic(float left, float right, float bottom, float top, float nearPlane,
                             float farPlane)
    {
        Mat4 result;
        result.m.fill(0.0f);

        result.at(0, 0) = 2.0f / (right - left);
        result.at(1, 1) = 2.0f / (top - bottom);
        result.at(2, 2) = -2.0f / (farPlane - nearPlane);
        result.at(0, 3) = -(right + left) / (right - left);
        result.at(1, 3) = -(top + bottom) / (top - bottom);
        result.at(2, 3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        result.at(3, 3) = 1.0f;

        return result;
    }

    // Look-at view matrix
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
    {
        Vec3 forward = (target - eye).normalized();
        Vec3 right = forward.cross(up).normalized();
        Vec3 newUp = right.cross(forward);

        Mat4 result;
        result.at(0, 0) = right.x;
        result.at(0, 1) = right.y;
        result.at(0, 2) = right.z;
        result.at(0, 3) = -right.dot(eye);

        result.at(1, 0) = newUp.x;
        result.at(1, 1) = newUp.y;
        result.at(1, 2) = newUp.z;
        result.at(1, 3) = -newUp.dot(eye);

        result.at(2, 0) = -forward.x;
        result.at(2, 1) = -forward.y;
        result.at(2, 2) = -forward.z;
        result.at(2, 3) = forward.dot(eye);

        result.at(3, 0) = 0.0f;
        result.at(3, 1) = 0.0f;
        result.at(3, 2) = 0.0f;
        result.at(3, 3) = 1.0f;

        return result;
    }
};

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

inline float radians(float degrees)
{
    return degrees * DEG_TO_RAD;
}
inline float degrees(float radians)
{
    return radians * RAD_TO_DEG;
}

} // namespace runeharbor::graphics
