// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

#include "../formats/odm_map.hpp"

namespace runeharbor::engine
{

inline float sampleOutdoorTerrainHeight(const formats::ODMMapData& odmData, float worldX,
                                        float worldY)
{
    if (odmData.heightmap.empty())
    {
        return 0.0f;
    }

    constexpr float kCellSize = 512.0f;
    constexpr float kHalfTerrain = formats::ODMMapData::TERRAIN_SIZE / 2.0f;

    const float gridX = (worldX / kCellSize) + kHalfTerrain;
    const float gridY = (worldY / kCellSize) + kHalfTerrain;

    const int x0 = std::clamp(static_cast<int>(gridX), 0, formats::ODMMapData::TERRAIN_SIZE - 1);
    const int y0 = std::clamp(static_cast<int>(gridY), 0, formats::ODMMapData::TERRAIN_SIZE - 1);
    const int x1 = std::clamp(x0 + 1, 0, formats::ODMMapData::TERRAIN_SIZE - 1);
    const int y1 = std::clamp(y0 + 1, 0, formats::ODMMapData::TERRAIN_SIZE - 1);

    const float fx = std::clamp(gridX - static_cast<float>(x0), 0.0f, 1.0f);
    const float fy = std::clamp(gridY - static_cast<float>(y0), 0.0f, 1.0f);

    const auto sample = [&](int gx, int gy) -> float
    {
        const size_t index = static_cast<size_t>(gy * formats::ODMMapData::TERRAIN_SIZE + gx);
        if (index >= odmData.heightmap.size())
        {
            return 0.0f;
        }
        return static_cast<float>(odmData.heightmap[index].height);
    };

    const float h00 = sample(x0, y0);
    const float h10 = sample(x1, y0);
    const float h01 = sample(x0, y1);
    const float h11 = sample(x1, y1);

    const float h0 = h00 + fx * (h10 - h00);
    const float h1 = h01 + fx * (h11 - h01);
    return h0 + fy * (h1 - h0);
}

// Clamp the party's Z so it can never sit below the terrain floor.
// Above-ground Z (jumping, falling, freshly loaded save) is preserved;
// sub-terrain Z gets snapped up to the floor.
inline float clampOutdoorPartyZ(const formats::ODMMapData& odmData, float worldX, float worldY,
                                float currentZ)
{
    if (odmData.heightmap.empty())
    {
        return currentZ;
    }

    const float floor = sampleOutdoorTerrainHeight(odmData, worldX, worldY);
    return std::max(currentZ, floor);
}

} // namespace runeharbor::engine
