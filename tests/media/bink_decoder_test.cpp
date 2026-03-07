// SPDX-License-Identifier: MIT
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/media/bink_decoder.hpp"

using runeharbor::media::BinkBitReader;
using runeharbor::media::BinkTree;

TEST_CASE("BinkBitReader reads bits correctly", "[bink][bitreader]")
{
    // Bink reads LSB from bytes
    // 0x01 = 00000001 -> first bit is 1, then seven 0s
    // 0x81 = 10000001 -> first bit is 1, next six are 0, last is 1
    std::vector<uint8_t> data = {0x01, 0x81};
    BinkBitReader reader(data.data(), data.size());

    REQUIRE(reader.readBit() == true);
    REQUIRE(reader.readBit() == false);
    REQUIRE(reader.readBits(6) == 0);

    REQUIRE(reader.readBit() == true);
    REQUIRE(reader.readBits(6) == 0);
    REQUIRE(reader.readBit() == true);
}

TEST_CASE("BinkTree decode returns correct symbols", "[bink][huffman]")
{
    // We'll use tree index 0 which has lengths of 4 for all 16 symbols.
    // Canonical Huffman with all lengths 4:
    // Symbol 0: 0000 (code 0)
    // Symbol 1: 0001 (code 1)
    // ...

    // To decode symbol 1 (canonical code 1 at length 4):
    // Bits 0-3: 0000 (tree index 0)
    // tree.build returns early if index is 0, no shuffle bit is read.
    // Bits 4-7: 1000 (code for symbol 1, since binkTreeBits[0][1] == 0x01, read LSB-first)
    // Byte 0: 0001 0000 -> 0x10

    std::vector<uint8_t> data = {0x10};
    BinkBitReader reader(data.data(), data.size());

    BinkTree tree;
    REQUIRE(tree.build(reader, 4));

    int symbol = tree.decode(reader);
    CHECK(symbol == 1);
}
