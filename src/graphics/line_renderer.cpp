// SPDX-License-Identifier: MIT
#include "line_renderer.hpp"

#include <SDL3/SDL.h>

#include <cmath>

#include "clip_utils.hpp"

namespace runeharbor::graphics
{

namespace
{
constexpr int kOutLeft = 1 << 0;
constexpr int kOutRight = 1 << 1;
constexpr int kOutBottom = 1 << 2;
constexpr int kOutTop = 1 << 3;

int computeNdcOutCode(float x, float y)
{
    int code = 0;
    if (x < -1.0f)
    {
        code |= kOutLeft;
    }
    else if (x > 1.0f)
    {
        code |= kOutRight;
    }
    if (y < -1.0f)
    {
        code |= kOutBottom;
    }
    else if (y > 1.0f)
    {
        code |= kOutTop;
    }
    return code;
}

bool clipLineToNdcRect(float& x0, float& y0, float& x1, float& y1)
{
    int out0 = computeNdcOutCode(x0, y0);
    int out1 = computeNdcOutCode(x1, y1);

    for (int guard = 0; guard < 16; guard++)
    {
        if ((out0 | out1) == 0)
        {
            return true;
        }
        if ((out0 & out1) != 0)
        {
            return false;
        }

        const int out = (out0 != 0) ? out0 : out1;
        float x = 0.0f;
        float y = 0.0f;

        if (out & kOutTop)
        {
            const float dy = y1 - y0;
            if (std::abs(dy) < 1e-6f)
            {
                return false;
            }
            x = x0 + (x1 - x0) * ((1.0f - y0) / dy);
            y = 1.0f;
        }
        else if (out & kOutBottom)
        {
            const float dy = y1 - y0;
            if (std::abs(dy) < 1e-6f)
            {
                return false;
            }
            x = x0 + (x1 - x0) * ((-1.0f - y0) / dy);
            y = -1.0f;
        }
        else if (out & kOutRight)
        {
            const float dx = x1 - x0;
            if (std::abs(dx) < 1e-6f)
            {
                return false;
            }
            y = y0 + (y1 - y0) * ((1.0f - x0) / dx);
            x = 1.0f;
        }
        else
        {
            const float dx = x1 - x0;
            if (std::abs(dx) < 1e-6f)
            {
                return false;
            }
            y = y0 + (y1 - y0) * ((-1.0f - x0) / dx);
            x = -1.0f;
        }

        if (out == out0)
        {
            x0 = x;
            y0 = y;
            out0 = computeNdcOutCode(x0, y0);
        }
        else
        {
            x1 = x;
            y1 = y;
            out1 = computeNdcOutCode(x1, y1);
        }
    }

    return false;
}
} // namespace

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

    Vec4 clippedA;
    Vec4 clippedB;
    if (!clipLineNearPlane(clipA, clipB, clippedA, clippedB))
    {
        return false;
    }

    // Perspective divide
    float ndcAx = clippedA.x / clippedA.w;
    float ndcAy = clippedA.y / clippedA.w;
    float ndcBx = clippedB.x / clippedB.w;
    float ndcBy = clippedB.y / clippedB.w;

    if (!std::isfinite(ndcAx) || !std::isfinite(ndcAy) || !std::isfinite(ndcBx) ||
        !std::isfinite(ndcBy))
    {
        return false;
    }
    if (!clipLineToNdcRect(ndcAx, ndcAy, ndcBx, ndcBy))
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
