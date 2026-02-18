// SPDX-License-Identifier: MIT
#include "visibility.hpp"

#include <cmath>

#include "../formats/blv_map.hpp"

namespace runeharbor::graphics
{

// ── Frustum ──────────────────────────────────────────────────────────────────

void Frustum::Plane::normalize()
{
    float len = std::sqrt(a * a + b * b + c * c);
    if (len > 0.0001f)
    {
        float inv = 1.0f / len;
        a *= inv;
        b *= inv;
        c *= inv;
        d *= inv;
    }
}

float Frustum::Plane::distanceTo(float x, float y, float z) const
{
    return a * x + b * y + c * z + d;
}

void Frustum::extractFromMatrix(const Mat4& m)
{
    // Extract from combined view-projection matrix (Gribb/Hartmann method)
    // Left
    planes[0] = {m.m[3] + m.m[0], m.m[7] + m.m[4], m.m[11] + m.m[8], m.m[15] + m.m[12]};
    // Right
    planes[1] = {m.m[3] - m.m[0], m.m[7] - m.m[4], m.m[11] - m.m[8], m.m[15] - m.m[12]};
    // Bottom
    planes[2] = {m.m[3] + m.m[1], m.m[7] + m.m[5], m.m[11] + m.m[9], m.m[15] + m.m[13]};
    // Top
    planes[3] = {m.m[3] - m.m[1], m.m[7] - m.m[5], m.m[11] - m.m[9], m.m[15] - m.m[13]};
    // Near
    planes[4] = {m.m[3] + m.m[2], m.m[7] + m.m[6], m.m[11] + m.m[10], m.m[15] + m.m[14]};
    // Far
    planes[5] = {m.m[3] - m.m[2], m.m[7] - m.m[6], m.m[11] - m.m[10], m.m[15] - m.m[14]};

    for (auto& plane : planes)
        plane.normalize();
}

bool Frustum::testAABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const
{
    for (const auto& plane : planes)
    {
        // Find the corner most aligned with the plane normal (positive vertex)
        float px = (plane.a >= 0) ? maxX : minX;
        float py = (plane.b >= 0) ? maxY : minY;
        float pz = (plane.c >= 0) ? maxZ : minZ;

        if (plane.distanceTo(px, py, pz) < 0)
            return false; // Entirely outside this plane
    }
    return true;
}

bool Frustum::testSphere(float cx, float cy, float cz, float radius) const
{
    for (const auto& plane : planes)
    {
        if (plane.distanceTo(cx, cy, cz) < -radius)
            return false;
    }
    return true;
}

// ── PortalVisibility ─────────────────────────────────────────────────────────

int PortalVisibility::findCameraSector(const formats::BLVMapData& mapData,
                                       const Vec3& cameraPos) const
{
    // Simple approach: find the sector whose faces enclose the camera position.
    // Check floor faces of each sector and test if camera is above them.
    for (size_t si = 0; si < mapData.sectors.size(); si++)
    {
        const auto& sector = mapData.sectors[si];

        // Check if the camera is within the bounding faces of this sector.
        // A simple heuristic: check if the camera is on the correct side
        // of at least one floor face in this sector.
        for (uint16_t faceId : sector.floorFaceIds)
        {
            if (faceId >= mapData.faces.size())
                continue;
            const auto& face = mapData.faces[faceId];

            // Floor face normal points up (positive Z).
            // Camera is above the floor if dot(normal, cameraPos - facePoint) > 0.
            if (face.numVertices == 0 || face.vertexIndices.empty())
                continue;

            uint16_t vi = face.vertexIndices[0];
            if (vi >= mapData.vertices.size())
                continue;

            const auto& v = mapData.vertices[vi];
            float relZ = cameraPos.z - static_cast<float>(v.z);

            // Check if camera X,Y is within the face bounds (simple AABB check)
            float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            for (uint8_t j = 0; j < face.numVertices && j < face.vertexIndices.size(); j++)
            {
                uint16_t vj = face.vertexIndices[j];
                if (vj < mapData.vertices.size())
                {
                    float fx = static_cast<float>(mapData.vertices[vj].x);
                    float fy = static_cast<float>(mapData.vertices[vj].y);
                    if (fx < minX)
                        minX = fx;
                    if (fx > maxX)
                        maxX = fx;
                    if (fy < minY)
                        minY = fy;
                    if (fy > maxY)
                        maxY = fy;
                }
            }

            if (cameraPos.x >= minX && cameraPos.x <= maxX && cameraPos.y >= minY &&
                cameraPos.y <= maxY && relZ >= 0)
            {
                return static_cast<int>(si);
            }
        }
    }

    return 0; // Default to sector 0
}

std::unordered_set<uint16_t>
PortalVisibility::computeVisibleSectors(const formats::BLVMapData& mapData, const Vec3& cameraPos,
                                        int maxDepth) const
{
    std::unordered_set<uint16_t> visible;
    int cameraSector = findCameraSector(mapData, cameraPos);

    if (cameraSector >= 0 && cameraSector < static_cast<int>(mapData.sectors.size()))
    {
        floodFill(mapData, static_cast<uint16_t>(cameraSector), visible, 0, maxDepth);
    }

    return visible;
}

void PortalVisibility::floodFill(const formats::BLVMapData& mapData, uint16_t sectorId,
                                 std::unordered_set<uint16_t>& visible, int depth,
                                 int maxDepth) const
{
    if (depth >= maxDepth)
        return;
    if (sectorId >= mapData.sectors.size())
        return;
    if (visible.count(sectorId))
        return;

    visible.insert(sectorId);

    // Follow portal faces to adjacent sectors
    const auto& sector = mapData.sectors[sectorId];
    for (uint16_t portalFaceId : sector.portalFaceIds)
    {
        if (portalFaceId >= mapData.faces.size())
            continue;
        const auto& face = mapData.faces[portalFaceId];

        // Portal face connects to otherSectorId
        uint16_t otherSector = face.otherSectorId;
        if (otherSector == sectorId)
        {
            // If otherSectorId == current, try the face's sectorId
            otherSector = face.sectorId;
        }

        if (otherSector != sectorId && otherSector < mapData.sectors.size())
        {
            floodFill(mapData, otherSector, visible, depth + 1, maxDepth);
        }
    }
}

// ── TerrainLOD ───────────────────────────────────────────────────────────────

int TerrainLOD::lodStep(float distanceSq)
{
    if (distanceSq > kMaxRenderDistSq)
        return 0; // Don't render at all
    if (distanceSq > kLOD2DistSq)
        return 4;
    if (distanceSq > kLOD1DistSq)
        return 2;
    return 1;
}

// ── AsyncLoader ──────────────────────────────────────────────────────────────

void AsyncLoader::enqueue(const std::string& name, LoadRequest::Type type)
{
    if (loaded_.count(name))
        return; // Already loaded

    // Check if already pending
    for (const auto& req : pending_)
    {
        if (req.resourceName == name)
            return;
    }

    pending_.push_back({name, type, false});
}

bool AsyncLoader::isLoaded(const std::string& name) const
{
    return loaded_.count(name) > 0;
}

void AsyncLoader::processPending()
{
    if (pending_.empty())
        return;

    // Process one item per frame (actual loading would happen here)
    auto& req = pending_.front();
    req.completed = true;
    loaded_.insert(req.resourceName);
    pending_.erase(pending_.begin());
}

void AsyncLoader::clear()
{
    pending_.clear();
    loaded_.clear();
}

} // namespace runeharbor::graphics
