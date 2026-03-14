// SPDX-License-Identifier: MIT
#pragma once

#include "math3d.hpp"

namespace runeharbor::graphics
{

// Gameplay/map coordinates use Z as elevation. The renderer uses Y-up.
inline Vec3 gameplayToRenderPosition(float x, float y, float z)
{
    return {x, z, y};
}

inline Vec3 gameplayToRenderDirection(float x, float y, float z)
{
    return {x, z, y};
}

} // namespace runeharbor::graphics
