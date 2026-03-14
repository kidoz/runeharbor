// SPDX-License-Identifier: MIT
#include "physics.hpp"
#include "outdoor_terrain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace runeharbor::engine
{

namespace
{
// Helper to check if a 2D point is inside a 2D polygon (ignoring Z for wall checks)
bool isPointInPolygonXY(float px, float py, const formats::ParsedFace& face,
                        const std::vector<formats::BLVVertex>& vertices)
{
    bool inside = false;
    const size_t numVerts = face.vertexIndices.size();
    for (size_t i = 0, j = numVerts - 1; i < numVerts; j = i++)
    {
        const auto& vi = vertices[face.vertexIndices[i]];
        const auto& vj = vertices[face.vertexIndices[j]];
        
        float vxi = static_cast<float>(vi.x);
        float vyi = static_cast<float>(vi.y);
        float vxj = static_cast<float>(vj.x);
        float vyj = static_cast<float>(vj.y);

        if (((vyi > py) != (vyj > py)) &&
            (px < (vxj - vxi) * (py - vyi) / (vyj - vyi) + vxi))
        {
            inside = !inside;
        }
    }
    return inside;
}

bool isPointInPolygonXY(float px, float py, const formats::ParsedFace& face,
                        const std::vector<formats::ODMVertex3D>& vertices)
{
    bool inside = false;
    const size_t numVerts = face.vertexIndices.size();
    for (size_t i = 0, j = numVerts - 1; i < numVerts; j = i++)
    {
        const auto& vi = vertices[face.vertexIndices[i]];
        const auto& vj = vertices[face.vertexIndices[j]];
        
        float vxi = static_cast<float>(vi.x);
        float vyi = static_cast<float>(vi.y);
        float vxj = static_cast<float>(vj.x);
        float vyj = static_cast<float>(vj.y);

        if (((vyi > py) != (vyj > py)) &&
            (px < (vxj - vxi) * (py - vyi) / (vyj - vyi) + vxi))
        {
            inside = !inside;
        }
    }
    return inside;
}

// Check if AABB overlaps
bool aabbOverlap(float minX1, float maxX1, float minY1, float maxY1, float minZ1, float maxZ1,
                 float minX2, float maxX2, float minY2, float maxY2, float minZ2, float maxZ2)
{
    return (minX1 <= maxX2 && maxX1 >= minX2) &&
           (minY1 <= maxY2 && maxY1 >= minY2) &&
           (minZ1 <= maxZ2 && maxZ1 >= minZ2);
}

} // namespace

bool resolveIndoorCollision(const formats::BLVMapData& blv, float& px, float& py, float& pz,
                            float radius, float height)
{
    bool collided = false;
    
    // Very simplified swept collision: push out of walls
    // We treat the player as a cylinder (radius, height).
    const float pMinX = px - radius;
    const float pMaxX = px + radius;
    const float pMinY = py - radius;
    const float pMaxY = py + radius;
    const float pMinZ = pz;
    const float pMaxZ = pz + height;

    for (const auto& face : blv.faces)
    {
        // Skip ethereal or invisible collision
        if (face.isPortal() || face.isInvisible() || face.isWater() || face.isLava())
            continue;

        // Bounding box early out
        if (!aabbOverlap(pMinX, pMaxX, pMinY, pMaxY, pMinZ, pMaxZ,
                         static_cast<float>(face.minX), static_cast<float>(face.maxX),
                         static_cast<float>(face.minY), static_cast<float>(face.maxY),
                         static_cast<float>(face.minZ), static_cast<float>(face.maxZ)))
        {
            continue;
        }

        // Calculate distance to plane
        float distToPlane = px * face.normalFX + py * face.normalFY + pz * face.normalFZ - face.normalFDist;

        // If we are too close to the plane
        if (std::abs(distToPlane) < radius)
        {
            // Check if we are inside the polygon boundaries (XY projection for walls)
            if (std::abs(face.normalFZ) < 0.1f) // Wall
            {
                // Push out along normal
                float pushDist = radius - std::abs(distToPlane);
                // Ensure we only push if we are actually "in" the face (rough approx via AABB for now,
                // proper PIP needs to handle polygon boundaries properly)
                
                // For a wall, check Z range explicitly
                if (pz + height > face.minZ && pz < face.maxZ)
                {
                    // Push out
                    float dir = (distToPlane > 0.0f) ? 1.0f : -1.0f;
                    px += face.normalFX * pushDist * dir;
                    py += face.normalFY * pushDist * dir;
                    collided = true;
                }
            }
            else // Floor or Ceiling
            {
                if (isPointInPolygonXY(px, py, face, blv.vertices))
                {
                    float pushDist = radius - std::abs(distToPlane);
                    float dir = (distToPlane > 0.0f) ? 1.0f : -1.0f;
                    pz += face.normalFZ * pushDist * dir;
                    collided = true;
                }
            }
        }
    }
    
    return collided;
}

bool resolveOutdoorCollision(const formats::ODMMapData& odm, float& px, float& py, float& pz,
                             float radius, float height)
{
    bool collided = false;
    const float pMinX = px - radius;
    const float pMaxX = px + radius;
    const float pMinY = py - radius;
    const float pMaxY = py + radius;
    const float pMinZ = pz;
    const float pMaxZ = pz + height;

    for (const auto& bldg : odm.buildings)
    {
        // Building AABB early out
        if (!aabbOverlap(pMinX, pMaxX, pMinY, pMaxY, pMinZ, pMaxZ,
                         static_cast<float>(bldg.minX), static_cast<float>(bldg.maxX),
                         static_cast<float>(bldg.minY), static_cast<float>(bldg.maxY),
                         static_cast<float>(bldg.minZ), static_cast<float>(bldg.maxZ)))
        {
            continue;
        }

        // Check faces
        for (const auto& face : bldg.faces)
        {
            if (face.isInvisible() || face.isWater() || face.isLava())
                continue;

            float distToPlane = px * face.normalFX + py * face.normalFY + pz * face.normalFZ - face.normalFDist;

            if (std::abs(distToPlane) < radius)
            {
                if (std::abs(face.normalFZ) < 0.1f) // Wall
                {
                    if (pz + height > face.minZ && pz < face.maxZ)
                    {
                        float pushDist = radius - std::abs(distToPlane);
                        float dir = (distToPlane > 0.0f) ? 1.0f : -1.0f;
                        px += face.normalFX * pushDist * dir;
                        py += face.normalFY * pushDist * dir;
                        collided = true;
                    }
                }
                else // Floor or Ceiling
                {
                    if (isPointInPolygonXY(px, py, face, bldg.vertices))
                    {
                        float pushDist = radius - std::abs(distToPlane);
                        float dir = (distToPlane > 0.0f) ? 1.0f : -1.0f;
                        pz += face.normalFZ * pushDist * dir;
                        collided = true;
                    }
                }
            }
        }
    }
    
    return collided;
}

void updatePartyPhysics(game::Party& party, const formats::BLVMapData* blv,
                        const formats::ODMMapData* odm, float deltaMs, const PhysicsConfig& config)
{
    float dt = deltaMs / 1000.0f;
    float px = party.worldX();
    float py = party.worldY();
    float pz = party.worldZ();
    float vz = party.velocityZ();

    // 1. Apply gravity
    vz += config.gravity * dt;
    if (vz < config.maxFallSpeed)
    {
        vz = config.maxFallSpeed;
    }

    // 2. Add velocity to position
    pz += vz * dt;

    // 3. Resolve collisions
    
    // First, resolve against outdoor terrain if it exists
    if (odm && !odm->heightmap.empty())
    {
        float terrainZ = sampleOutdoorTerrainHeight(*odm, px, py);
        if (pz <= terrainZ)
        {
            pz = terrainZ;
            vz = 0.0f;
        }
    }
    
    // Then resolve against indoor geometry or outdoor buildings
    if (blv && blv->faces.size() > 0)
    {
        // Save previous Z to check if we got pushed up
        float oldZ = pz;
        resolveIndoorCollision(*blv, px, py, pz, config.playerRadius, config.playerHeight);
        if (pz > oldZ)
        {
            vz = 0.0f; // Hit floor, stop falling
        }
    }
    else if (odm && odm->buildings.size() > 0)
    {
        float oldZ = pz;
        resolveOutdoorCollision(*odm, px, py, pz, config.playerRadius, config.playerHeight);
        if (pz > oldZ)
        {
            vz = 0.0f; // Hit floor/building roof, stop falling
        }
    }

    // Write back state
    party.setWorldPosition(px, py, pz);
    party.setVelocityZ(vz);
}

} // namespace runeharbor::engine
