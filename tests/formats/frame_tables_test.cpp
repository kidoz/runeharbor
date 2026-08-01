// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/frame_tables.hpp"

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

void appendFixedName(std::vector<uint8_t>& data, std::string_view name)
{
    for (size_t i = 0; i < 12; i++)
    {
        data.push_back(i < name.size() ? static_cast<uint8_t>(name[i]) : 0);
    }
}

} // namespace

TEST_CASE("SpriteFrameTable parses MM7 dsft records after the two header counts", "[frame_tables]")
{
    std::vector<uint8_t> data;
    appendLe32(data, 1); // frame count
    appendLe32(data, 7); // trailing group-lookup count
    appendFixedName(data, "dec33");
    appendFixedName(data, "dec33a");
    data.resize(data.size() + 16, 0);    // per-octant sprite ids
    appendLe32(data, 0x00018000);        // scale, 16.16 fixed point = 1.5
    appendLe32(data, 0x00040020);        // attributes: transparent | centre
    appendLe16(data, 9);                 // glowRadius
    appendLe16(data, 2);                 // paletteId
    appendLe16(data, 0);                 // paletteIndex
    appendLe16(data, 1);                 // animDuration
    appendLe16(data, 21);                // animLength
    appendLe16(data, 0);                 // reserved
    data.resize(data.size() + 7 * 2, 0); // group lookup table

    SpriteFrameTable table;
    REQUIRE(table.parse(data));
    REQUIRE(table.entries().size() == 1);

    const auto* entry = table.findEntryByIcon("DEC33");
    REQUIRE(entry != nullptr);
    CHECK(entry->iconName == "dec33");
    CHECK(entry->textureName == "dec33a");
    CHECK(entry->attributes == (kSpriteFrameTransparent | kSpriteFrameCenter));
    CHECK(entry->scale == Catch::Approx(1.5f));
    CHECK(entry->lightRadius == 9);
    CHECK(entry->animDuration == 1);
    CHECK(entry->animLength == 21);
    CHECK(entry->paletteId == 2);
}

TEST_CASE("SpriteFrameTable rejects truncated dsft data", "[frame_tables]")
{
    std::vector<uint8_t> data;
    appendLe32(data, 4); // claims four frames
    appendLe32(data, 0);
    data.resize(data.size() + 60, 0); // only one frame present

    SpriteFrameTable table;
    CHECK_FALSE(table.parse(data));
}

TEST_CASE("Sprite frame mirror bits select per-octant flipping", "[frame_tables]")
{
    const uint32_t attributes = kSpriteFrameMirror0 << 5;
    CHECK(spriteFrameMirrorsOctant(attributes, 5));
    CHECK_FALSE(spriteFrameMirrorsOctant(attributes, 4));
    CHECK_FALSE(spriteFrameMirrorsOctant(0, 0));
}

TEST_CASE("SpriteFrameTable rebuilds its case-insensitive icon index", "[frame_tables]")
{
    SpriteFrameTable table;
    REQUIRE(table.parseText("Goblin,Goblin01\nDragon,Dragon01\n"));
    REQUIRE(table.findEntryByIcon("GOBLIN") != nullptr);
    CHECK(table.findEntryByIcon("goblin")->textureName == "Goblin01");

    REQUIRE(table.parseText("Bat,Bat01\n"));
    CHECK(table.findEntryByIcon("Goblin") == nullptr);
    REQUIRE(table.findEntryByIcon("BAT") != nullptr);
}
