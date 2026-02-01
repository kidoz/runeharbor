// SPDX-License-Identifier: MIT
#pragma once

#include "math3d.hpp"

namespace runeharbor::graphics
{

/**
 * 3D Camera with orbit, pan, and zoom controls
 *
 * The camera orbits around a target point (pivot) and can be controlled with:
 * - Orbit: Rotate around the target (yaw/pitch)
 * - Zoom: Move closer/farther from target
 * - Pan: Move the target point in the view plane
 */
class Camera
{
  public:
    Camera();

    // Camera control
    void orbit(float deltaYaw, float deltaPitch);
    void zoom(float delta);
    void pan(float deltaX, float deltaY);

    // Set camera to look at a point from a distance
    void lookAt(const Vec3& target, float distance);

    // Set camera position directly
    void setPosition(const Vec3& position);
    void setTarget(const Vec3& target);

    // Set projection parameters
    void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
    void setOrthographic(float size, float aspect, float nearPlane, float farPlane);

    // Toggle between perspective and orthographic
    void setOrthographicMode(bool ortho);
    bool isOrthographic() const { return orthographic; }

    // Get matrices
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix() const;
    Mat4 getViewProjectionMatrix() const;

    // Accessors
    Vec3 getPosition() const;
    Vec3 getTarget() const { return target; }
    float getDistance() const { return distance; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }

    // Set aspect ratio (call on window resize)
    void setAspectRatio(float aspect);

    // Reset to default view
    void reset();

  private:
    void updatePosition();

    // Orbit parameters
    Vec3 target{0.0f, 0.0f, 0.0f}; // Point camera orbits around
    float distance = 1000.0f;      // Distance from target
    float yaw = 0.0f;              // Horizontal rotation (radians)
    float pitch = 0.5f;            // Vertical rotation (radians)

    // Projection parameters
    float fov = 60.0f; // Field of view (degrees)
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 1.0f;
    float farPlane = 100000.0f;
    float orthoSize = 1000.0f; // Half-height for orthographic
    bool orthographic = false;

    // Limits
    static constexpr float MIN_PITCH = -1.5f; // Near -90 degrees
    static constexpr float MAX_PITCH = 1.5f;  // Near +90 degrees
    static constexpr float MIN_DISTANCE = 10.0f;
    static constexpr float MAX_DISTANCE = 50000.0f;
};

} // namespace runeharbor::graphics
