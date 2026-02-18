// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "math3d.hpp"

namespace runeharbor::formats
{
struct BLVMapData;
struct ODMMapData;
} // namespace runeharbor::formats

namespace runeharbor::graphics
{

/// Frustum planes for culling (extracted from view-projection matrix)
struct Frustum
{
    struct Plane
    {
        float a, b, c, d; // ax + by + cz + d = 0
        void normalize();
        float distanceTo(float x, float y, float z) const;
    };

    Plane planes[6]; // Left, Right, Bottom, Top, Near, Far

    /// Extract frustum planes from a view-projection matrix
    void extractFromMatrix(const Mat4& viewProjection);

    /// Test if an axis-aligned bounding box is inside or intersects the frustum
    bool testAABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const;

    /// Test if a sphere is inside or intersects the frustum
    bool testSphere(float cx, float cy, float cz, float radius) const;
};

/// Portal-based visibility for indoor (BLV) maps.
/// Determines which sectors and faces are visible from the camera position.
class PortalVisibility
{
  public:
    /// Compute visible sectors using portal flood-fill from the camera sector.
    /// Returns set of visible sector indices.
    std::unordered_set<uint16_t> computeVisibleSectors(const formats::BLVMapData& mapData,
                                                       const Vec3& cameraPos,
                                                       int maxDepth = 8) const;

    /// Find which sector the camera is in
    int findCameraSector(const formats::BLVMapData& mapData, const Vec3& cameraPos) const;

  private:
    void floodFill(const formats::BLVMapData& mapData, uint16_t sectorId,
                   std::unordered_set<uint16_t>& visible, int depth, int maxDepth) const;
};

/// Terrain LOD for outdoor (ODM) maps.
/// Reduces triangle count for distant terrain cells.
class TerrainLOD
{
  public:
    /// Compute LOD step for a terrain cell based on distance to camera.
    /// Returns step size: 1 = full detail, 2 = half, 4 = quarter, etc.
    static int lodStep(float distanceSq);

    /// Maximum render distance squared for terrain
    static constexpr float kMaxRenderDistSq = 50000.0f * 50000.0f;

    /// LOD distance thresholds (squared)
    static constexpr float kLOD1DistSq = 15000.0f * 15000.0f;
    static constexpr float kLOD2DistSq = 30000.0f * 30000.0f;
};

/// Async resource loading placeholder.
/// Tracks pending load requests and completion callbacks.
class AsyncLoader
{
  public:
    struct LoadRequest
    {
        std::string resourceName;
        enum class Type : uint8_t
        {
            Texture,
            Sound,
            Map
        };
        Type type = Type::Texture;
        bool completed = false;
    };

    /// Queue a resource for async loading
    void enqueue(const std::string& name, LoadRequest::Type type);

    /// Check if a resource is loaded
    bool isLoaded(const std::string& name) const;

    /// Process pending loads (call each frame, loads one item per call)
    void processPending();

    /// Number of pending loads
    int pendingCount() const { return static_cast<int>(pending_.size()); }

    /// Clear all pending
    void clear();

  private:
    std::vector<LoadRequest> pending_;
    std::unordered_set<std::string> loaded_;
};

} // namespace runeharbor::graphics
