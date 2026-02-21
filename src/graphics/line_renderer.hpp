// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "../util/ilogger.hpp"
#include "math3d.hpp"

// Forward declare SDL types
struct SDL_Renderer;

namespace runeharbor::graphics
{

/**
 * Wireframe renderer for 3D geometry using SDL3 line primitives
 *
 * Transforms 3D lines through view-projection matrix and renders
 * to SDL renderer as 2D lines. Includes basic frustum culling.
 */
class LineRenderer
{
  public:
    LineRenderer(SDL_Renderer* renderer, util::ILogger& logger);

    // Set viewport size (call on window resize)
    void setViewport(int width, int height);

    // Set view-projection matrix for transformations
    void setViewProjection(const Mat4& viewProjection);

    // Clear line buffer (call at start of frame)
    void clear();

    // Add a 3D line to render
    void drawLine3D(const Vec3& a, const Vec3& b, uint8_t r, uint8_t g, uint8_t b_color);

    // Draw a 3D axis indicator at a position
    void drawAxes(const Vec3& origin, float size);

    // Draw a 3D point as a small cross
    void drawPoint3D(const Vec3& p, float size, uint8_t r, uint8_t g, uint8_t b);

    // Draw a grid on XY plane (Z=0)
    void drawGrid(float size, float spacing, uint8_t r, uint8_t g, uint8_t b_color);

    // Render all buffered lines (call at end of frame)
    void render();

    // Number of buffered projected lines (for debug/tests)
    int bufferedLineCount() const { return static_cast<int>(lines.size()); }

  private:
    // Transform 3D point to screen coordinates
    // Returns false if point is behind camera
    bool projectToScreen(const Vec3& worldPos, float& screenX, float& screenY) const;

    // Clip line against near plane and project
    bool clipAndProject(const Vec3& a, const Vec3& b, float& x1, float& y1, float& x2,
                        float& y2) const;

    struct Line2D
    {
        float x1, y1, x2, y2;
        uint8_t r, g, b;
    };

    SDL_Renderer* renderer = nullptr;
    [[maybe_unused]] util::ILogger& logger;

    Mat4 viewProjection;
    int viewportWidth = 800;
    int viewportHeight = 600;

    std::vector<Line2D> lines;
};

} // namespace runeharbor::graphics
