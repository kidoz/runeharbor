// SPDX-License-Identifier: MIT
#include "map_scene.hpp"

#include <algorithm>

namespace runeharbor::engine
{

namespace
{
constexpr uint8_t FLOOR_COLOR[3] = {80, 120, 200};
constexpr uint8_t WALL_COLOR[3] = {180, 180, 180};
constexpr uint8_t CEILING_COLOR[3] = {200, 100, 100};
constexpr uint8_t PORTAL_COLOR[3] = {200, 200, 100};
constexpr uint8_t WATER_COLOR[3] = {100, 200, 200};
} // namespace

MapScene::MapScene(util::ILogger& logger) : logger(logger) {}

bool MapScene::loadBLV(const std::string& name, const std::vector<uint8_t>& data,
                       ProgressCallback progress)
{
    clear();

    formats::BLVMap parser(logger);
    if (!parser.parse(data, progress))
    {
        logger.error("Failed to parse BLV map data");
        return false;
    }

    blv = parser.getData();
    mapName = name;
    if (progress)
    {
        progress(0.97f);
    }
    buildWorldVertices();
    computeBounds();
    loaded = true;
    if (progress)
    {
        progress(1.0f);
    }
    return true;
}

bool MapScene::loadODM(const std::string& name, const std::vector<uint8_t>& data,
                       ProgressCallback progress)
{
    clear();

    formats::ODMMap parser(logger);
    parser.setTileTable(tileTable);
    if (!parser.parse(data, progress))
    {
        logger.error("Failed to parse ODM map data");
        return false;
    }

    odm = parser.getData();
    mapName = name;

    // Build a coarse vertex cloud from the heightmap for bounds
    worldVertices.clear();
    worldVertices.reserve(odm.heightmap.size());

    if (progress)
    {
        progress(0.97f);
    }

    constexpr float cellSize = 512.0f;
    const int terrainSize = formats::ODMMapData::TERRAIN_SIZE;
    for (int y = 0; y < terrainSize; y++)
    {
        for (int x = 0; x < terrainSize; x++)
        {
            size_t index = static_cast<size_t>(y * terrainSize + x);
            if (index >= odm.heightmap.size())
            {
                continue;
            }

            float wx = (x - terrainSize / 2) * cellSize;
            float wz = (y - terrainSize / 2) * cellSize;
            float wy = static_cast<float>(odm.heightmap[index].height);
            worldVertices.emplace_back(wx, wy, wz);
        }
    }

    computeBounds();
    loaded = true;
    if (progress)
    {
        progress(1.0f);
    }
    return true;
}

void MapScene::renderWireframe(graphics::LineRenderer& renderer,
                               const MapRenderOptions& options) const
{
    if (!loaded)
    {
        return;
    }

    if (!odm.heightmap.empty())
    {
        constexpr float cellSize = 512.0f;
        constexpr int terrainSize = formats::ODMMapData::TERRAIN_SIZE;
        constexpr int step = 4;

        if (options.showFloors)
        {
            for (int y = 0; y < terrainSize - step; y += step)
            {
                for (int x = 0; x < terrainSize - step; x += step)
                {
                    float wx0 = (x - terrainSize / 2) * cellSize;
                    float wz0 = (y - terrainSize / 2) * cellSize;
                    float wx1 = (x + step - terrainSize / 2) * cellSize;
                    float wz1 = (y + step - terrainSize / 2) * cellSize;

                    size_t index00 = static_cast<size_t>(y * terrainSize + x);
                    size_t index10 = static_cast<size_t>(y * terrainSize + x + step);
                    size_t index01 = static_cast<size_t>((y + step) * terrainSize + x);

                    if (index00 >= odm.heightmap.size() || index10 >= odm.heightmap.size() ||
                        index01 >= odm.heightmap.size())
                    {
                        continue;
                    }

                    float h00 = static_cast<float>(odm.heightmap[index00].height);
                    float h10 = static_cast<float>(odm.heightmap[index10].height);
                    float h01 = static_cast<float>(odm.heightmap[index01].height);

                    graphics::Vec3 v00(wx0, h00, wz0);
                    graphics::Vec3 v10(wx1, h10, wz0);
                    graphics::Vec3 v01(wx0, h01, wz1);

                    renderer.drawLine3D(v00, v10, FLOOR_COLOR[0], FLOOR_COLOR[1], FLOOR_COLOR[2]);
                    renderer.drawLine3D(v00, v01, FLOOR_COLOR[0], FLOOR_COLOR[1], FLOOR_COLOR[2]);
                }
            }
        }

        if (options.showWalls)
        {
            for (const auto& building : odm.buildings)
            {
                float x0 = static_cast<float>(building.worldX + building.minX);
                float x1 = static_cast<float>(building.worldX + building.maxX);
                float z0 = static_cast<float>(building.worldZ + building.minZ);
                float z1 = static_cast<float>(building.worldZ + building.maxZ);
                float y0 = static_cast<float>(building.worldY + building.minY);
                float y1 = static_cast<float>(building.worldY + building.maxY);

                graphics::Vec3 corners[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
                                             {x0, y0, z1}, {x0, y1, z0}, {x1, y1, z0},
                                             {x1, y1, z1}, {x0, y1, z1}};

                renderer.drawLine3D(corners[0], corners[1], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[1], corners[2], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[2], corners[3], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[3], corners[0], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);

                renderer.drawLine3D(corners[4], corners[5], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[5], corners[6], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[6], corners[7], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[7], corners[4], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);

                renderer.drawLine3D(corners[0], corners[4], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[1], corners[5], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[2], corners[6], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
                renderer.drawLine3D(corners[3], corners[7], WALL_COLOR[0], WALL_COLOR[1],
                                    WALL_COLOR[2]);
            }
        }

        return;
    }

    if (!blv.faces.empty())
    {
        for (const auto& face : blv.faces)
        {
            if (face.vertexIndices.size() < 2 || face.isInvisible())
            {
                continue;
            }

            const uint8_t* color = WALL_COLOR;
            bool shouldRender = true;

            if (face.isFloor())
            {
                color = face.isWater() ? WATER_COLOR : FLOOR_COLOR;
                shouldRender = options.showFloors;
            }
            else if (face.isCeiling())
            {
                color = CEILING_COLOR;
                shouldRender = options.showCeilings;
            }
            else if (face.isPortal())
            {
                color = PORTAL_COLOR;
                shouldRender = options.showPortals;
            }
            else
            {
                shouldRender = options.showWalls;
            }

            if (!shouldRender)
            {
                continue;
            }

            for (size_t i = 0; i < face.vertexIndices.size(); i++)
            {
                size_t next = (i + 1) % face.vertexIndices.size();
                uint16_t aIndex = face.vertexIndices[i];
                uint16_t bIndex = face.vertexIndices[next];
                if (aIndex >= worldVertices.size() || bIndex >= worldVertices.size())
                {
                    continue;
                }

                const graphics::Vec3& a = worldVertices[aIndex];
                const graphics::Vec3& b = worldVertices[bIndex];
                renderer.drawLine3D(a, b, color[0], color[1], color[2]);
            }
        }
    }
    else
    {
        for (const auto& pos : worldVertices)
        {
            renderer.drawPoint3D(pos, 10.0f, 100, 150, 200);
        }
    }

    if (options.showLights)
    {
        for (const auto& light : blv.lights)
        {
            graphics::Vec3 pos(static_cast<float>(light.x), static_cast<float>(light.z),
                               static_cast<float>(light.y));
            float size = static_cast<float>(light.radius) * 0.1f;
            if (size < 10.0f)
            {
                size = 10.0f;
            }

            uint8_t r = light.red > 0 ? light.red : 255;
            uint8_t g = light.green > 0 ? light.green : 255;
            uint8_t b = light.blue > 0 ? light.blue : 100;
            renderer.drawPoint3D(pos, size, r, g, b);
        }
    }
}

void MapScene::clear()
{
    mapName.clear();
    blv = {};
    odm = {};
    worldVertices.clear();
    bounds = {};
    loaded = false;
}

void MapScene::buildWorldVertices()
{
    worldVertices.clear();
    worldVertices.reserve(blv.vertices.size());

    for (const auto& v : blv.vertices)
    {
        worldVertices.emplace_back(static_cast<float>(v.x), static_cast<float>(v.z),
                                   static_cast<float>(v.y));
    }
}

void MapScene::computeBounds()
{
    bounds = {};
    if (worldVertices.empty())
    {
        return;
    }

    graphics::Vec3 min = worldVertices.front();
    graphics::Vec3 max = worldVertices.front();

    for (const auto& v : worldVertices)
    {
        min.x = std::min(min.x, v.x);
        min.y = std::min(min.y, v.y);
        min.z = std::min(min.z, v.z);

        max.x = std::max(max.x, v.x);
        max.y = std::max(max.y, v.y);
        max.z = std::max(max.z, v.z);
    }

    bounds.min = min;
    bounds.max = max;
    bounds.valid = true;
}

} // namespace runeharbor::engine
