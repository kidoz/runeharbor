// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace runeharbor::graphics
{

/**
 * 2D Rectangle structure
 */
struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    Rect() = default;
    Rect(int x, int y, int w, int h) : x(x), y(y), width(w), height(h) {}

    bool contains(int px, int py) const
    {
        return px >= x && px < (x + width) && py >= y && py < (y + height);
    }
};

} // namespace runeharbor::graphics
