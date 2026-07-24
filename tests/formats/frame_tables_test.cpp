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

TEST_CASE("SpriteFrameTable parses MM7 dsft records after leading frame field", "[frame_tables]")
{
    std::vector<uint8_t> data;
    appendLe32(data, 1);          // count
    appendLe32(data, 0x00000002); // leading frame flags
    appendFixedName(data, "dec33");
    appendFixedName(data, "dec33a");
    data.resize(data.size() + 16, 0); // reserved
    appendLe16(data, 1);              // animDuration
    appendLe16(data, 21);             // animLength
    appendLe16(data, 0);              // animOffset
    appendLe16(data, 0x0080);         // attributes
    appendLe32(data, 0);
    appendLe16(data, 2); // paletteId
    appendLe16(data, 0); // paletteIndex

    SpriteFrameTable table;
    REQUIRE(table.parse(data));
    REQUIRE(table.entries().size() == 1);

    const auto* entry = table.findEntryByIcon("DEC33");
    REQUIRE(entry != nullptr);
    CHECK(entry->iconName == "dec33");
    CHECK(entry->textureName == "dec33a");
    CHECK(entry->attributes == 0x0080);
    CHECK(entry->animLength == 21);
    CHECK(entry->paletteId == 2);
}
