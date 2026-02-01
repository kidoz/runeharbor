// SPDX-License-Identifier: MIT
#include "line_renderer.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace runeharbor::graphics
{

LineRenderer::LineRenderer(SDL_Renderer* renderer, util::ILogger& logger)
    : renderer(renderer), logger(logger)
{
    lines.reserve(10000); // Pre-allocate for typical map
}

void LineRenderer::setViewport(int width, int height)
{
    viewportWidth = width;
    viewportHeight = height;
}

void LineRenderer::setViewProjection(const Mat4& vp)
{
    viewProjection = vp;
}

void LineRenderer::clear()
{
    lines.clear();
}

void LineRenderer::drawLine3D(const Vec3& a, const Vec3& b, uint8_t r, uint8_t g, uint8_t b_color)
{
    float x1, y1, x2, y2;
    if (clipAndProject(a, b, x1, y1, x2, y2))
    {
        lines.push_back({x1, y1, x2, y2, r, g, b_color});
    }
}

void LineRenderer::drawAxes(const Vec3& origin, float size)
{
    // X axis (red)
    drawLine3D(origin, origin + Vec3(size, 0, 0), 255, 0, 0);
    // Y axis (green)
    drawLine3D(origin, origin + Vec3(0, size, 0), 0, 255, 0);
    // Z axis (blue)
    drawLine3D(origin, origin + Vec3(0, 0, size), 0, 0, 255);
}

void LineRenderer::drawPoint3D(const Vec3& p, float size, uint8_t r, uint8_t g, uint8_t b)
{
    drawLine3D(p - Vec3(size, 0, 0), p + Vec3(size, 0, 0), r, g, b);
    drawLine3D(p - Vec3(0, size, 0), p + Vec3(0, size, 0), r, g, b);
    drawLine3D(p - Vec3(0, 0, size), p + Vec3(0, 0, size), r, g, b);
}

void LineRenderer::drawGrid(float size, float spacing, uint8_t r, uint8_t g, uint8_t b_color)
{
    int numLines = static_cast<int>(size / spacing);

    // Lines parallel to X axis
    for (int i = -numLines; i <= numLines; i++)
    {
        float z = i * spacing;
        drawLine3D(Vec3(-size, 0, z), Vec3(size, 0, z), r, g, b_color);
    }

    // Lines parallel to Z axis
    for (int i = -numLines; i <= numLines; i++)
    {
        float x = i * spacing;
        drawLine3D(Vec3(x, 0, -size), Vec3(x, 0, size), r, g, b_color);
    }
}

void LineRenderer::render()
{
    if (!renderer || lines.empty())
    {
        return;
    }

    // Render all lines
    for (const auto& line : lines)
    {
        SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
        SDL_RenderLine(renderer, line.x1, line.y1, line.x2, line.y2);
    }
}

bool LineRenderer::projectToScreen(const Vec3& worldPos, float& screenX, float& screenY) const
{
    // Transform to clip space
    Vec4 clip = viewProjection * Vec4(worldPos, 1.0f);

    // Check if behind camera (w < 0 means behind)
    if (clip.w <= 0.0f)
    {
        return false;
    }

    // Perspective divide to NDC (-1 to 1)
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    float ndcZ = clip.z / clip.w;

    // Check if outside clip volume
    if (ndcZ < -1.0f || ndcZ > 1.0f)
    {
        // Allow some slack for lines that cross the near/far plane
        if (ndcZ < -2.0f || ndcZ > 2.0f)
        {
            return false;
        }
    }

    // NDC to screen coordinates
    // NDC Y is up, screen Y is down
    screenX = (ndcX + 1.0f) * 0.5f * static_cast<float>(viewportWidth);
    screenY = (1.0f - ndcY) * 0.5f * static_cast<float>(viewportHeight);

    return true;
}

bool LineRenderer::clipAndProject(const Vec3& a, const Vec3& b, float& x1, float& y1, float& x2,
                                  float& y2) const
{
    // Transform both endpoints to clip space
    Vec4 clipA = viewProjection * Vec4(a, 1.0f);
    Vec4 clipB = viewProjection * Vec4(b, 1.0f);

    // Near plane clipping (simplified - just check if both behind)
    constexpr float NEAR_EPSILON = 0.1f;

    if (clipA.w < NEAR_EPSILON && clipB.w < NEAR_EPSILON)
    {
        // Both behind camera
        return false;
    }

    // If one point is behind camera, clip to near plane
    Vec4 clippedA = clipA;
    Vec4 clippedB = clipB;

    if (clipA.w < NEAR_EPSILON)
    {
        // A is behind, clip towards B
        float t = (NEAR_EPSILON - clipA.w) / (clipB.w - clipA.w);
        clippedA.x = clipA.x + t * (clipB.x - clipA.x);
        clippedA.y = clipA.y + t * (clipB.y - clipA.y);
        clippedA.z = clipA.z + t * (clipB.z - clipA.z);
        clippedA.w = NEAR_EPSILON;
    }
    else if (clipB.w < NEAR_EPSILON)
    {
        // B is behind, clip towards A
        float t = (NEAR_EPSILON - clipB.w) / (clipA.w - clipB.w);
        clippedB.x = clipB.x + t * (clipA.x - clipB.x);
        clippedB.y = clipB.y + t * (clipA.y - clipB.y);
        clippedB.z = clipB.z + t * (clipA.z - clipB.z);
        clippedB.w = NEAR_EPSILON;
    }

    // Perspective divide
    float ndcAx = clippedA.x / clippedA.w;
    float ndcAy = clippedA.y / clippedA.w;
    float ndcBx = clippedB.x / clippedB.w;
    float ndcBy = clippedB.y / clippedB.w;

    // Quick reject if completely outside screen (with some margin)
    constexpr float MARGIN = 2.0f;
    if ((ndcAx < -MARGIN && ndcBx < -MARGIN) || (ndcAx > MARGIN && ndcBx > MARGIN) ||
        (ndcAy < -MARGIN && ndcBy < -MARGIN) || (ndcAy > MARGIN && ndcBy > MARGIN))
    {
        return false;
    }

    // NDC to screen coordinates
    float w = static_cast<float>(viewportWidth);
    float h = static_cast<float>(viewportHeight);

    x1 = (ndcAx + 1.0f) * 0.5f * w;
    y1 = (1.0f - ndcAy) * 0.5f * h;
    x2 = (ndcBx + 1.0f) * 0.5f * w;
    y2 = (1.0f - ndcBy) * 0.5f * h;

    return true;
}

} // namespace runeharbor::graphics
