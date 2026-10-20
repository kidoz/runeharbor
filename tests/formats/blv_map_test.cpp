// SPDX-License-Identifier: MIT
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "../../src/formats/blv_map.hpp"
#include "../../src/util/console_logger.hpp"

namespace
{

void appendU32(std::vector<uint8_t>& out, uint32_t value)
{
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&value),
               reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void appendI32(std::vector<uint8_t>& out, int32_t value)
{
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&value),
               reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void appendU16(std::vector<uint8_t>& out, uint16_t value)
{
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&value),
               reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

} // namespace

TEST_CASE("BLV parser walks spawns and outlines with 24-byte spawn records", "[blv_map]")
{
    // Minimal BLV: header + empty geometry/sector sections + one spawn + one
    // outline. A 22-byte spawn record used to shift the outline count by two
    // bytes per spawn and produce the "Outline data exceeds file" warnings.
    std::vector<uint8_t> data(136, 0);
    appendU32(data, 1); // vertex count
    appendU16(data, 0); // vertex x
    appendU16(data, 0); // vertex y
    appendU16(data, 0); // vertex z
    appendU32(data, 1); // face count
    data.insert(data.end(), 96, 0);
    // header.faceDataSize is 0, so there is no face-data section.
    data.insert(data.end(), 10, 0); // one 10-byte face texture name
    appendU32(data, 0);             // face extra count
    appendU32(data, 0);             // sector count
    // header.sectorRDataSize / sectorLRDataSize are 0.
    appendU32(data, 0);    // door count
    appendU32(data, 0);    // decoration count
    appendU32(data, 0);    // light count
    appendU32(data, 0);    // BSP node count
    appendU32(data, 1);    // spawn count
    appendI32(data, 608);  // x
    appendI32(data, 2096); // y
    appendI32(data, 0);    // z
    appendU16(data, 32);   // radius
    appendU16(data, 2);    // object type
    appendU16(data, 1);    // object index
    appendU16(data, 0);    // attributes
    appendU32(data, 7);    // group
    appendU32(data, 1);    // outline count
    appendI32(data, 0);
    appendI32(data, 1);
    appendI32(data, 2);

    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::BLVMap map(logger);
    REQUIRE(map.parse(data));

    const auto& parsed = map.getData();
    REQUIRE(parsed.spawns.size() == 1);
    REQUIRE(parsed.spawns[0].x == 608);
    REQUIRE(parsed.spawns[0].y == 2096);
    REQUIRE(parsed.spawns[0].z == 0);
    REQUIRE(parsed.spawns[0].radius == 32);
    REQUIRE(parsed.spawns[0].objectType == 2);
    REQUIRE(parsed.spawns[0].objectIndex == 1);
    REQUIRE(parsed.spawns[0].group == 7);

    REQUIRE(parsed.outlines.size() == 1);
    REQUIRE(parsed.outlines[0].x == 0);
    REQUIRE(parsed.outlines[0].y == 1);
    REQUIRE(parsed.outlines[0].z == 2);
}
