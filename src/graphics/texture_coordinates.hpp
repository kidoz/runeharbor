// SPDX-License-Identifier: MIT
#pragma once

#include <numbers>

#include <cmath>

#include "../formats/blv_map.hpp"

namespace runeharbor::graphics
{

/// Extent assumed when a texture's real size is unknown.
constexpr float DEFAULT_TEXTURE_EXTENT = 256.0f;

inline float effectiveTextureExtent(float textureExtent)
{
    return textureExtent > 0.0f ? textureExtent : DEFAULT_TEXTURE_EXTENT;
}

/// Convert a texel-space face coordinate and its face offset to normalized UV space.
inline float normalizeTextureCoordinate(float coordinate, float offset, float textureExtent)
{
    return (coordinate + offset) / effectiveTextureExtent(textureExtent);
}

/// Texel-space scroll offset added to a face's texture coordinates.
struct TextureFlow
{
    float u = 0.0f;
    float v = 0.0f;
};

/// How a face's texture scrolls, derived from its FACE_Flow*/lava attributes.
struct TextureFlowDirection
{
    bool down = false;
    bool up = false;
    bool left = false;
    bool right = false;
    bool lava = false;
    /// True when the face is (nearly) horizontal — its gameplay-space normal
    /// points up or down. The vertical scroll direction is inverted there.
    bool horizontalSurface = false;

    bool any() const { return down || up || left || right || lava; }
};

/// Texels the flow advances per second. The original engine feeds the texture
/// offset `realtimeMilliseconds >> 4` texels, i.e. one texel per 16 ms.
constexpr float TEXTURE_FLOW_TEXELS_PER_SECOND = 1000.0f / 16.0f;

/// Seconds for a lava surface to complete one back-and-forth sweep.
constexpr float LAVA_FLOW_PERIOD_SECONDS = 8.0f;

/// Normal Z magnitude above which a face counts as horizontal (floor/ceiling).
constexpr float HORIZONTAL_SURFACE_NORMAL_Z = 0.9f;

inline TextureFlowDirection faceTextureFlowDirection(const formats::ParsedFace& face)
{
    TextureFlowDirection direction;
    direction.down = face.flowsDown();
    direction.up = face.flowsUp();
    direction.left = face.flowsLeft();
    direction.right = face.flowsRight();
    direction.lava = face.isLava();
    direction.horizontalSurface = std::abs(face.normalFZ) >= HORIZONTAL_SURFACE_NORMAL_Z;
    return direction;
}

/// Scroll offset in texels for a flowing face. The offset wraps modulo the
/// texture extent so it stays bounded however long the map has been loaded —
/// an ever-growing offset loses float precision and makes the texture drift.
inline TextureFlow textureFlowOffset(const TextureFlowDirection& direction, float timeSeconds,
                                     float textureWidth, float textureHeight)
{
    const float width = effectiveTextureExtent(textureWidth);
    const float height = effectiveTextureExtent(textureHeight);

    TextureFlow flow;
    if (direction.lava)
    {
        // Lava sweeps back and forth over a fixed period instead of scrolling.
        const float phase =
            std::fmod(timeSeconds, LAVA_FLOW_PERIOD_SECONDS) / LAVA_FLOW_PERIOD_SECONDS;
        flow.v = height * std::sin(phase * 2.0f * std::numbers::pi_v<float>);
        return flow;
    }

    const float advance = timeSeconds * TEXTURE_FLOW_TEXELS_PER_SECOND;
    if (direction.left)
    {
        flow.u = -std::fmod(advance, width);
    }
    else if (direction.right)
    {
        flow.u = std::fmod(advance, width);
    }

    // A horizontal surface is seen from the other side of its texture plane, so
    // "down" scrolls the opposite way there than it does on a wall.
    const float verticalSign = direction.horizontalSurface ? 1.0f : -1.0f;
    if (direction.down)
    {
        flow.v = verticalSign * std::fmod(advance, height);
    }
    else if (direction.up)
    {
        flow.v = -verticalSign * std::fmod(advance, height);
    }
    return flow;
}

} // namespace runeharbor::graphics
