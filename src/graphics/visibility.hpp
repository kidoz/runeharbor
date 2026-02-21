// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
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

    /// Configure LOD thresholds from MM7-style outdoor grid band settings.
    static void configureFromGridBands(int gridBand1, int gridBand2, int gridBand3);

    /// Reset runtime thresholds to built-in defaults.
    static void resetDefaults();

    /// Effective runtime thresholds (squared).
    static float lod1DistSq();
    static float lod2DistSq();
    static float maxRenderDistSq();

    /// Maximum render distance squared for terrain
    static constexpr float kMaxRenderDistSq = 50000.0f * 50000.0f;

    /// LOD distance thresholds (squared)
    static constexpr float kLOD1DistSq = 15000.0f * 15000.0f;
    static constexpr float kLOD2DistSq = 30000.0f * 30000.0f;

  private:
    static float lod1DistSq_;
    static float lod2DistSq_;
    static float maxRenderDistSq_;
};

/// Resource loading queue that processes a bounded number of requests per tick.
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

    using LoadHandler = std::function<bool(const LoadRequest&)>;

    /// Queue a resource for async loading
    void enqueue(const std::string& name, LoadRequest::Type type);

    /// Register a per-type load handler. Returns true on successful load.
    /// If no handler is registered for a type, requests succeed by default.
    void setHandler(LoadRequest::Type type, LoadHandler handler);

    /// Check if a resource is loaded
    bool isLoaded(const std::string& name) const;

    /// Check if a resource failed to load in the most recent attempt.
    bool isFailed(const std::string& name) const;

    /// Process pending loads (call each frame). Limits work per tick.
    void processPending(size_t maxItems = 1);

    /// Number of pending loads
    int pendingCount() const { return static_cast<int>(pending_.size()); }

    /// Clear all pending
    void clear();

  private:
    static constexpr size_t kTypeCount = 3;
    static constexpr size_t typeToIndex(LoadRequest::Type type)
    {
        return static_cast<size_t>(type);
    }

    std::deque<LoadRequest> pending_;
    std::unordered_set<std::string> loaded_;
    std::unordered_set<std::string> failed_;
    std::array<LoadHandler, kTypeCount> handlers_;
};

enum class PickObjectType : uint8_t
{
    Unknown = 0,
    IndoorFace = 1,
    IndoorDecoration = 2,
    OutdoorBuildingFace = 3,
    Monster = 4,
    MapItem = 5,
};

/// Candidate point for screen-space picking.
struct PickCandidate
{
    int id = 0; // Legacy compatibility: usually event id or object id.
    Vec3 worldPos = Vec3::zero();
    PickObjectType type = PickObjectType::Unknown;
    int objectIndex = -1;
    int eventId = 0;
};

/// Result of a successful screen-space pick.
struct PickHit
{
    int id = 0;
    float depth = 0.0f;
    float screenDistanceSq = 0.0f;
    PickObjectType type = PickObjectType::Unknown;
    int objectIndex = -1;
    int eventId = 0;
    Vec3 worldPos = Vec3::zero();
};

struct PickSelectionFilter
{
    bool requireEventId = false;
    std::optional<PickObjectType> type;
};

/// Pick nearest projected point under cursor.
std::optional<PickHit> pickClosestProjectedPoint(const Mat4& viewProjection, int viewportWidth,
                                                 int viewportHeight, int mouseX, int mouseY,
                                                 std::span<const PickCandidate> candidates,
                                                 float pickRadiusPx,
                                                 const PickSelectionFilter& filter = {});

/// Collect map event candidates visible from the current camera transform.
std::vector<PickCandidate> collectMapEventCandidates(const formats::BLVMapData& blvData,
                                                     const formats::ODMMapData& odmData,
                                                     const Mat4& viewProjection,
                                                     const Vec3& cameraPos);

/// Collect map event candidates using precomputed visibility state.
std::vector<PickCandidate>
collectMapEventCandidates(const formats::BLVMapData& blvData, const formats::ODMMapData& odmData,
                          const Frustum& frustum,
                          const std::unordered_set<uint16_t>& visibleIndoorSectors);

} // namespace runeharbor::graphics
