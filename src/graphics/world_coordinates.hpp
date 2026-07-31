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

/// Inverse of gameplayToRenderPosition. The mapping swaps Y and Z, so it is its
/// own inverse; this overload exists to make intent readable at call sites that
/// hand a render-space camera to gameplay-space map data.
inline Vec3 renderToGameplayPosition(const Vec3& renderPos)
{
    return {renderPos.x, renderPos.z, renderPos.y};
}

} // namespace runeharbor::graphics
