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

// Software Tessellation to fix affine texture warping in SDL_RenderGeometry.
// Recursively subdivides a clip-space triangle if its depth (W) ratio is too high.
// Linearly interpolating in clip space preserves perspective-correct UVs!
inline void tessellateTriangle(const ClipVertex& v0, const ClipVertex& v1, const ClipVertex& v2,
                               std::vector<ClipVertex>& outVerts, int depth = 0)
{
    // Max depth to prevent infinite loops/excessive geometry
    const int MAX_SUBDIV = 3;

    // We subdivide if the ratio of max_w to min_w is too large (meaning heavy perspective)
    float minW = std::min({v0.clip.w, v1.clip.w, v2.clip.w});
    float maxW = std::max({v0.clip.w, v1.clip.w, v2.clip.w});

    // A ratio of 1.5x depth across a single triangle is where affine wobble gets very noticeable
    bool needsSubdiv = (maxW / std::max(minW, 0.001f) > 1.4f);

    if (depth >= MAX_SUBDIV || !needsSubdiv)
    {
        outVerts.push_back(v0);
        outVerts.push_back(v1);
        outVerts.push_back(v2);
        return;
    }

    // Subdivide into 4 smaller triangles by splitting the edges
    auto mid = [](const ClipVertex& a, const ClipVertex& b) -> ClipVertex
    {
        ClipVertex m;
        m.clip.x = (a.clip.x + b.clip.x) * 0.5f;
        m.clip.y = (a.clip.y + b.clip.y) * 0.5f;
        m.clip.z = (a.clip.z + b.clip.z) * 0.5f;
        m.clip.w = (a.clip.w + b.clip.w) * 0.5f;
        m.u = (a.u + b.u) * 0.5f;
        m.v = (a.v + b.v) * 0.5f;
        m.color.r = (a.color.r + b.color.r) * 0.5f;
        m.color.g = (a.color.g + b.color.g) * 0.5f;
        m.color.b = (a.color.b + b.color.b) * 0.5f;
        m.color.a = (a.color.a + b.color.a) * 0.5f;
        return m;
    };

    ClipVertex m01 = mid(v0, v1);
    ClipVertex m12 = mid(v1, v2);
    ClipVertex m20 = mid(v2, v0);

    tessellateTriangle(v0, m01, m20, outVerts, depth + 1);
    tessellateTriangle(v1, m12, m01, outVerts, depth + 1);
    tessellateTriangle(v2, m20, m12, outVerts, depth + 1);
    tessellateTriangle(m01, m12, m20, outVerts, depth + 1);
}

} // namespace runeharbor::graphics
