// SPDX-License-Identifier: MIT
#include "monster_collision.hpp"

#include <algorithm>

#include <cmath>

#include "../formats/blv_map.hpp"
#include "../formats/odm_map.hpp"
#include "outdoor_terrain.hpp"

namespace runeharbor::engine
{

namespace
{

constexpr float kRayEpsilon = 1e-4f;
// Faces steeper than this are floors/ceilings/ramps rather than walls.
constexpr float kMaxWallNormalZ = 0.8f;

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

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

bool intersectTriangle(const Vec3& origin, const Vec3& direction, const Vec3& a, const Vec3& b,
                       const Vec3& c, float& distance)
{
    const Vec3 edge1{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3 edge2{c.x - a.x, c.y - a.y, c.z - a.z};
    const Vec3 p{direction.y * edge2.z - direction.z * edge2.y,
                 direction.z * edge2.x - direction.x * edge2.z,
                 direction.x * edge2.y - direction.y * edge2.x};
    const float determinant = edge1.x * p.x + edge1.y * p.y + edge1.z * p.z;
    if (std::abs(determinant) < 1e-6f)
    {
        return false;
    }

    const float inverseDeterminant = 1.0f / determinant;
    const Vec3 t{origin.x - a.x, origin.y - a.y, origin.z - a.z};
    const float u = (t.x * p.x + t.y * p.y + t.z * p.z) * inverseDeterminant;
    if (u < -kRayEpsilon || u > 1.0f + kRayEpsilon)
    {
        return false;
    }

    const Vec3 q{t.y * edge1.z - t.z * edge1.y, t.z * edge1.x - t.x * edge1.z,
                 t.x * edge1.y - t.y * edge1.x};
    const float v =
        (direction.x * q.x + direction.y * q.y + direction.z * q.z) * inverseDeterminant;
    if (v < -kRayEpsilon || u + v > 1.0f + kRayEpsilon)
    {
        return false;
    }

    distance = (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z) * inverseDeterminant;
    return true;
}

template <typename Vertex>
bool segmentHitsFaces(const std::vector<formats::ParsedFace>& faces,
                      const std::vector<Vertex>& vertices, const Vec3& from, const Vec3& to,
                      float radius)
{
    if (vertices.empty() || faces.empty())
    {
        return false;
    }

    const Vec3 segment{to.x - from.x, to.y - from.y, to.z - from.z};
    const float minX = std::min(from.x, to.x) - radius;
    const float maxX = std::max(from.x, to.x) + radius;
    const float minY = std::min(from.y, to.y) - radius;
    const float maxY = std::max(from.y, to.y) + radius;
    const float minZ = std::min(from.z, to.z) - radius;
    const float maxZ = std::max(from.z, to.z) + radius;

    for (const auto& face : faces)
    {
        if (face.vertexIndices.size() < 3 || face.isPortal() || face.isInvisible() ||
            face.isWater() || face.isLava())
        {
            continue;
        }
        // Prefer the parsed polygon type; fall back to the plane normal for
        // data that does not classify faces. Ramps (BetweenFloorAndWall /
        // BetweenCeilingAndWall) stay walkable.
        if (face.polygonType != 0)
        {
            if (!face.isWall())
            {
                continue;
            }
        }
        else if (std::abs(face.normalFZ) > kMaxWallNormalZ)
        {
            continue;
        }

        for (size_t i = 1; i + 1 < face.vertexIndices.size(); ++i)
        {
            const uint16_t ia = face.vertexIndices[0];
            const uint16_t ib = face.vertexIndices[i];
            const uint16_t ic = face.vertexIndices[i + 1];
            if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
            {
                continue;
            }

            const Vec3 a = toVec(vertices[ia]);
            const Vec3 b = toVec(vertices[ib]);
            const Vec3 c = toVec(vertices[ic]);

            // Triangle bounding-box reject expanded by the monster radius.
            const float triMinX = std::min({a.x, b.x, c.x}) - radius;
            const float triMaxX = std::max({a.x, b.x, c.x}) + radius;
            const float triMinY = std::min({a.y, b.y, c.y}) - radius;
            const float triMaxY = std::max({a.y, b.y, c.y}) + radius;
            const float triMinZ = std::min({a.z, b.z, c.z}) - radius;
            const float triMaxZ = std::max({a.z, b.z, c.z}) + radius;
            if (triMaxX < minX || triMinX > maxX || triMaxY < minY || triMinY > maxY ||
                triMaxZ < minZ || triMinZ > maxZ)
            {
                continue;
            }

            float distance = 0.0f;
            if (intersectTriangle(from, segment, a, b, c, distance) && distance > kRayEpsilon &&
                distance < 1.0f - kRayEpsilon)
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace

bool canMonsterMove(const formats::BLVMapData* blv, const formats::ODMMapData* odm, float fromX,
                    float fromY, float fromZ, float toX, float toY, float toZ,
                    const MonsterCollisionConfig& config)
{
    const Vec3 from{fromX, fromY, fromZ + config.probeHeight};
    const Vec3 to{toX, toY, toZ + config.probeHeight};

    if (blv != nullptr && segmentHitsFaces(blv->faces, blv->vertices, from, to, config.radius))
    {
        return false;
    }

    if (odm == nullptr)
    {
        return true;
    }

    const float terrainFrom = sampleOutdoorTerrainHeight(*odm, fromX, fromY);
    const bool grounded = fromZ <= terrainFrom + config.stepHeight;

    if (grounded)
    {
        // Water blocks ground monsters; monsters already in water may leave it.
        if (!isOutdoorWaterAt(*odm, fromX, fromY) && isOutdoorWaterAt(*odm, toX, toY))
        {
            return false;
        }

        const float terrainTo = sampleOutdoorTerrainHeight(*odm, toX, toY);
        if (terrainTo - fromZ > config.stepHeight)
        {
            return false;
        }
    }

    for (const auto& building : odm->buildings)
    {
        // Building bounds are int32 and safe for a coarse reject.
        if (static_cast<float>(building.maxX) < std::min(fromX, toX) - config.radius ||
            static_cast<float>(building.minX) > std::max(fromX, toX) + config.radius ||
            static_cast<float>(building.maxY) < std::min(fromY, toY) - config.radius ||
            static_cast<float>(building.minY) > std::max(fromY, toY) + config.radius)
        {
            continue;
        }
        if (segmentHitsFaces(building.faces, building.vertices, from, to, config.radius))
        {
            return false;
        }
    }

    return true;
}

} // namespace runeharbor::engine
