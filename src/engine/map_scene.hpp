// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "../formats/blv_map.hpp"
#include "../formats/odm_map.hpp"
#include "../graphics/line_renderer.hpp"
#include "../graphics/math3d.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::engine
{

struct MapRenderOptions
{
    bool showFloors = true;
    bool showWalls = true;
    bool showCeilings = true;
    bool showPortals = true;
    bool showLights = true;
};

struct MapBounds
{
    graphics::Vec3 min;
    graphics::Vec3 max;
    bool valid = false;

    graphics::Vec3 center() const { return valid ? (min + max) * 0.5f : graphics::Vec3::zero(); }

    float radius() const
    {
        if (!valid)
        {
            return 0.0f;
        }
        graphics::Vec3 extent = max - min;
        return std::max(std::max(extent.x, extent.y), extent.z) * 0.5f;
    }
};

class MapScene
{
  public:
    using ProgressCallback = std::function<void(float)>;

    explicit MapScene(util::ILogger& logger);

    bool loadBLV(const std::string& name, const std::vector<uint8_t>& data,
                 ProgressCallback progress = {});
    bool loadODM(const std::string& name, const std::vector<uint8_t>& data,
                 ProgressCallback progress = {});

    bool isLoaded() const { return loaded; }
    const std::string& getName() const { return mapName; }
    const MapBounds& getBounds() const { return bounds; }
    const formats::BLVMapData& getBLVData() const { return blv; }
    const formats::ODMMapData& getODMData() const { return odm; }
    formats::BLVMapData& mutableBLVData() { return blv; }
    formats::ODMMapData& mutableODMData() { return odm; }

    void renderWireframe(graphics::LineRenderer& renderer, const MapRenderOptions& options) const;

  private:
    void clear();
    void buildWorldVertices();
    void computeBounds();

    util::ILogger& logger;
    std::string mapName;
    formats::BLVMapData blv;
    formats::ODMMapData odm;
    std::vector<graphics::Vec3> worldVertices;
    MapBounds bounds;
    bool loaded = false;
};

} // namespace runeharbor::engine
