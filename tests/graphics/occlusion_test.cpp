// SPDX-License-Identifier: MIT
#include <unordered_set>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../../src/formats/blv_map.hpp"
#include "../../src/formats/odm_map.hpp"
#include "../../src/graphics/occlusion.hpp"

namespace formats = runeharbor::formats;
using namespace runeharbor::formats;
using namespace runeharbor::graphics;

namespace
{

formats::ParsedFace makeQuadFace(std::vector<uint16_t> indices)
{
    formats::ParsedFace face;
    face.vertexIndices = std::move(indices);
    face.numVertices = static_cast<uint8_t>(face.vertexIndices.size());
    return face;
}

formats::BLVMapData makeIndoorWallScene()
{
    formats::BLVMapData data;
    // Vertical wall at y = 0, spanning x 0..100 and z 0..100.
    data.vertices = {
        {0, 0, 0},
        {100, 0, 0},
        {100, 0, 100},
        {0, 0, 100},
    };
    formats::ParsedFace face = makeQuadFace({0, 1, 2, 3});
    face.sectorId = 0;
    face.minX = 0;
    face.maxX = 100;
    face.minY = 0;
    face.maxY = 0;
    face.minZ = 0;
    face.maxZ = 100;
    data.faces.push_back(std::move(face));
    return data;
}

} // namespace

TEST_CASE("Indoor billboard occlusion rejects geometry between camera and target",
          "[graphics][occlusion]")
{
    const auto data = makeIndoorWallScene();
    const Vec3 camera{50.0f, -100.0f, 50.0f};

    SECTION("A wall between the camera and the target occludes the target")
    {
        CHECK(isOccludedByIndoorGeometry(data, camera, {50.0f, 100.0f, 50.0f}));
    }

    SECTION("A target in front of the wall is visible")
    {
        CHECK_FALSE(isOccludedByIndoorGeometry(data, camera, {50.0f, -50.0f, 50.0f}));
    }

    SECTION("Faces in non-visible sectors do not occlude")
    {
        const std::unordered_set<uint16_t> visible{1};
        CHECK_FALSE(isOccludedByIndoorGeometry(data, camera, {50.0f, 100.0f, 50.0f}, &visible));
    }

    SECTION("Portal faces do not occlude")
    {
        auto portalData = data;
        portalData.faces[0].attributes = static_cast<uint32_t>(formats::FaceAttribute::Portal);
        CHECK_FALSE(isOccludedByIndoorGeometry(portalData, camera, {50.0f, 100.0f, 50.0f}));
    }
}

TEST_CASE("Outdoor billboard occlusion tests terrain and building faces", "[graphics][occlusion]")
{
    formats::ODMMapData data;
    data.heightmap.assign(static_cast<size_t>(formats::ODMMapData::TERRAIN_SIZE) *
                              formats::ODMMapData::TERRAIN_SIZE,
                          {});
    const Vec3 camera{0.0f, -1000.0f, 100.0f};
    const Vec3 target{0.0f, 1000.0f, 100.0f};

    SECTION("Flat terrain does not occlude")
    {
        CHECK_FALSE(isOccludedByOutdoorGeometry(data, camera, target));
    }

    SECTION("A terrain hill between camera and target occludes")
    {
        const int gridX = static_cast<int>(formats::outdoorWorldToGridX(0.0f));
        const int gridY = static_cast<int>(formats::outdoorWorldToGridY(0.0f));
        const size_t index = static_cast<size_t>(gridY * formats::ODMMapData::TERRAIN_SIZE + gridX);
        data.heightmap[index].height = 300;
        CHECK(isOccludedByOutdoorGeometry(data, camera, target));
    }

    SECTION("An opaque building face between camera and target occludes")
    {
        formats::ParsedBuilding building;
        building.vertices = {
            {-100, 0, 0},
            {100, 0, 0},
            {100, 0, 200},
            {-100, 0, 200},
        };
        building.faces.push_back(makeQuadFace({0, 1, 2, 3}));
        building.minX = -100;
        building.maxX = 100;
        building.minY = 0;
        building.maxY = 0;
        building.minZ = 0;
        building.maxZ = 200;
        data.buildings.push_back(std::move(building));

        CHECK(isOccludedByOutdoorGeometry(data, camera, target));
    }
}
