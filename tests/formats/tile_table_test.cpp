// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/tile_table.hpp"

using namespace runeharbor::formats;

namespace
{

void appendLe16(std::vector<uint8_t>& data, uint16_t value)
{
    data.push_back(static_cast<uint8_t>(value & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
}

void appendLe32(std::vector<uint8_t>& data, uint32_t value)
{
    data.push_back(static_cast<uint8_t>(value & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 16u) & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 24u) & 0xFFu));
}

void appendTile(std::vector<uint8_t>& data, std::string_view name, uint16_t tileset,
                uint16_t variant, uint16_t flags = 0)
{
    for (size_t i = 0; i < 16; i++)
    {
        data.push_back(i < name.size() ? static_cast<uint8_t>(name[i]) : 0);
    }
    appendLe16(data, 0); // tileId, unused in MM7 data
    appendLe16(data, 0); // bitmapId, unused in MM7 data
    appendLe16(data, tileset);
    appendLe16(data, variant);
    appendLe16(data, flags);
}

// Grass at ids 0..1, water at 2..3, swamp at 4, road at 5.
std::vector<uint8_t> buildTable()
{
    std::vector<uint8_t> data;
    appendLe32(data, 6);
    appendTile(data, "grastyl", 0, 0);
    appendTile(data, "grdrtN", 0, 1);
    appendTile(data, "wtrtyl", 5, 0, kTileFlagWater);
    appendTile(data, "wtrdrNE", 5, 1, kTileFlagShore);
    appendTile(data, "swtyl", 7, 0);
    appendTile(data, "road", 10, 0);
    return data;
}

} // namespace

TEST_CASE("TileTable parses dtile records", "[tile_table]")
{
    TileTable table;
    REQUIRE(table.parse(buildTable()));
    REQUIRE(table.entries().size() == 6);

    CHECK(table.tile(0).textureName == "grastyl");
    CHECK(table.tile(2).tileset == 5);
    CHECK((table.tile(2).flags & kTileFlagWater) != 0);
    CHECK(table.tile(3).variant == 1);
}

TEST_CASE("TileTable rejects malformed dtile data", "[tile_table]")
{
    TileTable empty;
    CHECK_FALSE(empty.parse({}));

    std::vector<uint8_t> truncated;
    appendLe32(truncated, 4);
    truncated.resize(truncated.size() + 26, 0); // only one record present
    CHECK_FALSE(empty.parse(truncated));
}

TEST_CASE("TileTable returns a blank tile for out-of-range ids", "[tile_table]")
{
    TileTable table;
    REQUIRE(table.parse(buildTable()));

    CHECK(table.tile(-1).textureName.empty());
    CHECK(table.tile(999).textureName.empty());
    CHECK(table.tile(999).tileset == kInvalidTileset);
}

TEST_CASE("TileTable finds the base tile of a tileset", "[tile_table]")
{
    TileTable table;
    REQUIRE(table.parse(buildTable()));

    CHECK(table.baseTileId(0) == 0);
    CHECK(table.baseTileId(5) == 2);
    CHECK(table.baseTileId(7) == 4);
    CHECK(table.baseTileId(3) == -1); // tileset absent from the table
}

TEST_CASE("TileTable resolves map-local tile ids through the map's tilesets", "[tile_table]")
{
    TileTable table;
    REQUIRE(table.parse(buildTable()));

    // out01.odm-style tilesets: grass, water, swamp, cobble road.
    const std::array<uint8_t, 4> tilesets = {0, 5, 7, 10};

    // Below 90 the byte is already a global tile id.
    CHECK(table.resolveLocalTileId(tilesets, 3) == 3);

    // Each tileset owns a 36-wide window; the offset inside it picks the variant.
    CHECK(table.resolveLocalTileId(tilesets, 90) == 0);  // grass base
    CHECK(table.resolveLocalTileId(tilesets, 91) == 1);  // grass variant 1
    CHECK(table.resolveLocalTileId(tilesets, 126) == 2); // water base
    CHECK(table.resolveLocalTileId(tilesets, 127) == 3); // water variant 1
    CHECK(table.resolveLocalTileId(tilesets, 162) == 4); // swamp base
    CHECK(table.resolveLocalTileId(tilesets, 198) == 5); // road base

    // Bytes at or past the four windows are unused by MM7 maps.
    CHECK(table.resolveLocalTileId(tilesets, 234) == -1);
    CHECK(table.resolveLocalTileId(tilesets, 255) == -1);

    // Variants that run past the end of the table are rejected rather than clamped.
    CHECK(table.resolveLocalTileId(tilesets, 125) == -1);
}

TEST_CASE("TileTable resolution is inert without data", "[tile_table]")
{
    TileTable table;
    const std::array<uint8_t, 4> tilesets = {0, 5, 7, 10};
    CHECK(table.resolveLocalTileId(tilesets, 126) == -1);
}
