// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace runeharbor::media
{

/**
 * Bink Video Decoder
 *
 * Bink Header (44 bytes):
 *   0x00: 4 bytes - Magic "BIKx" where x = version (b,d,f,g,h,i)
 *   0x04: 4 bytes - File size (excluding first 8 bytes)
 *   0x08: 4 bytes - Frame count
 *   0x0C: 4 bytes - Maximum frame size
 *   0x10: 4 bytes - Frame count (repeated)
 *   0x14: 4 bytes - Video width
 *   0x18: 4 bytes - Video height
 *   0x1C: 4 bytes - FPS dividend
 *   0x20: 4 bytes - FPS divider
 *   0x24: 4 bytes - Video flags
 *   0x28: 4 bytes - Audio track count
 */

struct BinkHeader
{
    char magic[4];     // "BIKx" where x = version
    uint32_t fileSize; // File size - 8
    uint32_t frameCount;
    uint32_t maxFrameSize;
    uint32_t frameCount2; // Repeated
    uint32_t width;
    uint32_t height;
    uint32_t fpsDividend;
    uint32_t fpsDivider;
    uint32_t flags;
    uint32_t audioTrackCount;
};

struct BinkFrame
{
    std::vector<uint8_t> pixels; // RGBA pixels (fallback)
    uint32_t width;
    uint32_t height;
    bool isKeyframe;
};

struct BinkYUVPlanes
{
    const uint8_t* y;
    const uint8_t* u;
    const uint8_t* v;
    uint32_t yStride;
    uint32_t uvStride;
    uint32_t width;
    uint32_t height;
};

/**
 * Bink audio track info
 */
struct BinkAudioInfo
{
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    bool hasAudio = false;
    bool useDCT = false; // true = DCT, false = RDFT
};

/**
 * Decoded audio samples for a frame
 */
struct BinkAudioFrame
{
    std::vector<int16_t> samples; // Interleaved stereo if applicable
    uint32_t sampleRate = 0;
    uint8_t channels = 1;
};

/**
 * Bink Bundle Types - Different data streams in Bink format
 */
enum class BinkBundleType
{
    BlockTypes = 0,
    SubBlockTypes,
    Colors,
    Pattern,
    MotionX,
    MotionY,
    IntraDC,
    InterDC,
    Run,
    Count
};

/**
 * Bink Block Types
 */
enum BinkBlockType
{
    BINK_BLOCK_SKIP = 0,    // Copy from previous frame
    BINK_BLOCK_SCALED = 1,  // 16x16 scaled block
    BINK_BLOCK_MOTION = 2,  // Motion compensation
    BINK_BLOCK_RUN = 3,     // RLE with pattern
    BINK_BLOCK_RESIDUE = 4, // Motion + residue
    BINK_BLOCK_INTRA = 5,   // Intra DCT
    BINK_BLOCK_FILL = 6,    // Solid color
    BINK_BLOCK_INTER = 7,   // Inter DCT
    BINK_BLOCK_PATTERN = 8, // 2-color pattern
    BINK_BLOCK_RAW = 9      // Raw 64 values
};

/**
 * BitReader for Bink - reads LSB from 32-bit LE words
 */
class BinkBitReader
{
  public:
    BinkBitReader(const uint8_t* data, size_t sizeBytes);

    uint32_t readBits(int count);
    bool readBit();
    bool peekBit(int offset);
    void skipBits(int count);
    void align32();
    bool atEnd() const;
    size_t bitsRemaining() const;
    size_t getPos() const { return bitPos_; }

  private:
    const uint8_t* data_;
    size_t bitPos_ = 0;
    size_t maxBits_;
};

/**
 * Bink Huffman Tree
 */
class BinkTree
{
  public:
    bool build(BinkBitReader& bits, int maxDepth);
    int decode(BinkBitReader& bits) const;
    int getSymbol(int i) const { return symbols_[i]; }
    int getVlcNum() const { return vlcNum_; }

  private:
    static constexpr int MAX_SYMBOLS = 16;
    int symbols_[MAX_SYMBOLS];
    int vlcNum_ = 0;
};

/**
 * Bundle - data stream for specific value types
 */
class BinkBundle
{
  public:
    void reset();
    void buildTree(BinkBitReader& bits, BinkBundleType type);
    bool readBlockTypes(BinkBitReader& bits, int lenBits);
    bool readColors(BinkBitReader& bits, int lenBits);
    bool readPatterns(BinkBitReader& bits, int lenBits);
    bool readMotionValues(BinkBitReader& bits, int lenBits);
    bool readDCs(BinkBitReader& bits, int lenBits, int startBits, bool hasSign);
    bool readRuns(BinkBitReader& bits, int lenBits);
    int getValue();
    int peekValue(size_t offset) const;

  private:
    std::vector<int> data_;
    size_t dataLen_ = 0;
    size_t readPos_ = 0;
    bool eof_ = false;
    BinkTree tree_;
    BinkTree treeHigh_[16];
    int lastColorHigh_ = 0;
};

/**
 * BinkDecoder - Full Bink video decoder
 */
class BinkDecoder
{
  public:
    BinkDecoder();
    ~BinkDecoder();

    BinkDecoder(const BinkDecoder&) = delete;
    BinkDecoder& operator=(const BinkDecoder&) = delete;

    // Load video from memory buffer
    bool load(const uint8_t* data, size_t size);
    bool load(const std::vector<uint8_t>& data);

    // Video info
    uint32_t width() const { return header_.width; }
    uint32_t height() const { return header_.height; }
    uint32_t frameCount() const { return header_.frameCount; }
    double frameRate() const;
    double durationMs() const;
    char version() const { return header_.magic[3]; }

    // Audio info
    uint32_t audioTrackCount() const { return header_.audioTrackCount; }
    BinkAudioInfo getAudioInfo(uint32_t track) const;
    bool hasAudio(uint32_t track = 0) const;

    // Decode frames
    bool decodeFrame(uint32_t frameIndex, BinkFrame& outFrame);

    // Decode audio for a frame
    bool decodeAudio(uint32_t frameIndex, uint32_t track, BinkAudioFrame& outAudio);

    // Get current frame as RGBA (convenience)
    std::vector<uint8_t> getFrameRGBA(uint32_t frameIndex);

    // Get current frame as YUV planes (performance)
    std::optional<BinkYUVPlanes> getYUVPlanes() const;

    // Reset decoder state
    void reset();

  private:
    BinkHeader header_;
    std::vector<uint8_t> data_;
    std::vector<uint32_t> frameOffsets_; // Frame index table
    std::vector<bool> frameKeyFlags_;    // Keyframe flags

    // Frame buffers (YUV planes)
    std::vector<uint8_t> planeY_; // Luma
    std::vector<uint8_t> planeU_; // Chroma U
    std::vector<uint8_t> planeV_; // Chroma V
    std::vector<uint8_t> prevY_;  // Previous frame luma
    std::vector<uint8_t> prevU_;  // Previous frame chroma U
    std::vector<uint8_t> prevV_;  // Previous frame chroma V

    // Plane dimensions
    uint32_t planeWidthY_ = 0;
    uint32_t planeHeightY_ = 0;
    uint32_t planeWidthC_ = 0;
    uint32_t planeHeightC_ = 0;

    // Bundles for different data types
    BinkBundle bundles_[static_cast<int>(BinkBundleType::Count)];

    // Cache for seeking
    uint32_t lastDecodedFrame_ = UINT32_MAX;

    // Audio track info
    struct AudioTrackInfo
    {
        uint32_t sampleRate = 0;
        uint16_t channels = 0;
        bool isDCT = false;
        uint32_t trackId = 0;
    };
    std::vector<AudioTrackInfo> audioTracks_;

    // Audio decoding state
    std::vector<float> audioOverlap_;
    size_t audioFrameSize_ = 0;
    size_t audioOverlapSize_ = 0;
    bool audioFirst_ = true;

    bool parseHeader();
    bool parseFrameIndex();
    bool decodeFrameInternal(uint32_t frameIndex);
    bool decodePlane(BinkBitReader& bits, uint8_t* plane, uint8_t* prev, uint32_t width,
                     uint32_t height, bool isChroma);
    bool readBundle(BinkBitReader& bits, BinkBundleType type);
    int readDCTCoeffs(BinkBitReader& bits, int32_t block[64], int* coefCount, int coefIdx[64], int q);
    void unquantizeDCTCoeffs(int32_t block[64], const int32_t quant[64], int coefCount, int coefIdx[64]);

    // Block decoders
    void decodeBlockSkip(uint8_t* dst, const uint8_t* prev, int stride);
    void decodeBlockFill(uint8_t* dst, int stride, uint8_t color);
    void decodeBlockRun(uint8_t* dst, int stride, BinkBitReader& bits);
    void decodeBlockPattern(uint8_t* dst, int stride, uint8_t c0, uint8_t c1, uint8_t pattern);
    void decodeBlockRaw(uint8_t* dst, int stride, BinkBitReader& bits);
    void decodeBlockMotion(uint8_t* dst, const uint8_t* prev, int stride, int mvX, int mvY);
    void decodeBlockIntraDCT(uint8_t* dst, int stride, BinkBitReader& bits, int dc);
    void decodeBlockInterDCT(uint8_t* dst, const uint8_t* prev, int stride, BinkBitReader& bits,
                             int mvX, int mvY, int dc);
    void decodeBlockScaled(uint8_t* dst, const uint8_t* prev, int stride, BinkBitReader& bits,
                           uint32_t planeWidth, uint32_t planeHeight);
    void decodeBlockResidue(uint8_t* dst, const uint8_t* prev, int stride, BinkBitReader& bits,
                            int mvX, int mvY);
    int readResidue(BinkBitReader& bits, int16_t block[64], int masksCount);

    // IDCT
    void idctPut(uint8_t* dst, int stride, const int32_t* block);
    void idctAdd(uint8_t* dst, int stride, const int32_t* block);

    // YUV to RGB conversion
    void convertYUVToRGBA(std::vector<uint8_t>& rgba);

    // Audio decoding
    bool decodeAudioTrack(BinkBitReader& bits, uint32_t track, BinkAudioFrame& outAudio);
    void rdft(float* data, size_t n, bool inverse);
    void dct(float* data, size_t n, bool inverse);
};

} // namespace runeharbor::media
