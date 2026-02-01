// SPDX-License-Identifier: MIT
// Based on publicly available Bink format documentation and FFmpeg's implementation
#include "bink_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace runeharbor::media
{

// ============================================================================
// Bink Constants
// ============================================================================

// Predefined Huffman trees (16 trees with 16 symbols each)
// From MultimediaWiki / FFmpeg bink.c
static const uint8_t binkTreeLens[16][16] = {
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
    {1, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {2, 2, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5},
    {3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5},
    {3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5},
    {3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5},
    {3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4},
};

// Run length table for RLE blocks (reserved for future use)
// static const uint8_t binkRunBits[64] = {
//     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
//     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
//     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
// };

// Scan order for DCT (zigzag)
static const uint8_t binkScan[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

// Quantization table for DCT (reserved for future use)
// static const uint8_t binkQuantTable[64] = {
//     16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
//     16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
//     16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
//     16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
// };

// Pattern scan order for 8x8 blocks
static const uint8_t binkPatternScan[64] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

// ============================================================================
// BinkBitReader Implementation
// ============================================================================

BinkBitReader::BinkBitReader(const uint8_t* data, size_t sizeBytes)
    : data_(data), bitPos_(0), maxBits_(sizeBytes * 8)
{
}

bool BinkBitReader::readBit()
{
    if (bitPos_ >= maxBits_)
    {
        return false;
    }

    // Bink reads LSB from 32-bit LE words
    size_t byteIdx = bitPos_ / 8;
    size_t bitIdx = bitPos_ % 8;
    bitPos_++;

    return (data_[byteIdx] >> bitIdx) & 1;
}

uint32_t BinkBitReader::readBits(int count)
{
    uint32_t result = 0;
    for (int i = 0; i < count; i++)
    {
        if (readBit())
        {
            result |= (1u << i);
        }
    }
    return result;
}

void BinkBitReader::skipBits(int count)
{
    bitPos_ += count;
    if (bitPos_ > maxBits_)
    {
        bitPos_ = maxBits_;
    }
}

void BinkBitReader::align32()
{
    size_t remainder = bitPos_ % 32;
    if (remainder != 0)
    {
        bitPos_ += 32 - remainder;
    }
}

bool BinkBitReader::atEnd() const
{
    return bitPos_ >= maxBits_;
}

size_t BinkBitReader::bitsRemaining() const
{
    return bitPos_ < maxBits_ ? maxBits_ - bitPos_ : 0;
}

// ============================================================================
// BinkTree Implementation
// ============================================================================

bool BinkTree::build(BinkBitReader& bits, int /*maxDepth*/)
{
    // Read tree index (4 bits)
    int treeIdx = static_cast<int>(bits.readBits(4));

    // Initialize symbols in order
    for (int i = 0; i < MAX_SYMBOLS; i++)
    {
        symbols_[i] = i;
        lengths_[i] = binkTreeLens[treeIdx][i];
    }
    numSymbols_ = MAX_SYMBOLS;

    // Check if symbols need reordering
    if (bits.readBit())
    {
        // Shuffle depth (number of merge passes)
        int depth = static_cast<int>(bits.readBits(2)) + 1;

        // Reorder symbols by swapping pairs
        for (int i = 0; i < depth; i++)
        {
            for (int j = 0; j < (1 << i); j++)
            {
                int idx1 = j * 2;
                int idx2 = j * 2 + 1;
                if (idx2 < MAX_SYMBOLS && bits.readBit())
                {
                    std::swap(symbols_[idx1], symbols_[idx2]);
                }
            }
        }
    }

    return true;
}

int BinkTree::decode(BinkBitReader& bits) const
{
    if (numSymbols_ == 0)
    {
        return 0;
    }

    // Simple lookup based on code lengths
    uint32_t code = 0;
    int codeLen = 0;

    for (int i = 0; i < MAX_SYMBOLS && !bits.atEnd(); i++)
    {
        int len = lengths_[i];
        while (codeLen < len && !bits.atEnd())
        {
            code = (code << 1) | (bits.readBit() ? 1 : 0);
            codeLen++;
        }

        // Check if this code matches
        if (codeLen == len)
        {
            // Found matching code length, check actual code
            return symbols_[i];
        }
    }

    return 0;
}

// ============================================================================
// BinkBundle Implementation
// ============================================================================

void BinkBundle::reset()
{
    data_.clear();
    readPos_ = 0;
}

bool BinkBundle::decode(BinkBitReader& bits, BinkBundleType type)
{
    reset();

    // Read count of values
    uint32_t count = bits.readBits(13);
    if (count == 0)
    {
        return true;
    }

    data_.reserve(count);

    // Build tree for this bundle
    tree_.build(bits, 4);

    // Decode values based on bundle type
    for (uint32_t i = 0; i < count && !bits.atEnd(); i++)
    {
        int val = tree_.decode(bits);

        // Handle RLE for some bundle types
        if (type == BinkBundleType::BlockTypes || type == BinkBundleType::SubBlockTypes)
        {
            if (val >= 12)
            {
                // RLE run
                int runLen = (val >= 12) ? (1 << (val - 11)) : 1;
                int runVal = tree_.decode(bits);
                for (int j = 0; j < runLen && i + j < count; j++)
                {
                    data_.push_back(runVal);
                }
                i += runLen - 1;
            }
            else
            {
                data_.push_back(val);
            }
        }
        else
        {
            data_.push_back(val);
        }
    }

    return true;
}

int BinkBundle::getValue()
{
    if (readPos_ < data_.size())
    {
        return data_[readPos_++];
    }
    return 0;
}

// ============================================================================
// BinkDecoder Implementation
// ============================================================================

BinkDecoder::BinkDecoder()
{
    std::memset(&header_, 0, sizeof(header_));
}

BinkDecoder::~BinkDecoder() = default;

bool BinkDecoder::load(const uint8_t* data, size_t size)
{
    if (!data || size < sizeof(BinkHeader))
    {
        return false;
    }

    data_.assign(data, data + size);
    return parseHeader() && parseFrameIndex();
}

bool BinkDecoder::load(const std::vector<uint8_t>& data)
{
    return load(data.data(), data.size());
}

bool BinkDecoder::parseHeader()
{
    if (data_.size() < sizeof(BinkHeader))
    {
        return false;
    }

    std::memcpy(&header_, data_.data(), sizeof(BinkHeader));

    // Validate magic: "BIKx" where x = version letter
    if (header_.magic[0] != 'B' || header_.magic[1] != 'I' || header_.magic[2] != 'K')
    {
        return false;
    }

    char ver = header_.magic[3];
    if (ver != 'b' && ver != 'd' && ver != 'f' && ver != 'g' && ver != 'h' && ver != 'i')
    {
        return false;
    }

    // Validate dimensions
    if (header_.width == 0 || header_.height == 0 || header_.width > 4096 || header_.height > 4096)
    {
        return false;
    }

    if (header_.frameCount == 0 || header_.frameCount > 1000000)
    {
        return false;
    }

    // Initialize plane buffers
    // Luma plane is full resolution, chroma is half
    planeWidthY_ = (header_.width + 7) & ~7;   // Align to 8
    planeHeightY_ = (header_.height + 7) & ~7;
    planeWidthC_ = planeWidthY_ / 2;
    planeHeightC_ = planeHeightY_ / 2;

    planeY_.resize(planeWidthY_ * planeHeightY_, 128);
    planeU_.resize(planeWidthC_ * planeHeightC_, 128);
    planeV_.resize(planeWidthC_ * planeHeightC_, 128);
    prevY_.resize(planeWidthY_ * planeHeightY_, 128);
    prevU_.resize(planeWidthC_ * planeHeightC_, 128);
    prevV_.resize(planeWidthC_ * planeHeightC_, 128);

    return true;
}

bool BinkDecoder::parseFrameIndex()
{
    size_t headerSize = 44;
    size_t offset = headerSize;

    // Parse audio track info (if present)
    // Based on FFmpeg's bink.c:
    // - For each track: 2 bytes sample rate + 2 bytes flags
    // - Channels determined from STEREO flag (0x2000)
    // - Total: 4 bytes per audio track

    audioTracks_.resize(header_.audioTrackCount);

    if (header_.audioTrackCount > 0)
    {
        for (uint32_t t = 0; t < header_.audioTrackCount && offset + 4 <= data_.size(); t++)
        {
            // Read sample rate (2 bytes, little-endian)
            uint16_t sampleRate;
            std::memcpy(&sampleRate, data_.data() + offset, 2);
            audioTracks_[t].sampleRate = sampleRate;
            offset += 2;

            // Read flags (2 bytes)
            // Bit 12 (0x1000) = use DCT
            // Bit 13 (0x2000) = stereo
            // Bit 14 (0x4000) = 16-bit
            uint16_t flags;
            std::memcpy(&flags, data_.data() + offset, 2);
            audioTracks_[t].isDCT = (flags & 0x1000) != 0;
            audioTracks_[t].channels = (flags & 0x2000) ? 2 : 1;
            offset += 2;
        }

        // Initialize audio frame size based on sample rate
        if (!audioTracks_.empty() && audioTracks_[0].sampleRate > 0)
        {
            uint32_t sr = audioTracks_[0].sampleRate;
            if (sr < 22050)
            {
                audioFrameSize_ = 2048;
            }
            else if (sr < 44100)
            {
                audioFrameSize_ = 4096;
            }
            else
            {
                audioFrameSize_ = 8192;
            }
            audioOverlapSize_ = audioFrameSize_ / 16;
        }
    }

    // Frame index table
    frameOffsets_.resize(header_.frameCount + 1);
    frameKeyFlags_.resize(header_.frameCount, false);

    size_t indexSize = (header_.frameCount + 1) * sizeof(uint32_t);
    if (offset + indexSize > data_.size())
    {
        return false;
    }

    for (uint32_t i = 0; i <= header_.frameCount; i++)
    {
        uint32_t entry;
        std::memcpy(&entry, data_.data() + offset, 4);
        offset += 4;

        // Bit 0 is keyframe flag
        frameOffsets_[i] = entry & ~1u;
        if (i < header_.frameCount)
        {
            frameKeyFlags_[i] = (entry & 1) != 0;
        }
    }

    lastDecodedFrame_ = UINT32_MAX;
    return true;
}

double BinkDecoder::frameRate() const
{
    if (header_.fpsDivider == 0)
    {
        return 15.0;
    }
    return static_cast<double>(header_.fpsDividend) / header_.fpsDivider;
}

double BinkDecoder::durationMs() const
{
    double fps = frameRate();
    if (fps <= 0)
    {
        fps = 15.0;
    }
    return (header_.frameCount * 1000.0) / fps;
}

bool BinkDecoder::decodeFrame(uint32_t frameIndex, BinkFrame& outFrame)
{
    if (frameIndex >= header_.frameCount)
    {
        return false;
    }

    // Find nearest keyframe before target
    uint32_t startFrame = 0;
    if (lastDecodedFrame_ != UINT32_MAX && lastDecodedFrame_ < frameIndex)
    {
        startFrame = lastDecodedFrame_ + 1;
    }
    else
    {
        // Need to decode from a keyframe
        for (uint32_t i = frameIndex; i > 0; i--)
        {
            if (frameKeyFlags_[i])
            {
                startFrame = i;
                break;
            }
        }

        // Reset planes if starting from keyframe
        std::fill(planeY_.begin(), planeY_.end(), static_cast<uint8_t>(128));
        std::fill(planeU_.begin(), planeU_.end(), static_cast<uint8_t>(128));
        std::fill(planeV_.begin(), planeV_.end(), static_cast<uint8_t>(128));
    }

    // Decode frames up to target
    for (uint32_t i = startFrame; i <= frameIndex; i++)
    {
        if (!decodeFrameInternal(i))
        {
            return false;
        }
    }

    lastDecodedFrame_ = frameIndex;

    // Convert YUV to RGBA
    outFrame.pixels.resize(header_.width * header_.height * 4);
    outFrame.width = header_.width;
    outFrame.height = header_.height;
    outFrame.isKeyframe = frameKeyFlags_[frameIndex];

    convertYUVToRGBA(outFrame.pixels);

    return true;
}

bool BinkDecoder::decodeFrameInternal(uint32_t frameIndex)
{
    if (frameIndex >= header_.frameCount)
    {
        return false;
    }

    // Save current frame as previous
    std::copy(planeY_.begin(), planeY_.end(), prevY_.begin());
    std::copy(planeU_.begin(), planeU_.end(), prevU_.begin());
    std::copy(planeV_.begin(), planeV_.end(), prevV_.begin());

    // Get frame data
    uint32_t frameStart = frameOffsets_[frameIndex];
    uint32_t frameEnd = frameOffsets_[frameIndex + 1] & ~1u;

    if (frameStart >= data_.size() || frameEnd > data_.size() || frameEnd <= frameStart)
    {
        return false;
    }

    const uint8_t* frameData = data_.data() + frameStart;
    size_t frameSize = frameEnd - frameStart;

    BinkBitReader bits(frameData, frameSize);

    // Skip audio data for each track
    for (uint32_t track = 0; track < header_.audioTrackCount; track++)
    {
        uint32_t audioLen = bits.readBits(32);
        if (audioLen > 0)
        {
            bits.skipBits((audioLen - 4) * 8);
        }
    }

    // For versions >= 'i', each plane has a size prefix
    bool hasPlaneSize = (header_.magic[3] >= 'i');

    // Decode Y plane
    if (hasPlaneSize)
    {
        bits.readBits(32); // Plane size
    }
    if (!decodePlane(bits, planeY_.data(), prevY_.data(), planeWidthY_, planeHeightY_, false))
    {
        return false;
    }

    // Decode U plane
    if (hasPlaneSize)
    {
        bits.readBits(32);
    }
    if (!decodePlane(bits, planeU_.data(), prevU_.data(), planeWidthC_, planeHeightC_, true))
    {
        return false;
    }

    // Decode V plane
    if (hasPlaneSize)
    {
        bits.readBits(32);
    }
    if (!decodePlane(bits, planeV_.data(), prevV_.data(), planeWidthC_, planeHeightC_, true))
    {
        return false;
    }

    return true;
}

bool BinkDecoder::decodePlane(BinkBitReader& bits, uint8_t* plane, uint8_t* prev, uint32_t width,
                              uint32_t height, bool /*isChroma*/)
{
    // Reset bundles
    for (int i = 0; i < static_cast<int>(BinkBundleType::Count); i++)
    {
        bundles_[i].reset();
    }

    // Read bundles
    readBundle(bits, BinkBundleType::BlockTypes);
    readBundle(bits, BinkBundleType::SubBlockTypes);
    readBundle(bits, BinkBundleType::Colors);
    readBundle(bits, BinkBundleType::Pattern);
    readBundle(bits, BinkBundleType::MotionX);
    readBundle(bits, BinkBundleType::MotionY);
    readBundle(bits, BinkBundleType::IntraDC);
    readBundle(bits, BinkBundleType::InterDC);
    readBundle(bits, BinkBundleType::Run);

    int stride = static_cast<int>(width);

    // Process 8x8 blocks
    for (uint32_t by = 0; by < height; by += 8)
    {
        for (uint32_t bx = 0; bx < width; bx += 8)
        {
            uint8_t* dst = plane + by * width + bx;
            const uint8_t* prevPtr = prev + by * width + bx;

            int blockType = bundles_[static_cast<int>(BinkBundleType::BlockTypes)].getValue();

            switch (blockType)
            {
            case BINK_BLOCK_SKIP:
                decodeBlockSkip(dst, prevPtr, stride);
                break;

            case BINK_BLOCK_FILL:
            {
                int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                decodeBlockFill(dst, stride, static_cast<uint8_t>(color));
                break;
            }

            case BINK_BLOCK_MOTION:
            {
                int mvX = bundles_[static_cast<int>(BinkBundleType::MotionX)].getValue();
                int mvY = bundles_[static_cast<int>(BinkBundleType::MotionY)].getValue();
                // Sign extend
                if (mvX >= 8)
                    mvX -= 16;
                if (mvY >= 8)
                    mvY -= 16;
                decodeBlockMotion(dst, prevPtr, stride, mvX, mvY, width, height);
                break;
            }

            case BINK_BLOCK_RUN:
                decodeBlockRun(dst, stride, bits);
                break;

            case BINK_BLOCK_PATTERN:
            {
                int c0 = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                int c1 = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                int pattern = bundles_[static_cast<int>(BinkBundleType::Pattern)].getValue();
                decodeBlockPattern(dst, stride, static_cast<uint8_t>(c0), static_cast<uint8_t>(c1),
                                   static_cast<uint8_t>(pattern));
                break;
            }

            case BINK_BLOCK_RAW:
                decodeBlockRaw(dst, stride, bits);
                break;

            case BINK_BLOCK_INTRA:
            {
                int dc = bundles_[static_cast<int>(BinkBundleType::IntraDC)].getValue();
                decodeBlockIntraDCT(dst, stride, bits, dc);
                break;
            }

            case BINK_BLOCK_INTER:
            {
                int mvX = bundles_[static_cast<int>(BinkBundleType::MotionX)].getValue();
                int mvY = bundles_[static_cast<int>(BinkBundleType::MotionY)].getValue();
                if (mvX >= 8)
                    mvX -= 16;
                if (mvY >= 8)
                    mvY -= 16;
                int dc = bundles_[static_cast<int>(BinkBundleType::InterDC)].getValue();
                decodeBlockInterDCT(dst, prevPtr, stride, bits, mvX, mvY, dc, width, height);
                break;
            }

            case BINK_BLOCK_SCALED:
                decodeBlockScaled(dst, prevPtr, stride, bits, width, height);
                break;

            case BINK_BLOCK_RESIDUE:
            {
                int mvX = bundles_[static_cast<int>(BinkBundleType::MotionX)].getValue();
                int mvY = bundles_[static_cast<int>(BinkBundleType::MotionY)].getValue();
                if (mvX >= 8)
                    mvX -= 16;
                if (mvY >= 8)
                    mvY -= 16;
                decodeBlockResidue(dst, prevPtr, stride, bits, mvX, mvY, width, height);
                break;
            }

            default:
                // Unknown block type - skip
                decodeBlockSkip(dst, prevPtr, stride);
                break;
            }
        }
    }

    return true;
}

bool BinkDecoder::readBundle(BinkBitReader& bits, BinkBundleType type)
{
    return bundles_[static_cast<int>(type)].decode(bits, type);
}

// ============================================================================
// Block Decoders
// ============================================================================

void BinkDecoder::decodeBlockSkip(uint8_t* dst, const uint8_t* prev, int stride)
{
    for (int y = 0; y < 8; y++)
    {
        std::memcpy(dst + y * stride, prev + y * stride, 8);
    }
}

void BinkDecoder::decodeBlockFill(uint8_t* dst, int stride, uint8_t color)
{
    for (int y = 0; y < 8; y++)
    {
        std::memset(dst + y * stride, color, 8);
    }
}

void BinkDecoder::decodeBlockRun(uint8_t* dst, int stride, BinkBitReader& /*bits*/)
{
    int pos = 0;

    while (pos < 64)
    {
        int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
        int run = bundles_[static_cast<int>(BinkBundleType::Run)].getValue();

        // Apply run
        for (int i = 0; i < run && pos < 64; i++, pos++)
        {
            int x = binkPatternScan[pos] % 8;
            int y = binkPatternScan[pos] / 8;
            dst[y * stride + x] = static_cast<uint8_t>(color);
        }
    }

    // Fill remaining with last color
    int lastColor = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
    while (pos < 64)
    {
        int x = binkPatternScan[pos] % 8;
        int y = binkPatternScan[pos] / 8;
        dst[y * stride + x] = static_cast<uint8_t>(lastColor);
        pos++;
    }
}

void BinkDecoder::decodeBlockPattern(uint8_t* dst, int stride, uint8_t c0, uint8_t c1,
                                     uint8_t pattern)
{
    // 8-bit pattern defines 8 rows, alternating c0/c1
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            // Use simple pattern: odd/even based on pattern byte
            dst[y * stride + x] = ((pattern >> (y % 8)) & 1) ? c1 : c0;
        }
    }
}

void BinkDecoder::decodeBlockRaw(uint8_t* dst, int stride, BinkBitReader& /*bits*/)
{
    // Read 64 raw color values
    for (int i = 0; i < 64; i++)
    {
        int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
        int x = binkPatternScan[i] % 8;
        int y = binkPatternScan[i] / 8;
        dst[y * stride + x] = static_cast<uint8_t>(color);
    }
}

void BinkDecoder::decodeBlockMotion(uint8_t* dst, const uint8_t* prev, int stride, int mvX, int mvY,
                                    uint32_t planeWidth, uint32_t planeHeight)
{
    // Calculate source position from pointer offset
    ptrdiff_t offset = dst - prev;
    int srcY = static_cast<int>(offset / stride);
    int srcX = static_cast<int>(offset % stride);

    int refX = srcX + mvX;
    int refY = srcY + mvY;

    // Clamp to valid range
    refX = std::max(0, std::min(refX, static_cast<int>(planeWidth) - 8));
    refY = std::max(0, std::min(refY, static_cast<int>(planeHeight) - 8));

    const uint8_t* src = prev + refY * stride + refX;

    for (int y = 0; y < 8; y++)
    {
        std::memcpy(dst + y * stride, src + y * stride, 8);
    }
}

void BinkDecoder::decodeBlockIntraDCT(uint8_t* dst, int stride, BinkBitReader& bits, int dc)
{
    int block[64] = {0};

    // DC coefficient
    block[0] = dc * 8;

    // Read AC coefficients
    readDCTCoeffs(bits, block);

    // Inverse DCT
    idct8x8(block);

    // Store result
    addBlock(dst, stride, block);
}

void BinkDecoder::decodeBlockInterDCT(uint8_t* dst, const uint8_t* prev, int stride,
                                      BinkBitReader& bits, int mvX, int mvY, int dc,
                                      uint32_t planeWidth, uint32_t planeHeight)
{
    // First, copy motion-compensated block
    decodeBlockMotion(dst, prev, stride, mvX, mvY, planeWidth, planeHeight);

    // Then apply residue via DCT
    int block[64] = {0};
    block[0] = dc * 8;
    readDCTCoeffs(bits, block);
    idct8x8(block);

    // Add residue to motion-compensated block
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            int val = dst[y * stride + x] + block[y * 8 + x];
            dst[y * stride + x] = static_cast<uint8_t>(std::clamp(val, 0, 255));
        }
    }
}

void BinkDecoder::decodeBlockScaled(uint8_t* dst, const uint8_t* /*prev*/, int stride,
                                    BinkBitReader& /*bits*/, uint32_t /*planeWidth*/,
                                    uint32_t /*planeHeight*/)
{
    // Scaled block: decode 4x4 sub-block and scale up to 8x8
    uint8_t temp[16];
    constexpr int tempStride = 4;

    // Decode as 4x4
    for (int i = 0; i < 16; i++)
    {
        int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
        temp[i] = static_cast<uint8_t>(color);
    }

    // Scale 4x4 to 8x8
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            dst[y * stride + x] = temp[(y / 2) * tempStride + (x / 2)];
        }
    }
}

void BinkDecoder::decodeBlockResidue(uint8_t* dst, const uint8_t* prev, int stride,
                                     BinkBitReader& /*bits*/, int mvX, int mvY,
                                     uint32_t planeWidth, uint32_t planeHeight)
{
    // Copy motion block first
    decodeBlockMotion(dst, prev, stride, mvX, mvY, planeWidth, planeHeight);

    // Read residue values (lossless mode)
    for (int i = 0; i < 64; i++)
    {
        int residue = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
        // Sign extend if needed
        if (residue >= 128)
            residue -= 256;

        int x = binkPatternScan[i] % 8;
        int y = binkPatternScan[i] / 8;
        int val = dst[y * stride + x] + residue;
        dst[y * stride + x] = static_cast<uint8_t>(std::clamp(val, 0, 255));
    }
}

void BinkDecoder::readDCTCoeffs(BinkBitReader& bits, int* block)
{
    // Simple coefficient reading - just get from bundles
    // Full implementation would use proper entropy decoding
    for (int i = 1; i < 64 && !bits.atEnd(); i++)
    {
        int coeff = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
        if (coeff >= 128)
            coeff -= 256;
        block[binkScan[i]] = coeff;
    }
}

void BinkDecoder::idct8x8(int* block)
{
    // Simple IDCT implementation
    // Constants
    const float c1 = 0.9807853f;
    const float c2 = 0.9238795f;
    const float c3 = 0.8314696f;
    const float c4 = 0.7071068f;
    const float c5 = 0.5555702f;
    const float c6 = 0.3826834f;
    const float c7 = 0.1950903f;

    float temp[64];

    // Rows
    for (int y = 0; y < 8; y++)
    {
        int* row = block + y * 8;
        float* out = temp + y * 8;

        float s0 = static_cast<float>(row[0]);
        float s1 = static_cast<float>(row[1]);
        float s2 = static_cast<float>(row[2]);
        float s3 = static_cast<float>(row[3]);
        float s4 = static_cast<float>(row[4]);
        float s5 = static_cast<float>(row[5]);
        float s6 = static_cast<float>(row[6]);
        float s7 = static_cast<float>(row[7]);

        // Simple 1D IDCT
        out[0] = c4 * (s0 + s4) + c2 * s2 + c6 * s6;
        out[1] = c1 * s1 + c3 * s3 + c5 * s5 + c7 * s7;
        out[2] = c4 * (s0 - s4) + c6 * s2 - c2 * s6;
        out[3] = c3 * s1 - c7 * s3 - c1 * s5 - c5 * s7;
        out[4] = c4 * (s0 + s4) - c2 * s2 - c6 * s6;
        out[5] = c5 * s1 - c1 * s3 + c7 * s5 + c3 * s7;
        out[6] = c4 * (s0 - s4) - c6 * s2 + c2 * s6;
        out[7] = c7 * s1 - c5 * s3 + c3 * s5 - c1 * s7;
    }

    // Columns
    for (int x = 0; x < 8; x++)
    {
        float s0 = temp[x + 0 * 8];
        float s1 = temp[x + 1 * 8];
        float s2 = temp[x + 2 * 8];
        float s3 = temp[x + 3 * 8];
        float s4 = temp[x + 4 * 8];
        float s5 = temp[x + 5 * 8];
        float s6 = temp[x + 6 * 8];
        float s7 = temp[x + 7 * 8];

        block[x + 0 * 8] = static_cast<int>(c4 * (s0 + s4) + c2 * s2 + c6 * s6);
        block[x + 1 * 8] = static_cast<int>(c1 * s1 + c3 * s3 + c5 * s5 + c7 * s7);
        block[x + 2 * 8] = static_cast<int>(c4 * (s0 - s4) + c6 * s2 - c2 * s6);
        block[x + 3 * 8] = static_cast<int>(c3 * s1 - c7 * s3 - c1 * s5 - c5 * s7);
        block[x + 4 * 8] = static_cast<int>(c4 * (s0 + s4) - c2 * s2 - c6 * s6);
        block[x + 5 * 8] = static_cast<int>(c5 * s1 - c1 * s3 + c7 * s5 + c3 * s7);
        block[x + 6 * 8] = static_cast<int>(c4 * (s0 - s4) - c6 * s2 + c2 * s6);
        block[x + 7 * 8] = static_cast<int>(c7 * s1 - c5 * s3 + c3 * s5 - c1 * s7);
    }
}

void BinkDecoder::addBlock(uint8_t* dst, int stride, const int* block)
{
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            int val = block[y * 8 + x];
            // Shift and clamp
            val = (val + 128) >> 4;  // Scale down and add DC offset
            dst[y * stride + x] = static_cast<uint8_t>(std::clamp(val, 0, 255));
        }
    }
}

void BinkDecoder::convertYUVToRGBA(std::vector<uint8_t>& rgba)
{
    for (uint32_t y = 0; y < header_.height; y++)
    {
        for (uint32_t x = 0; x < header_.width; x++)
        {
            // Get Y sample
            uint8_t yVal = planeY_[y * planeWidthY_ + x];

            // Get UV samples (chroma is half resolution)
            uint32_t cx = x / 2;
            uint32_t cy = y / 2;
            uint8_t uVal = planeU_[cy * planeWidthC_ + cx];
            uint8_t vVal = planeV_[cy * planeWidthC_ + cx];

            // YUV to RGB conversion (BT.601)
            int yy = static_cast<int>(yVal) - 16;
            int uu = static_cast<int>(uVal) - 128;
            int vv = static_cast<int>(vVal) - 128;

            int r = (298 * yy + 409 * vv + 128) >> 8;
            int g = (298 * yy - 100 * uu - 208 * vv + 128) >> 8;
            int b = (298 * yy + 516 * uu + 128) >> 8;

            size_t idx = (y * header_.width + x) * 4;
            rgba[idx + 0] = static_cast<uint8_t>(std::clamp(r, 0, 255));
            rgba[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
            rgba[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
            rgba[idx + 3] = 255;
        }
    }
}

std::vector<uint8_t> BinkDecoder::getFrameRGBA(uint32_t frameIndex)
{
    BinkFrame frame;
    if (!decodeFrame(frameIndex, frame))
    {
        return {};
    }
    return frame.pixels;
}

void BinkDecoder::reset()
{
    std::fill(planeY_.begin(), planeY_.end(), static_cast<uint8_t>(128));
    std::fill(planeU_.begin(), planeU_.end(), static_cast<uint8_t>(128));
    std::fill(planeV_.begin(), planeV_.end(), static_cast<uint8_t>(128));
    std::fill(prevY_.begin(), prevY_.end(), static_cast<uint8_t>(128));
    std::fill(prevU_.begin(), prevU_.end(), static_cast<uint8_t>(128));
    std::fill(prevV_.begin(), prevV_.end(), static_cast<uint8_t>(128));
    lastDecodedFrame_ = UINT32_MAX;
}

// ============================================================================
// Audio Decoding
// ============================================================================

BinkAudioInfo BinkDecoder::getAudioInfo(uint32_t track) const
{
    BinkAudioInfo info;
    if (track >= audioTracks_.size())
    {
        return info;
    }

    const auto& at = audioTracks_[track];
    info.sampleRate = at.sampleRate;
    info.channels = at.channels;
    info.hasAudio = at.sampleRate > 0 && at.channels > 0;
    info.useDCT = at.isDCT;

    return info;
}

bool BinkDecoder::hasAudio(uint32_t track) const
{
    if (track >= audioTracks_.size())
    {
        return false;
    }
    const auto& at = audioTracks_[track];
    return at.sampleRate > 0 && at.channels > 0;
}

bool BinkDecoder::decodeAudio(uint32_t frameIndex, uint32_t track, BinkAudioFrame& outAudio)
{
    if (frameIndex >= header_.frameCount || track >= audioTracks_.size())
    {
        return false;
    }

    if (!hasAudio(track))
    {
        return false;
    }

    // Get frame data
    uint32_t frameStart = frameOffsets_[frameIndex];
    uint32_t frameEnd = frameOffsets_[frameIndex + 1] & ~1u;

    if (frameStart >= data_.size() || frameEnd > data_.size() || frameEnd <= frameStart)
    {
        return false;
    }

    const uint8_t* frameData = data_.data() + frameStart;
    size_t frameSize = frameEnd - frameStart;

    BinkBitReader bits(frameData, frameSize);

    // Find and decode the requested audio track
    for (uint32_t t = 0; t <= track; t++)
    {
        uint32_t audioLen = bits.readBits(32);
        if (audioLen == 0)
        {
            if (t == track)
            {
                return false;  // No audio data for this track
            }
            continue;
        }

        if (t == track)
        {
            // Decode this track
            return decodeAudioTrack(bits, track, outAudio);
        }
        else
        {
            // Skip this track
            bits.skipBits((audioLen - 4) * 8);
        }
    }

    return false;
}

bool BinkDecoder::decodeAudioTrack(BinkBitReader& bits, uint32_t track, BinkAudioFrame& outAudio)
{
    if (track >= audioTracks_.size())
    {
        return false;
    }

    const auto& trackInfo = audioTracks_[track];
    outAudio.sampleRate = trackInfo.sampleRate;
    outAudio.channels = static_cast<uint8_t>(trackInfo.channels);

    // Read sample count
    uint32_t sampleCount = bits.readBits(32);
    if (sampleCount == 0 || sampleCount > 10 * 1024 * 1024)
    {
        return false;
    }

    // Calculate frame size based on sample rate
    size_t frameSize = audioFrameSize_;
    if (frameSize == 0)
    {
        frameSize = 4096;  // Default
    }

    // Allocate output
    outAudio.samples.resize(sampleCount * trackInfo.channels);

    // For now, output silence as a placeholder
    // Full Bink audio decoding requires DCT/RDFT implementation
    // which is complex and requires floating-point transforms
    std::fill(outAudio.samples.begin(), outAudio.samples.end(), static_cast<int16_t>(0));

    // Skip the compressed audio data
    // A full implementation would:
    // 1. Read floating-point coefficients (5-bit exp + 23-bit mantissa + 1-bit sign)
    // 2. Unpack quantizers across 25 frequency bands
    // 3. Apply inverse DCT or RDFT
    // 4. Window/overlap with previous frame

    return true;
}

void BinkDecoder::rdft(float* /*data*/, size_t /*n*/, bool /*inverse*/)
{
    // Real Discrete Fourier Transform
    // Placeholder for future implementation
}

void BinkDecoder::dct(float* /*data*/, size_t /*n*/, bool /*inverse*/)
{
    // Discrete Cosine Transform
    // Placeholder for future implementation
}

} // namespace runeharbor::media
