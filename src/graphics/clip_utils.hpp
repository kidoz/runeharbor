// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL.h>

#include "math3d.hpp"

namespace runeharbor::graphics
{

/// Near-plane epsilon in clip space (w threshold).
/// Matches the value used in line_renderer.cpp.
constexpr float CLIP_NEAR_EPSILON = 0.1f;

/// A vertex in clip space, carrying color and texture coordinates for interpolation
/// during near-plane clipping.
struct ClipVertex
{
    Vec4 clip;
    SDL_FColor color;
    float u = 0.0f;
    float v = 0.0f;
};

/// Maximum polygon vertices after clipping (original + 1 per clip plane).
constexpr int MAX_CLIP_VERTS = 24;

/// Project a clip-space point to screen coordinates.
/// Returns false if the point is behind the near plane (w < CLIP_NEAR_EPSILON).
inline bool projectClipToScreen(const Vec4& clip, float vpW, float vpH, float& sx, float& sy)
{
    if (clip.w < CLIP_NEAR_EPSILON)
    {
        return false;
    }

    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;

    // NDC (-1..1) to screen. NDC Y is up, screen Y is down.
    sx = (ndcX + 1.0f) * 0.5f * vpW;
    sy = (1.0f - ndcY) * 0.5f * vpH;
    return true;
}

/// Sutherland-Hodgman clip of a convex polygon against the near plane (w >= CLIP_NEAR_EPSILON).
/// Input: `in[0..inCount-1]`.  Output written to `out`, returns output vertex count.
/// Works in-place if out == in as long as a temporary is used internally.
inline int clipPolygonNearPlane(const ClipVertex* in, int inCount, ClipVertex* out)
{
    if (inCount < 1 || inCount > MAX_CLIP_VERTS)
    {
        return 0;
    }

    int outCount = 0;

    for (int i = 0; i < inCount; i++)
    {
        const ClipVertex& cur = in[i];
        const ClipVertex& prev = in[(i + inCount - 1) % inCount];

        bool curInside = cur.clip.w >= CLIP_NEAR_EPSILON;
        bool prevInside = prev.clip.w >= CLIP_NEAR_EPSILON;

        if (curInside != prevInside)
        {
            // Edge crosses the near plane — emit intersection
            float t = (CLIP_NEAR_EPSILON - prev.clip.w) / (cur.clip.w - prev.clip.w);

            ClipVertex intersection;
            intersection.clip.x = prev.clip.x + t * (cur.clip.x - prev.clip.x);
            intersection.clip.y = prev.clip.y + t * (cur.clip.y - prev.clip.y);
            intersection.clip.z = prev.clip.z + t * (cur.clip.z - prev.clip.z);
            intersection.clip.w = CLIP_NEAR_EPSILON;
            intersection.color.r = prev.color.r + t * (cur.color.r - prev.color.r);
            intersection.color.g = prev.color.g + t * (cur.color.g - prev.color.g);
            intersection.color.b = prev.color.b + t * (cur.color.b - prev.color.b);
            intersection.color.a = prev.color.a + t * (cur.color.a - prev.color.a);
            intersection.u = prev.u + t * (cur.u - prev.u);
            intersection.v = prev.v + t * (cur.v - prev.v);

            if (outCount < MAX_CLIP_VERTS)
            {
                out[outCount++] = intersection;
            }
        }

        if (curInside)
        {
            if (outCount < MAX_CLIP_VERTS)
            {
                out[outCount++] = cur;
            }
        }
    }

    return outCount;
}

/// Clip a line segment in clip space against the near plane (w >= CLIP_NEAR_EPSILON).
/// Returns false if the whole segment lies behind the near plane.
inline bool clipLineNearPlane(const Vec4& inA, const Vec4& inB, Vec4& outA, Vec4& outB)
{
    const bool aInside = inA.w >= CLIP_NEAR_EPSILON;
    const bool bInside = inB.w >= CLIP_NEAR_EPSILON;

    if (!aInside && !bInside)
    {
        return false;
    }

    outA = inA;
    outB = inB;

    if (aInside && bInside)
    {
        return true;
    }

    const float denom = inB.w - inA.w;
    if (std::abs(denom) < 1e-6f)
    {
        return false;
    }

    const float t = (CLIP_NEAR_EPSILON - inA.w) / denom;
    Vec4 i;
    i.x = inA.x + t * (inB.x - inA.x);
    i.y = inA.y + t * (inB.y - inA.y);
    i.z = inA.z + t * (inB.z - inA.z);
    i.w = CLIP_NEAR_EPSILON;

    if (!aInside)
    {
        outA = i;
    }
    else
    {
        outB = i;
    }

    return true;
}

} // namespace runeharbor::graphics
