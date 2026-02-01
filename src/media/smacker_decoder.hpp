// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace runeharbor::media
{

/**
 * Smacker Video Decoder
 *
 * Decodes RAD Game Tools Smacker video format (.smk)
 * Based on publicly available format documentation from MultimediaWiki.
 *
 * Smacker Header (104 bytes):
 *   0x00: 4 bytes - Magic "SMK2" or "SMK4"
 *   0x04: 4 bytes - Width
 *   0x08: 4 bytes - Height
 *   0x0C: 4 bytes - Frame count
 *   0x10: 4 bytes - Frame rate (signed, special encoding)
 *   0x14: 4 bytes - Flags
 *   0x18: 28 bytes - Audio sizes for 7 audio tracks
 *   0x34: 4 bytes - Trees size
 *   0x38: 4 bytes - MMap size
 *   0x3C: 4 bytes - MClr size
 *   0x40: 4 bytes - Full size
 *   0x44: 4 bytes - Type size
 *   0x48: 28 bytes - Audio rates for 7 audio tracks
 *   0x64: 4 bytes - Dummy
 */

struct SmackerHeader
{
    char magic[4];        // "SMK2" or "SMK4"
    uint32_t width;
    uint32_t height;
    uint32_t frameCount;
    int32_t frameRate;    // Signed, special encoding
    uint32_t flags;
    uint32_t audioSize[7];
    uint32_t treesSize;
    uint32_t mmapSize;
    uint32_t mclrSize;
    uint32_t fullSize;
    uint32_t typeSize;
    uint32_t audioRate[7];
    uint32_t dummy;
};

struct SmackerFrame
{
    std::vector<uint8_t> pixels;  // Indexed color (palette)
    uint32_t width;
    uint32_t height;
    bool isKeyframe;
};

/**
 * Audio track configuration
 */
struct SmackerAudioInfo
{
    uint32_t sampleRate = 0;
    bool is16Bit = false;
    bool isStereo = false;
    bool hasAudio = false;
    bool isCompressed = false;
};

/**
 * Decoded audio samples for a frame
 */
struct SmackerAudioFrame
{
    std::vector<int16_t> samples;  // Interleaved stereo if applicable
    uint32_t sampleRate = 0;
    uint8_t channels = 1;
    bool is16Bit = true;
};

/**
 * BitReader - Read variable bit lengths from packed data
 */
class BitReader
{
  public:
    BitReader(const uint8_t* data, size_t sizeBytes);

    uint32_t readBits(int count);
    bool readBit();
    void skipBits(int count);
    bool atEnd() const;
    size_t bitsRemaining() const;
    size_t position() const { return bitPos_; }

  private:
    const uint8_t* data_;
    size_t bitPos_ = 0;
    size_t maxBits_;
};

/**
 * SmackerDecoder - Full Smacker video decoder
 */
class SmackerDecoder
{
  public:
    SmackerDecoder();
    ~SmackerDecoder();

    SmackerDecoder(const SmackerDecoder&) = delete;
    SmackerDecoder& operator=(const SmackerDecoder&) = delete;

    // Load video from memory buffer
    bool load(const uint8_t* data, size_t size);
    bool load(const std::vector<uint8_t>& data);

    // Video info
    uint32_t width() const { return header_.width; }
    uint32_t height() const { return header_.height; }
    uint32_t frameCount() const { return header_.frameCount; }
    double frameRate() const;
    double durationMs() const;
    bool isVersion4() const { return isV4_; }

    // Palette (256 RGB entries = 768 bytes)
    const uint8_t* palette() const { return palette_.data(); }

    // Audio info
    SmackerAudioInfo getAudioInfo(int track = 0) const;
    bool hasAudio(int track = 0) const;

    // Decode frames
    bool decodeFrame(uint32_t frameIndex, SmackerFrame& outFrame);

    // Decode audio for a frame
    bool decodeAudio(uint32_t frameIndex, int track, SmackerAudioFrame& outAudio);

    // Get current frame as RGBA (convenience)
    std::vector<uint8_t> getFrameRGBA(uint32_t frameIndex);

    // Reset decoder state
    void reset();

  private:
    // Block types
    enum BlockType
    {
        BLOCK_MONO = 0,  // 2-color with bitmap
        BLOCK_FULL = 1,  // Full color
        BLOCK_SKIP = 2,  // Copy from previous
        BLOCK_FILL = 3   // Solid color
    };

    SmackerHeader header_;
    std::vector<uint8_t> data_;
    std::vector<uint32_t> frameSizes_;
    std::vector<uint8_t> frameTypes_;
    std::vector<uint8_t> palette_;      // 256 * 3 = 768 bytes
    std::vector<uint8_t> frameBuffer_;  // Current decoded frame (indexed)

    size_t dataOffset_ = 0;  // Start of frame data in buffer
    size_t treesOffset_ = 0; // Start of Huffman trees
    bool isV4_ = false;      // SMK4 format

    // Huffman trees for decoding (16-bit values)
    struct BigHuffmanTree;
    std::unique_ptr<BigHuffmanTree> mmapTree_;  // Block type/color
    std::unique_ptr<BigHuffmanTree> mclrTree_;  // Mono colors
    std::unique_ptr<BigHuffmanTree> fullTree_;  // Full block colors
    std::unique_ptr<BigHuffmanTree> typeTree_;  // Block descriptors

    // Cache for decoded frames to support seeking
    uint32_t lastDecodedFrame_ = UINT32_MAX;

    bool parseHeader();
    bool buildTrees();
    bool decodeFrameInternal(uint32_t frameIndex);
    bool decodeVideoData(BitReader& bits, bool hasKeyframe);
    void decodePalette(const uint8_t* data, size_t size);

    // Audio decoding
    bool decodeAudioTrack(const uint8_t* data, size_t size, int track, SmackerAudioFrame& outAudio);

    // Block decoders
    void decodeBlockSkip(uint32_t x, uint32_t y);
    void decodeBlockFill(uint32_t x, uint32_t y, uint8_t color);
    void decodeBlockMono(uint32_t x, uint32_t y, uint8_t c0, uint8_t c1, uint16_t bitmap);
    void decodeBlockFull(BitReader& bits, uint32_t x, uint32_t y);
};

} // namespace runeharbor::media
