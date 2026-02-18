// SPDX-License-Identifier: MIT
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "../../src/media/smacker_decoder.hpp"

using runeharbor::media::SmackerAudioFrame;
using runeharbor::media::SmackerDecoder;
using runeharbor::media::SmackerFrame;
using runeharbor::media::SmackerHeader;

namespace
{
struct BitWriter
{
    std::vector<uint8_t> data;
    int bitPos = 0;

    void writeBit(bool bit)
    {
        if (bitPos == 0)
        {
            data.push_back(0);
        }

        if (bit)
        {
            data.back() |= static_cast<uint8_t>(1u << bitPos);
        }

        bitPos = (bitPos + 1) & 7;
    }

    void writeBits(uint32_t value, int count)
    {
        for (int i = 0; i < count; i++)
        {
            writeBit(((value >> i) & 1u) != 0);
        }
    }
};

void appendU32LE(std::vector<uint8_t>& data, uint32_t value)
{
    data.push_back(static_cast<uint8_t>(value & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

std::vector<uint8_t> buildSmk(const SmackerHeader& header, const std::vector<uint32_t>& frameSizes,
                              const std::vector<uint8_t>& frameTypes,
                              const std::vector<uint8_t>& frameData)
{
    std::vector<uint8_t> data(sizeof(SmackerHeader), 0);
    std::memcpy(data.data(), &header, sizeof(SmackerHeader));

    for (uint32_t size : frameSizes)
    {
        appendU32LE(data, size);
    }
    data.insert(data.end(), frameTypes.begin(), frameTypes.end());
    data.insert(data.end(), frameData.begin(), frameData.end());
    return data;
}

} // namespace

TEST_CASE("SmackerDecoder parses keyframe flags from frame sizes", "[smacker][keyframe]")
{
    SmackerHeader header{};
    std::memcpy(header.magic, "SMK2", 4);
    header.width = 1;
    header.height = 1;
    header.frameCount = 3;
    header.frameRate = 1000;
    header.treesSize = 0;
    header.mmapSize = 0;
    header.mclrSize = 0;
    header.fullSize = 0;
    header.typeSize = 0;

    std::vector<uint32_t> frameSizes = {
        0x00000001u, // keyframe
        0x00000000u,
        0x00000001u // keyframe
    };
    std::vector<uint8_t> frameTypes = {0, 0, 0};
    std::vector<uint8_t> frameData;

    auto data = buildSmk(header, frameSizes, frameTypes, frameData);

    SmackerDecoder decoder;
    REQUIRE(decoder.load(data));

    SmackerFrame frame;
    REQUIRE(decoder.decodeFrame(0, frame));
    REQUIRE(frame.isKeyframe);

    REQUIRE(decoder.decodeFrame(1, frame));
    REQUIRE_FALSE(frame.isKeyframe);

    REQUIRE(decoder.decodeFrame(2, frame));
    REQUIRE(frame.isKeyframe);
}

TEST_CASE("SmackerDecoder DPCM uses wraparound deltas for 16-bit audio", "[smacker][audio]")
{
    SmackerHeader header{};
    std::memcpy(header.magic, "SMK2", 4);
    header.width = 1;
    header.height = 1;
    header.frameCount = 1;
    header.frameRate = 1000;
    header.treesSize = 0;
    header.mmapSize = 0;
    header.mclrSize = 0;
    header.fullSize = 0;
    header.typeSize = 0;
    header.audioRate[0] = 0xE0000000u | 8000u; // has audio, compressed, 16-bit, mono, 8kHz

    BitWriter writer;
    writer.writeBit(true);  // data present
    writer.writeBit(false); // mono
    writer.writeBit(true);  // 16-bit

    auto writeSingleLeafTree = [&writer](uint8_t value)
    {
        writer.writeBit(true);  // tree present
        writer.writeBit(false); // leaf
        writer.writeBits(value, 8);
        writer.writeBit(false); // terminator
    };

    writeSingleLeafTree(0xFF); // low byte delta
    writeSingleLeafTree(0xFF); // high byte delta

    writer.writeBits(0x00, 8); // base high byte
    writer.writeBits(0x00, 8); // base low byte

    std::vector<uint8_t> payload;
    appendU32LE(payload, 4); // uncompressed length in bytes (2 samples * 2 bytes)
    payload.insert(payload.end(), writer.data.begin(), writer.data.end());

    uint32_t audioSize = static_cast<uint32_t>(payload.size() + 4);
    std::vector<uint8_t> frameData;
    appendU32LE(frameData, audioSize);
    frameData.insert(frameData.end(), payload.begin(), payload.end());

    while ((frameData.size() & 3u) != 0u)
    {
        frameData.push_back(0);
    }

    uint32_t frameSizeFlagged = static_cast<uint32_t>(frameData.size()) | 0x1u;
    std::vector<uint32_t> frameSizes = {frameSizeFlagged};
    std::vector<uint8_t> frameTypes = {0x02}; // audio track 0 present

    auto data = buildSmk(header, frameSizes, frameTypes, frameData);

    SmackerDecoder decoder;
    REQUIRE(decoder.load(data));

    SmackerAudioFrame audio;
    REQUIRE(decoder.decodeAudio(0, 0, audio));
    REQUIRE(audio.samples.size() == 2);
    REQUIRE(audio.samples[0] == 0);
    REQUIRE(audio.samples[1] == static_cast<int16_t>(-1));
}

TEST_CASE("SmackerDecoder DPCM uses wraparound deltas for 8-bit audio", "[smacker][audio]")
{
    SmackerHeader header{};
    std::memcpy(header.magic, "SMK2", 4);
    header.width = 1;
    header.height = 1;
    header.frameCount = 1;
    header.frameRate = 1000;
    header.treesSize = 0;
    header.mmapSize = 0;
    header.mclrSize = 0;
    header.fullSize = 0;
    header.typeSize = 0;
    header.audioRate[0] = 0x40000000u | 0x80000000u |
                          8000u; // has audio, compressed, 8-bit (is16Bit=false), mono, 8kHz

    BitWriter writer;
    writer.writeBit(true);  // data present
    writer.writeBit(false); // mono
    writer.writeBit(false); // 8-bit

    auto writeSingleLeafTree = [&writer](uint8_t value)
    {
        writer.writeBit(true);  // tree present
        writer.writeBit(false); // leaf
        writer.writeBits(value, 8);
        writer.writeBit(false); // terminator
    };

    writeSingleLeafTree(0x01); // delta (1)

    writer.writeBits(128, 8); // base value (128 = silence in 8-bit unsigned Smacker)

    std::vector<uint8_t> payload;
    appendU32LE(payload, 2); // uncompressed length in bytes (2 samples * 1 byte)
    payload.insert(payload.end(), writer.data.begin(), writer.data.end());

    uint32_t audioSize = static_cast<uint32_t>(payload.size() + 4);
    std::vector<uint8_t> frameData;
    appendU32LE(frameData, audioSize);
    frameData.insert(frameData.end(), payload.begin(), payload.end());

    while ((frameData.size() & 3u) != 0u)
    {
        frameData.push_back(0);
    }

    uint32_t frameSizeFlagged = static_cast<uint32_t>(frameData.size()) | 0x1u;
    std::vector<uint32_t> frameSizes = {frameSizeFlagged};
    std::vector<uint8_t> frameTypes = {0x02}; // audio track 0 present

    auto data = buildSmk(header, frameSizes, frameTypes, frameData);

    SmackerDecoder decoder;
    REQUIRE(decoder.load(data));

    SmackerAudioFrame audio;
    REQUIRE(decoder.decodeAudio(0, 0, audio));
    REQUIRE(audio.samples.size() == 2);
    // Base 128 -> (128-128)<<8 = 0
    REQUIRE(audio.samples[0] == 0);
    // 128 + delta(1) = 129 -> (129-128)<<8 = 1 << 8 = 256
    REQUIRE(audio.samples[1] == 256);
}

TEST_CASE("SmackerDecoder decodes FILL block", "[smacker][video]")
{
    SmackerHeader header{};
    std::memcpy(header.magic, "SMK2", 4);
    header.width = 4;
    header.height = 4;
    header.frameCount = 1;
    header.frameRate = 1000;

    const uint8_t color = 42;
    // typeDesc for a FILL block with run length 1.
    // bits 0-1: block type (3 for FILL)
    // bits 2-7: run length - 1 (0 for run length 1)
    // bits 8-15: color for the fill
    const uint16_t typeDesc = (static_cast<uint16_t>(color) << 8) | 3u;
    const uint8_t low_byte = typeDesc & 0xFFu;
    const uint8_t high_byte = (typeDesc >> 8) & 0xFFu;

    BitWriter treeWriter;
    // MMap, MClr, Full trees (all empty)
    treeWriter.writeBit(false);
    treeWriter.writeBit(false);
    treeWriter.writeBit(false);

    // --- Type Tree ---
    treeWriter.writeBit(true); // BigHuffmanTree present

    // low8 tree (for leaf value)
    treeWriter.writeBit(true);         // present
    treeWriter.writeBit(false);        // leaf
    treeWriter.writeBits(low_byte, 8); // value
    treeWriter.writeBit(false);        // terminator

    // hi8 tree (for leaf value)
    treeWriter.writeBit(true);          // present
    treeWriter.writeBit(false);         // leaf
    treeWriter.writeBits(high_byte, 8); // value
    treeWriter.writeBit(false);         // terminator

    // Cache values (3 * 16 bits)
    treeWriter.writeBits(0, 48);

    // Big tree structure: a single leaf.
    treeWriter.writeBit(false); // is a leaf
    treeWriter.writeBit(false); // terminator

    header.treesSize = static_cast<uint32_t>(treeWriter.data.size());
    header.mmapSize = 1; // 1 byte for empty tree flag
    header.mclrSize = 1;
    header.fullSize = 1;
    header.typeSize = header.treesSize - 3;

    // Frame data must not be empty (after masking flags) to trigger video decoding,
    // and must be flagged as a keyframe (bit 0 set).
    std::vector<uint8_t> videoData = {0x00, 0x00, 0x00, 0x00}; // 4 dummy bytes
    std::vector<uint32_t> frameSizes = {static_cast<uint32_t>(videoData.size()) | 1u};
    std::vector<uint8_t> frameTypes = {0}; // No palette change, no audio

    auto data = buildSmk(header, frameSizes, frameTypes, treeWriter.data);
    data.insert(data.end(), videoData.begin(), videoData.end());

    SmackerDecoder decoder;
    REQUIRE(decoder.load(data));

    SmackerFrame frame;
    REQUIRE(decoder.decodeFrame(0, frame));
    REQUIRE(frame.width == 4);
    REQUIRE(frame.height == 4);
    REQUIRE(frame.pixels.size() == 16);

    for (int i = 0; i < 16; ++i)
    {
        REQUIRE(frame.pixels[i] == color);
    }
}
