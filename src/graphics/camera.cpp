// SPDX-License-Identifier: MIT
#include "camera.hpp"

#include <algorithm>

#include <cmath>

namespace runeharbor::graphics
{

Camera::Camera()
{
    updatePosition();
}

void Camera::orbit(float deltaYaw, float deltaPitch)
{
    yaw += deltaYaw;
    pitch += deltaPitch;

    // Clamp pitch to avoid gimbal lock
    pitch = std::clamp(pitch, MIN_PITCH, MAX_PITCH);

    // Keep yaw in reasonable range
    while (yaw > PI)
        yaw -= 2.0f * PI;
    while (yaw < -PI)
        yaw += 2.0f * PI;
}

void Camera::zoom(float delta)
{
    distance -= delta;
    distance = std::clamp(distance, MIN_DISTANCE, MAX_DISTANCE);

    // Also scale ortho size proportionally
    if (orthographic)
    {
        orthoSize = distance * 0.5f;
    }
}

void Camera::pan(float deltaX, float deltaY)
{
    // Calculate right and up vectors in world space
    float cy = std::cos(yaw);
    float sy = std::sin(yaw);
    float cp = std::cos(pitch);
    float sp = std::sin(pitch);

    // Right vector (perpendicular to view direction in XZ plane)
    Vec3 right(cy, 0.0f, -sy);

    // Up vector (perpendicular to right and forward)
    Vec3 up(-sy * sp, cp, -cy * sp);

    // Scale pan by distance for consistent feel
    float panScale = distance * 0.001f;
    target += right * (deltaX * panScale);
    target += up * (deltaY * panScale);
}

void Camera::lookAt(const Vec3& newTarget, float newDistance)
{
    target = newTarget;
    distance = std::clamp(newDistance, MIN_DISTANCE, MAX_DISTANCE);
}

void Camera::setPosition(const Vec3& position)
{
    // Calculate distance and angles from position to target
    Vec3 dir = position - target;
    distance = dir.length();

    if (distance > 0.0001f)
    {
        // Calculate yaw (horizontal angle)
        yaw = std::atan2(-dir.x, -dir.z);

        // Calculate pitch (vertical angle)
        pitch = std::asin(dir.y / distance);
        pitch = std::clamp(pitch, MIN_PITCH, MAX_PITCH);
    }

    distance = std::clamp(distance, MIN_DISTANCE, MAX_DISTANCE);
}

void Camera::setTarget(const Vec3& newTarget)
{
    target = newTarget;
}

void Camera::setPerspective(float fovDegrees, float aspect, float near, float far)
{
    fov = fovDegrees;
    aspectRatio = aspect;
    nearPlane = near;
    farPlane = far;
    orthographic = false;
}

void Camera::setOrthographic(float size, float aspect, float near, float far)
{
    orthoSize = size;
    aspectRatio = aspect;
    nearPlane = near;
    farPlane = far;
    orthographic = true;
}

void Camera::setOrthographicMode(bool ortho)
{
    orthographic = ortho;
    if (ortho)
    {
        // Set ortho size based on current distance
        orthoSize = distance * 0.5f;
    }
}

Mat4 Camera::getViewMatrix() const
{
    Vec3 position = getPosition();
    return Mat4::lookAt(position, target, Vec3::up());
}

Mat4 Camera::getProjectionMatrix() const
{
    if (orthographic)
    {
        float halfWidth = orthoSize * aspectRatio;
        float halfHeight = orthoSize;
        return Mat4::orthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane,
                                  farPlane);
    }
    else
    {
        return Mat4::perspective(radians(fov), aspectRatio, nearPlane, farPlane);
    }
}

Mat4 Camera::getViewProjectionMatrix() const
{
    return getProjectionMatrix() * getViewMatrix();
}

Vec3 Camera::getPosition() const
{
    // Calculate position from orbit parameters
    float cy = std::cos(yaw);
    float sy = std::sin(yaw);
    float cp = std::cos(pitch);
    float sp = std::sin(pitch);

    Vec3 offset(distance * cp * sy, distance * sp, distance * cp * cy);

    return target + offset;
}

void Camera::setAspectRatio(float aspect)
{
    aspectRatio = aspect;
}

void Camera::reset()
{
    target = Vec3(0.0f, 0.0f, 0.0f);
    distance = 1000.0f;
    yaw = 0.0f;
    pitch = 0.5f;
    orthographic = false;
}

void Camera::updatePosition()
{
    // Position is calculated on-demand in getPosition()
}

} // namespace runeharbor::graphics
