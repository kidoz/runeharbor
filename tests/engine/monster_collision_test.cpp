// SPDX-License-Identifier: MIT
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/engine/monster_collision.hpp"
#include "../../src/formats/blv_map.hpp"
#include "../../src/formats/odm_map.hpp"

using namespace runeharbor;

namespace
{

formats::ParsedFace makeWall(float normalZ = 0.0f)
{
    formats::ParsedFace face;
    face.normalFX = 0.0f;
    face.normalFY = 1.0f - normalZ;
    face.normalFZ = normalZ;
    face.vertexIndices = {0, 1, 2, 3};
    return face;
}

formats::BLVMapData makeIndoorRoom()
{
    formats::BLVMapData data;
    data.vertices = {
        {0, -1000, -1000},
        {0, 1000, -1000},
        {0, 1000, 1000},
        {0, -1000, 1000},
    };
    data.faces.push_back(makeWall());
    return data;
}

formats::ODMMapData makeOutdoorMap()
{
    formats::ODMMapData data;
    data.heightmap.resize(static_cast<size_t>(formats::ODMMapData::TERRAIN_SIZE) *
                          formats::ODMMapData::TERRAIN_SIZE);
    data.tileTextures = {"grastyl", "wtrtyl"};
    return data;
}

} // namespace

TEST_CASE("Monster collision blocks indoor walls but ignores floors and portals",
          "[engine][collision]")
{
    formats::BLVMapData room = makeIndoorRoom();

    CHECK_FALSE(engine::canMonsterMove(&room, nullptr, -100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f));
    // Movement on the same side of the wall is allowed.
    CHECK(engine::canMonsterMove(&room, nullptr, -100.0f, 0.0f, 0.0f, -50.0f, 100.0f, 0.0f));

    // Portal faces (open doors) are passable.
    room.faces[0].attributes = static_cast<uint32_t>(formats::FaceAttribute::Portal);
    CHECK(engine::canMonsterMove(&room, nullptr, -100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f));

    // Typed ramps are walkable, typed vertical walls block.
    room.faces[0].attributes = 0;
    room.faces[0].polygonType = static_cast<uint8_t>(formats::PolygonType::BetweenFloorAndWall);
    CHECK(engine::canMonsterMove(&room, nullptr, -100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f));
    room.faces[0].polygonType = static_cast<uint8_t>(formats::PolygonType::VerticalWall);
    CHECK_FALSE(engine::canMonsterMove(&room, nullptr, -100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f));
    room.faces[0].polygonType = 0;

    // A floor/ramp face is not a wall.
    formats::BLVMapData floorMap;
    floorMap.vertices = {
        {-1000, -1000, 0},
        {1000, -1000, 0},
        {1000, 1000, 0},
        {-1000, 1000, 0},
    };
    floorMap.faces.push_back(makeWall(1.0f));
    CHECK(engine::canMonsterMove(&floorMap, nullptr, -100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f));
}

TEST_CASE("Monster collision blocks water and steep terrain outdoors", "[engine][collision]")
{
    formats::ODMMapData map = makeOutdoorMap();
    // Water cell at grid (65, 64), which begins at world x=512 for y in [0,512).
    map.heightmap[static_cast<size_t>(64 * formats::ODMMapData::TERRAIN_SIZE + 65)].tileIndex = 1;

    CHECK_FALSE(engine::canMonsterMove(nullptr, &map, 256.0f, 0.0f, 0.0f, 600.0f, 0.0f, 0.0f));
    CHECK(engine::canMonsterMove(nullptr, &map, 256.0f, 0.0f, 0.0f, 400.0f, 0.0f, 0.0f));
    // A monster already in water may leave it.
    CHECK(engine::canMonsterMove(nullptr, &map, 700.0f, 0.0f, 0.0f, 900.0f, 0.0f, 0.0f));

    // A tall step blocks ground movement; a low step does not.
    map.tileTextures[1] = "grastyl";
    map.heightmap[static_cast<size_t>(64 * formats::ODMMapData::TERRAIN_SIZE + 65)].height = 200;
    CHECK_FALSE(engine::canMonsterMove(nullptr, &map, 256.0f, 0.0f, 0.0f, 600.0f, 0.0f, 0.0f));
    map.heightmap[static_cast<size_t>(64 * formats::ODMMapData::TERRAIN_SIZE + 65)].height = 16;
    CHECK(engine::canMonsterMove(nullptr, &map, 256.0f, 0.0f, 0.0f, 600.0f, 0.0f, 0.0f));
}

TEST_CASE("Monster collision blocks outdoor building walls", "[engine][collision]")
{
    formats::ODMMapData map = makeOutdoorMap();

    formats::ParsedBuilding building;
    building.vertices = {
        {0, -1000, -1000},
        {0, 1000, -1000},
        {0, 1000, 1000},
        {0, -1000, 1000},
    };
    building.faces.push_back(makeWall());
    building.minX = -10;
    building.maxX = 10;
    building.minY = -1000;
    building.maxY = 1000;
    building.minZ = -1000;
    building.maxZ = 1000;
    map.buildings.push_back(building);

    CHECK_FALSE(engine::canMonsterMove(nullptr, &map, -100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f));
    CHECK(engine::canMonsterMove(nullptr, &map, -100.0f, 0.0f, 0.0f, -50.0f, 100.0f, 0.0f));
}
