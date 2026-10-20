// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

#include <cmath>

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

    const float gridX = formats::outdoorWorldToGridX(worldX);
    const float gridY = formats::outdoorWorldToGridY(worldY);

    const int x0 =
        std::clamp(static_cast<int>(std::floor(gridX)), 0, formats::ODMMapData::TERRAIN_SIZE - 1);
    const int y0 =
        std::clamp(static_cast<int>(std::floor(gridY)), 0, formats::ODMMapData::TERRAIN_SIZE - 1);
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

// Highest walkable surface (terrain or an outdoor model such as a dock, bridge,
// or building floor) at or below `maxZ`. The terrain-only sampler is not
// enough at places like the Emerald Island pier, where the party starts on a
// model floor above the water heightmap.
inline float sampleOutdoorFloorHeight(const formats::ODMMapData& odmData, float worldX,
                                      float worldY, float maxZ)
{
    float best = sampleOutdoorTerrainHeight(odmData, worldX, worldY);

    for (const auto& building : odmData.buildings)
    {
        // Building bounds are stored relative to the model anchor in the
        // shipped data; accept absolute bounds as a fallback for robustness.
        const float relMinX = static_cast<float>(building.worldX + building.minX);
        const float relMaxX = static_cast<float>(building.worldX + building.maxX);
        const float relMinY = static_cast<float>(building.worldY + building.minY);
        const float relMaxY = static_cast<float>(building.worldY + building.maxY);
        const float absMinX = static_cast<float>(building.minX);
        const float absMaxX = static_cast<float>(building.maxX);
        const float absMinY = static_cast<float>(building.minY);
        const float absMaxY = static_cast<float>(building.maxY);

        const bool inRel =
            worldX >= relMinX && worldX <= relMaxX && worldY >= relMinY && worldY <= relMaxY;
        const bool inAbs =
            worldX >= absMinX && worldX <= absMaxX && worldY >= absMinY && worldY <= absMaxY;
        if (!inRel && !inAbs)
        {
            continue;
        }

        for (const auto& face : building.faces)
        {
            if (face.isPortal() || face.isInvisible() || face.isWater() || face.isLava() ||
                face.normalFZ <= 0.5f || face.vertexIndices.size() < 3)
            {
                continue;
            }

            bool validIndices = true;
            for (const uint16_t index : face.vertexIndices)
            {
                if (index >= building.vertices.size())
                {
                    validIndices = false;
                    break;
                }
            }
            if (!validIndices)
            {
                continue;
            }

            const float planeZ =
                (face.normalFDist - face.normalFX * worldX - face.normalFY * worldY) /
                face.normalFZ;
            if (planeZ > maxZ || planeZ < best)
            {
                continue;
            }

            // 2D point-in-polygon in XY.
            bool inside = false;
            const size_t count = face.vertexIndices.size();
            for (size_t i = 0, j = count - 1; i < count; j = i++)
            {
                const auto& vi = building.vertices[face.vertexIndices[i]];
                const auto& vj = building.vertices[face.vertexIndices[j]];
                const float vyi = static_cast<float>(vi.y);
                const float vyj = static_cast<float>(vj.y);
                if (((vyi > worldY) != (vyj > worldY)) &&
                    (worldX < (static_cast<float>(vj.x) - static_cast<float>(vi.x)) *
                                      (worldY - vyi) / (vyj - vyi) +
                                  static_cast<float>(vi.x)))
                {
                    inside = !inside;
                }
            }

            if (inside)
            {
                best = planeZ;
            }
        }
    }

    return best;
}

// True when the terrain cell under the world position uses the water tileset.
// RuneHarbor has no depth/shoreline classification, so water cells are treated
// as impassable without Water Walk.
inline bool isOutdoorWaterAt(const formats::ODMMapData& odmData, float worldX, float worldY)
{
    if (odmData.heightmap.empty())
    {
        return false;
    }

    const int gx = std::clamp(static_cast<int>(std::floor(formats::outdoorWorldToGridX(worldX))), 0,
                              formats::ODMMapData::TERRAIN_SIZE - 1);
    const int gy = std::clamp(static_cast<int>(std::floor(formats::outdoorWorldToGridY(worldY))), 0,
                              formats::ODMMapData::TERRAIN_SIZE - 1);
    const size_t index = static_cast<size_t>(gy * formats::ODMMapData::TERRAIN_SIZE + gx);
    if (index >= odmData.heightmap.size())
    {
        return false;
    }

    const uint8_t tileIndex = odmData.heightmap[index].tileIndex;
    if (tileIndex >= odmData.tileTextures.size())
    {
        return false;
    }
    const std::string& texture = odmData.tileTextures[tileIndex];
    return texture.rfind("wtrtyl", 0) == 0;
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

    const float floor = sampleOutdoorFloorHeight(odmData, worldX, worldY, currentZ + 48.0f);
    return std::max(currentZ, floor);
}

} // namespace runeharbor::engine
