// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

#include "math3d.hpp"

namespace runeharbor::graphics
{

/// A texture coordinate pair for a billboard corner.
struct BillboardUV
{
    float u = 0.0f;
    float v = 0.0f;
};

/// A screen-facing (Y-up) billboard quad expressed in world space.
///
/// Corners are anchored at the base (feet) position and extend upward, matching
/// the MM7 convention where a sprite stands on the ground at `basePos`. Corner
/// order is bottom-left, bottom-right, top-left, top-right so a {0,1,2, 2,1,3}
/// index list forms two triangles.
struct BillboardQuad
{
    Vec3 bottomLeft;
    Vec3 bottomRight;
    Vec3 topLeft;
    Vec3 topRight;

    BillboardUV uvBottomLeft{0.0f, 1.0f};
    BillboardUV uvBottomRight{1.0f, 1.0f};
    BillboardUV uvTopLeft{0.0f, 0.0f};
    BillboardUV uvTopRight{1.0f, 0.0f};
};

struct BillboardDimensions
{
    float halfWidth = 0.0f;
    float height = 0.0f;
};

/// Resolve a billboard's final dimensions while preserving gameplay height modifiers.
inline BillboardDimensions resolveBillboardDimensions(float fallbackHalfWidth, float fallbackHeight,
                                                      float textureWidth, float textureHeight,
                                                      float textureScale, float heightScale = 1.0f)
{
    BillboardDimensions result{fallbackHalfWidth, fallbackHeight};
    if (textureWidth > 0.0f && textureHeight > 0.0f)
    {
        result.halfWidth = textureWidth * textureScale * 0.5f;
        result.height = textureHeight * textureScale;
    }
    result.height *= std::max(0.0f, heightScale);
    return result;
}

/// Build a screen-facing billboard quad.
///
/// The quad yaws to face the camera around the world up axis (Y), so it stays
/// vertical regardless of camera pitch. `halfWidth` is half the quad's width and
/// `height` is its full height; both are in world units. When `flipU` is true the
/// horizontal texture coordinates are mirrored — used for the MM7
/// 5-frames-to-8-facings scheme where the mirrored octants reuse a flipped frame.
inline BillboardQuad computeBillboardQuad(const Vec3& basePos, const Vec3& cameraPos,
                                          float halfWidth, float height, bool flipU = false)
{
    // Camera-relative facing, flattened to the ground plane so the billboard
    // stays upright. Fall back to a fixed forward/right basis when the camera is
    // directly above the sprite (degenerate horizontal direction).
    Vec3 toCamera = cameraPos - basePos;
    toCamera.y = 0.0f;
    if (toCamera.lengthSquared() < 0.001f)
    {
        toCamera = Vec3::forward();
    }
    const Vec3 forward = toCamera.normalized();

    Vec3 right = Vec3::up().cross(forward);
    if (right.lengthSquared() < 0.001f)
    {
        right = Vec3::right();
    }
    right.normalize();

    const Vec3 up = Vec3::up();

    BillboardQuad quad;
    quad.bottomLeft = basePos - right * halfWidth;
    quad.bottomRight = basePos + right * halfWidth;
    quad.topLeft = quad.bottomLeft + up * height;
    quad.topRight = quad.bottomRight + up * height;

    const float uLeft = flipU ? 1.0f : 0.0f;
    const float uRight = flipU ? 0.0f : 1.0f;
    quad.uvBottomLeft = {uLeft, 1.0f};
    quad.uvBottomRight = {uRight, 1.0f};
    quad.uvTopLeft = {uLeft, 0.0f};
    quad.uvTopRight = {uRight, 0.0f};
    return quad;
}

} // namespace runeharbor::graphics
