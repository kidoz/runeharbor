// SPDX-License-Identifier: MIT
#include "occlusion.hpp"

#include <algorithm>

#include <cmath>

#include "../formats/blv_map.hpp"
#include "../formats/odm_map.hpp"

namespace runeharbor::graphics
{

namespace
{

constexpr float kRayEpsilon = 1e-4f;
// Do not treat geometry at the target's own position as an occluder (sprites
// stand on the ground, often against the wall behind them).
constexpr float kTargetMargin = 8.0f;
constexpr int kTerrainSamples = 48;
constexpr float kTerrainMargin = 16.0f;

bool intersectTriangle(const Vec3& origin, const Vec3& direction, const Vec3& a, const Vec3& b,
                       const Vec3& c, float& distance)
{
    const Vec3 edge1 = b - a;
    const Vec3 edge2 = c - a;
    const Vec3 p = direction.cross(edge2);
    const float determinant = edge1.dot(p);
    if (std::abs(determinant) < 1e-6f)
    {
        return false;
    }

    const float inverseDeterminant = 1.0f / determinant;
    const Vec3 t = origin - a;
    const float u = t.dot(p) * inverseDeterminant;
    if (u < -kRayEpsilon || u > 1.0f + kRayEpsilon)
    {
        return false;
    }

    const Vec3 q = t.cross(edge1);
    const float v = direction.dot(q) * inverseDeterminant;
    if (v < -kRayEpsilon || u + v > 1.0f + kRayEpsilon)
    {
        return false;
    }

    distance = edge2.dot(q) * inverseDeterminant;
    return distance > kRayEpsilon;
}

Vec3 toVec(const formats::BLVVertex& vertex)
{
    return {static_cast<float>(vertex.x), static_cast<float>(vertex.y),
            static_cast<float>(vertex.z)};
}

Vec3 toVec(const formats::ODMVertex3D& vertex)
{
    return {static_cast<float>(vertex.x), static_cast<float>(vertex.y),
            static_cast<float>(vertex.z)};
}

bool faceBlocksLineOfSight(const formats::ParsedFace& face)
{
    return !face.isPortal() && !face.isInvisible() && !face.isWater() && !face.isLava();
}

bool segmentHitsFace(const Vec3& origin, const Vec3& direction, float maxDistance,
                     const formats::ParsedFace& face, const auto& vertexAt, const auto& vertexCount)
{
    const auto& indices = face.vertexIndices;
    if (indices.size() < 3)
    {
        return false;
    }

    for (size_t i = 1; i + 1 < indices.size(); ++i)
    {
        if (indices[0] >= vertexCount || indices[i] >= vertexCount || indices[i + 1] >= vertexCount)
        {
            continue;
        }
        const Vec3 a = vertexAt(indices[0]);
        const Vec3 b = vertexAt(indices[i]);
        const Vec3 c = vertexAt(indices[i + 1]);
        float distance = 0.0f;
        if (intersectTriangle(origin, direction, a, b, c, distance) && distance < maxDistance)
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool isOccludedByIndoorGeometry(const formats::BLVMapData& data, const Vec3& cameraPos,
                                const Vec3& targetPos,
                                const std::unordered_set<uint16_t>* visibleSectors)
{
    if (data.vertices.empty() || data.faces.empty())
    {
        return false;
    }

    const Vec3 segment = targetPos - cameraPos;
    const float length = segment.length();
    if (length <= kTargetMargin)
    {
        return false;
    }
    const Vec3 direction = segment / length;
    const float maxDistance = length - kTargetMargin;
    const auto vertexCount = data.vertices.size();
    const auto vertexAt = [&data](uint16_t index) { return toVec(data.vertices[index]); };
    const float minSegX = std::min(cameraPos.x, targetPos.x);
    const float maxSegX = std::max(cameraPos.x, targetPos.x);
    const float minSegY = std::min(cameraPos.y, targetPos.y);
    const float maxSegY = std::max(cameraPos.y, targetPos.y);
    const float minSegZ = std::min(cameraPos.z, targetPos.z);
    const float maxSegZ = std::max(cameraPos.z, targetPos.z);

    for (const auto& face : data.faces)
    {
        if (!faceBlocksLineOfSight(face))
        {
            continue;
        }
        if (visibleSectors != nullptr && !visibleSectors->contains(face.sectorId))
        {
            continue;
        }
        // Cheap bounding-box reject before triangulating the face.
        if (static_cast<float>(face.maxX) < minSegX || static_cast<float>(face.minX) > maxSegX ||
            static_cast<float>(face.maxY) < minSegY || static_cast<float>(face.minY) > maxSegY ||
            static_cast<float>(face.maxZ) < minSegZ || static_cast<float>(face.minZ) > maxSegZ)
        {
            continue;
        }
        if (segmentHitsFace(cameraPos, direction, maxDistance, face, vertexAt, vertexCount))
        {
            return true;
        }
    }
    return false;
}

bool isOccludedByOutdoorGeometry(const formats::ODMMapData& data, const Vec3& cameraPos,
                                 const Vec3& targetPos)
{
    const Vec3 segment = targetPos - cameraPos;
    const float length = segment.length();
    if (length <= kTargetMargin)
    {
        return false;
    }
    const Vec3 direction = segment / length;
    const float maxDistance = length - kTargetMargin;

    // Terrain: march the segment and compare the ray height with the heightmap.
    if (!data.heightmap.empty())
    {
        for (int sample = 1; sample < kTerrainSamples; ++sample)
        {
            const float t = static_cast<float>(sample) / static_cast<float>(kTerrainSamples);
            const Vec3 point = cameraPos + segment * t;
            const float gridX = formats::outdoorWorldToGridX(point.x);
            const float gridY = formats::outdoorWorldToGridY(point.y);
            if (gridX < 0.0f || gridY < 0.0f || gridX >= formats::ODMMapData::TERRAIN_SIZE ||
                gridY >= formats::ODMMapData::TERRAIN_SIZE)
            {
                continue;
            }

            const int cellX = std::clamp(static_cast<int>(std::floor(gridX)), 0,
                                         formats::ODMMapData::TERRAIN_SIZE - 1);
            const int cellY = std::clamp(static_cast<int>(std::floor(gridY)), 0,
                                         formats::ODMMapData::TERRAIN_SIZE - 1);
            const size_t index =
                static_cast<size_t>(cellY * formats::ODMMapData::TERRAIN_SIZE + cellX);
            if (index >= data.heightmap.size())
            {
                continue;
            }

            if (static_cast<float>(data.heightmap[index].height) > point.z + kTerrainMargin)
            {
                return true;
            }
        }
    }

    // Opaque building faces.
    const float minSegX = std::min(cameraPos.x, targetPos.x);
    const float maxSegX = std::max(cameraPos.x, targetPos.x);
    const float minSegY = std::min(cameraPos.y, targetPos.y);
    const float maxSegY = std::max(cameraPos.y, targetPos.y);
    const float minSegZ = std::min(cameraPos.z, targetPos.z);
    const float maxSegZ = std::max(cameraPos.z, targetPos.z);
    for (const auto& building : data.buildings)
    {
        if (building.vertices.empty() || building.faces.empty())
        {
            continue;
        }
        if (static_cast<float>(building.maxX) < minSegX ||
            static_cast<float>(building.minX) > maxSegX ||
            static_cast<float>(building.maxY) < minSegY ||
            static_cast<float>(building.minY) > maxSegY ||
            static_cast<float>(building.maxZ) < minSegZ ||
            static_cast<float>(building.minZ) > maxSegZ)
        {
            continue;
        }
        const auto vertexCount = building.vertices.size();
        const auto vertexAt = [&building](uint16_t index)
        { return toVec(building.vertices[index]); };
        for (const auto& face : building.faces)
        {
            if (!faceBlocksLineOfSight(face))
            {
                continue;
            }
            if (segmentHitsFace(cameraPos, direction, maxDistance, face, vertexAt, vertexCount))
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace runeharbor::graphics
