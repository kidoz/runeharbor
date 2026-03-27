// SPDX-License-Identifier: MIT
#include "visibility.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include <cmath>

#include "../formats/blv_map.hpp"
#include "../formats/odm_map.hpp"
#include "world_coordinates.hpp"

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

float TerrainLOD::lod1DistSq_ = TerrainLOD::kLOD1DistSq;
float TerrainLOD::lod2DistSq_ = TerrainLOD::kLOD2DistSq;
float TerrainLOD::maxRenderDistSq_ = TerrainLOD::kMaxRenderDistSq;

int TerrainLOD::lodStep(float distanceSq)
{
    if (distanceSq > maxRenderDistSq_)
        return 0; // Don't render at all
    if (distanceSq > lod2DistSq_)
        return 4;
    if (distanceSq > lod1DistSq_)
        return 2;
    return 1;
}

void TerrainLOD::configureFromGridBands(int gridBand1, int gridBand2, int gridBand3)
{
    const int nearBand = std::max(1, gridBand1);
    const int midBand = std::max(nearBand, gridBand2);
    const int farBand = std::max(midBand, gridBand3);

    const float lod1Dist = static_cast<float>(nearBand) * 1500.0f;
    const float lod2Dist = static_cast<float>(midBand) * 2000.0f;
    const float maxDist = static_cast<float>(farBand) * 2000.0f;

    lod1DistSq_ = lod1Dist * lod1Dist;
    lod2DistSq_ = lod2Dist * lod2Dist;
    maxRenderDistSq_ = maxDist * maxDist;
}

void TerrainLOD::resetDefaults()
{
    lod1DistSq_ = kLOD1DistSq;
    lod2DistSq_ = kLOD2DistSq;
    maxRenderDistSq_ = kMaxRenderDistSq;
}

float TerrainLOD::lod1DistSq()
{
    return lod1DistSq_;
}

float TerrainLOD::lod2DistSq()
{
    return lod2DistSq_;
}

float TerrainLOD::maxRenderDistSq()
{
    return maxRenderDistSq_;
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

    // Clear previous failure state if caller is retrying this resource.
    failed_.erase(name);
    pending_.push_back({name, type, false});
}

void AsyncLoader::setHandler(LoadRequest::Type type, LoadHandler handler)
{
    handlers_[typeToIndex(type)] = std::move(handler);
}

bool AsyncLoader::isLoaded(const std::string& name) const
{
    return loaded_.count(name) > 0;
}

bool AsyncLoader::isFailed(const std::string& name) const
{
    return failed_.count(name) > 0;
}

void AsyncLoader::processPending(size_t maxItems)
{
    if (pending_.empty() || maxItems == 0)
        return;

    const size_t toProcess = std::min(maxItems, pending_.size());
    for (size_t i = 0; i < toProcess; i++)
    {
        auto req = pending_.front();
        pending_.pop_front();

        bool success = true;
        const auto& handler = handlers_[typeToIndex(req.type)];
        if (handler)
        {
            success = handler(req);
        }

        if (success)
        {
            loaded_.insert(req.resourceName);
            failed_.erase(req.resourceName);
        }
        else
        {
            failed_.insert(req.resourceName);
        }
    }
}

void AsyncLoader::clear()
{
    pending_.clear();
    loaded_.clear();
    failed_.clear();
}

std::optional<PickHit> pickClosestProjectedPoint(const Mat4& viewProjection, int viewportWidth,
                                                 int viewportHeight, int mouseX, int mouseY,
                                                 std::span<const PickCandidate> candidates,
                                                 float pickRadiusPx,
                                                 const PickSelectionFilter& filter)
{
    if (viewportWidth <= 0 || viewportHeight <= 0 || candidates.empty())
    {
        return std::nullopt;
    }

    PickHit best{};
    bool found = false;
    float bestDepth = std::numeric_limits<float>::max();
    float bestDistSq = std::numeric_limits<float>::max();
    const float radiusSq = pickRadiusPx * pickRadiusPx;

    for (const auto& candidate : candidates)
    {
        if (filter.requireEventId && candidate.eventId <= 0)
        {
            continue;
        }
        if (filter.type.has_value() && candidate.type != *filter.type)
        {
            continue;
        }

        Vec4 clip = viewProjection * Vec4(candidate.worldPos, 1.0f);
        if (clip.w <= 0.001f)
        {
            continue;
        }

        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f)
        {
            continue;
        }

        const float sx = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportWidth);
        const float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(viewportHeight);
        const float dx = static_cast<float>(mouseX) - sx;
        const float dy = static_cast<float>(mouseY) - sy;
        const float distSq = dx * dx + dy * dy;
        if (distSq > radiusSq)
        {
            continue;
        }

        const float depth = clip.w;
        if (!found || depth < bestDepth ||
            (std::abs(depth - bestDepth) < 0.01f && distSq < bestDistSq))
        {
            found = true;
            bestDepth = depth;
            bestDistSq = distSq;
            best.id = candidate.id;
            best.depth = depth;
            best.screenDistanceSq = distSq;
            best.type = candidate.type;
            best.objectIndex = candidate.objectIndex;
            best.eventId = candidate.eventId;
            best.worldPos = candidate.worldPos;
        }
    }

    return found ? std::optional<PickHit>(best) : std::nullopt;
}

std::vector<PickCandidate> collectMapEventCandidates(const formats::BLVMapData& blvData,
                                                     const formats::ODMMapData& odmData,
                                                     const Mat4& viewProjection,
                                                     const Vec3& cameraPos)
{
    Frustum frustum;
    frustum.extractFromMatrix(viewProjection);

    std::unordered_set<uint16_t> visibleIndoorSectors;
    if (!blvData.sectors.empty())
    {
        PortalVisibility portalVisibility;
        visibleIndoorSectors = portalVisibility.computeVisibleSectors(blvData, cameraPos, 8);
    }

    return collectMapEventCandidates(blvData, odmData, frustum, visibleIndoorSectors);
}

std::vector<PickCandidate>
collectMapEventCandidates(const formats::BLVMapData& blvData, const formats::ODMMapData& odmData,
                          const Frustum& frustum,
                          const std::unordered_set<uint16_t>& visibleIndoorSectors)
{
    std::vector<PickCandidate> candidates;
    const bool hasIndoorSectorGraph = !blvData.sectors.empty();
    const bool cullAllIndoor = hasIndoorSectorGraph && visibleIndoorSectors.empty();

    if (!cullAllIndoor)
    {
        for (const auto& face : blvData.faces)
        {
            if (face.vertexIndices.empty() || face.isInvisible())
            {
                continue;
            }
            if (!visibleIndoorSectors.empty() && !visibleIndoorSectors.contains(face.sectorId))
            {
                continue;
            }
            if (!frustum.testAABB(static_cast<float>(face.minX), static_cast<float>(face.minY),
                                  static_cast<float>(face.minZ), static_cast<float>(face.maxX),
                                  static_cast<float>(face.maxY), static_cast<float>(face.maxZ)))
            {
                continue;
            }

            Vec3 centroid = Vec3::zero();
            int valid = 0;
            for (uint16_t vi : face.vertexIndices)
            {
                if (vi >= blvData.vertices.size())
                {
                    continue;
                }
                const auto& v = blvData.vertices[vi];
                centroid = centroid + Vec3(static_cast<float>(v.x), static_cast<float>(v.y),
                                           static_cast<float>(v.z));
                valid++;
            }
            if (valid <= 0)
            {
                continue;
            }
            centroid = centroid * (1.0f / static_cast<float>(valid));
            candidates.push_back(PickCandidate{
                .id = (face.eventId > 0) ? face.eventId : static_cast<int>(candidates.size()) + 1,
                .worldPos = centroid,
                .type = PickObjectType::IndoorFace,
                .objectIndex = static_cast<int>(&face - blvData.faces.data()),
                .eventId = face.eventId,
            });
        }

        for (size_t i = 0; i < blvData.decorations.size(); i++)
        {
            const auto& decor = blvData.decorations[i];
            if (decor.hidden)
            {
                continue;
            }
            if (!frustum.testSphere(static_cast<float>(decor.x), static_cast<float>(decor.y),
                                    static_cast<float>(decor.z), 128.0f))
            {
                continue;
            }
            candidates.push_back(PickCandidate{
                .id = (decor.eventId > 0) ? static_cast<int>(decor.eventId)
                                          : static_cast<int>(candidates.size()) + 1,
                .worldPos = Vec3(static_cast<float>(decor.x), static_cast<float>(decor.y),
                                 static_cast<float>(decor.z)),
                .type = PickObjectType::IndoorDecoration,
                .objectIndex = static_cast<int>(i),
                .eventId = static_cast<int>(decor.eventId),
            });
        }
    }

    for (size_t bi = 0; bi < odmData.buildings.size(); bi++)
    {
        const auto& building = odmData.buildings[bi];
        const float minX = static_cast<float>(building.worldX + building.minX);
        const float minY = static_cast<float>(building.worldZ + building.minZ);
        const float minZ = static_cast<float>(building.worldY + building.minY);
        const float maxX = static_cast<float>(building.worldX + building.maxX);
        const float maxY = static_cast<float>(building.worldZ + building.maxZ);
        const float maxZ = static_cast<float>(building.worldY + building.maxY);
        if (!frustum.testAABB(minX, minY, minZ, maxX, maxY, maxZ))
        {
            continue;
        }

        for (size_t fi = 0; fi < building.faces.size(); fi++)
        {
            const auto& face = building.faces[fi];
            if (face.vertexIndices.empty() || face.isInvisible())
            {
                continue;
            }

            Vec3 centroid = Vec3::zero();
            int valid = 0;
            for (uint16_t vi : face.vertexIndices)
            {
                if (vi >= building.vertices.size())
                {
                    continue;
                }
                const auto& v = building.vertices[vi];
                // BSP model vertices are in absolute world coordinates
                centroid = centroid + gameplayToRenderPosition(static_cast<float>(v.x),
                                                               static_cast<float>(v.y),
                                                               static_cast<float>(v.z));
                valid++;
            }
            if (valid <= 0)
            {
                continue;
            }
            centroid = centroid * (1.0f / static_cast<float>(valid));
            const int packedFaceIndex =
                (static_cast<int>(bi & 0xFFFFu) << 16) | static_cast<int>(fi & 0xFFFFu);
            candidates.push_back(PickCandidate{
                .id = (face.eventId > 0) ? face.eventId : static_cast<int>(candidates.size()) + 1,
                .worldPos = centroid,
                .type = PickObjectType::OutdoorBuildingFace,
                .objectIndex = packedFaceIndex,
                .eventId = face.eventId,
            });
        }
    }

    return candidates;
}

} // namespace runeharbor::graphics
