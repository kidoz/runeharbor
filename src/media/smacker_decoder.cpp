// SPDX-License-Identifier: MIT
// Based on publicly available Smacker format documentation and libsmacker algorithms
#include "smacker_decoder.hpp"

#include <algorithm>

#include <cmath>
#include <cstring>

#include "../util/fft.hpp"

namespace runeharbor::media
{

// ============================================================================
// BitReader Implementation
// ============================================================================

BitReader::BitReader(const uint8_t* data, size_t sizeBytes)
    : data_(data), bitPos_(0), maxBits_(sizeBytes * 8)
{
}

bool BitReader::readBit()
{
    if (bitPos_ >= maxBits_)
    {
        return false;
    }

    size_t byteIdx = bitPos_ / 8;
    size_t bitIdx = bitPos_ % 8;
    bitPos_++;

    return (data_[byteIdx] >> bitIdx) & 1;
}

uint32_t BitReader::readBits(int count)
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

void BitReader::skipBits(int count)
{
    bitPos_ += count;
    if (bitPos_ > maxBits_)
    {
        bitPos_ = maxBits_;
    }
}

bool BitReader::atEnd() const
{
    return bitPos_ >= maxBits_;
}

size_t BitReader::bitsRemaining() const
{
    return bitPos_ < maxBits_ ? maxBits_ - bitPos_ : 0;
}

// ============================================================================
// Huffman Tree Constants
// ============================================================================

static constexpr uint16_t HUFF8_BRANCH = 0x8000;
static constexpr uint16_t HUFF8_LEAF_MASK = 0x7FFF;

static constexpr uint32_t HUFF16_BRANCH = 0x80000000;
static constexpr uint32_t HUFF16_CACHE = 0x40000000;
static constexpr uint32_t HUFF16_LEAF_MASK = 0x3FFFFFFF;

// ============================================================================
// 8-bit Huffman Tree (for building 16-bit trees)
// ============================================================================

struct Huff8Tree
{
    std::vector<uint16_t> tree;
    std::vector<uint8_t> leafValues; // Leaf values in encounter order (for big tree building)
    size_t size = 0;
    size_t leafCursor = 0; // Current position for sequential leaf access

    bool buildRecursive(BitReader& bits)
    {
        if (size >= 511)
        {
            return false; // Max size exceeded
        }

        if (bits.atEnd())
        {
            return false;
        }

        if (bits.readBit())
        {
            // Branch node
            size_t currentIdx = size++;
            tree.resize(size);

            // Build left subtree
            if (!buildRecursive(bits))
            {
                return false;
            }

            // Mark as branch with jump to right subtree
            tree[currentIdx] = HUFF8_BRANCH | static_cast<uint16_t>(size);

            // Build right subtree
            if (!buildRecursive(bits))
            {
                return false;
            }
        }
        else
        {
            // Leaf node - read 8-bit value
            uint8_t value = static_cast<uint8_t>(bits.readBits(8));
            tree.push_back(static_cast<uint16_t>(value));
            leafValues.push_back(value); // Store in encounter order
            size++;
        }

        return true;
    }

    bool build(BitReader& bits)
    {
        tree.clear();
        leafValues.clear();
        size = 0;
        leafCursor = 0;

        // Check presence bit
        if (!bits.readBit())
        {
            // Empty tree - single leaf with value 0
            tree.push_back(0);
            leafValues.push_back(0);
            size = 1;
            return true;
        }

        // Build tree recursively
        if (!buildRecursive(bits))
        {
            return false;
        }

        // Consume terminator bit
        bits.readBit();

        return true;
    }

    // Get next leaf value in encounter order (for big tree building)
    uint8_t nextLeaf()
    {
        if (leafCursor >= leafValues.size())
        {
            return 0;
        }
        return leafValues[leafCursor++];
    }

    void resetLeafCursor() { leafCursor = 0; }

    int lookup(BitReader& bits) const
    {
        if (tree.empty())
        {
            return 0;
        }

        size_t index = 0;
        while (index < tree.size() && (tree[index] & HUFF8_BRANCH))
        {
            if (bits.atEnd())
            {
                return 0;
            }

            if (bits.readBit())
            {
                // Right branch
                index = tree[index] & HUFF8_LEAF_MASK;
            }
            else
            {
                // Left branch (next entry)
                index++;
            }
        }

        return (index < tree.size()) ? tree[index] : 0;
    }
};

// ============================================================================
// 16-bit Huffman Tree (BigHuffmanTree)
// ============================================================================

struct SmackerDecoder::BigHuffmanTree
{
    std::vector<uint32_t> tree;
    size_t size = 0;
    uint16_t cache[3] = {0, 0, 0};
    uint16_t originalCache[3] = {0, 0, 0}; // Store original values for reset
    bool isEmpty = true;

    bool buildRecursive(BitReader& bits, const Huff8Tree& low8, const Huff8Tree& hi8, size_t limit)
    {
        // Allow building past limit - we'll handle this later
        // Some files have tree structures larger than allocated space
        if (bits.atEnd())
        {
            return true; // Ran out of bits - tree is complete
        }

        if (bits.readBit())
        {
            // Branch node
            if (size >= tree.size())
            {
                tree.resize(size + 1);
            }
            size_t currentIdx = size++;

            // Build left subtree
            if (!buildRecursive(bits, low8, hi8, limit))
            {
                return false;
            }

            // Mark as branch with jump to right subtree
            tree[currentIdx] = HUFF16_BRANCH | static_cast<uint32_t>(size);

            // Build right subtree
            if (!buildRecursive(bits, low8, hi8, limit))
            {
                return false;
            }
        }
        else
        {
            // Leaf node - decode using low and high 8-bit trees
            // Per FFmpeg: use VLC (tree traversal) to decode values from bitstream
            int lowVal = low8.lookup(bits);
            int hiVal = hi8.lookup(bits);

            uint32_t value = static_cast<uint32_t>((hiVal << 8) | (lowVal & 0xFF));

            // Check if value matches any cache entry
            if (value == cache[0])
            {
                value = HUFF16_CACHE | 0;
            }
            else if (value == cache[1])
            {
                value = HUFF16_CACHE | 1;
            }
            else if (value == cache[2])
            {
                value = HUFF16_CACHE | 2;
            }

            if (size >= tree.size())
            {
                tree.resize(size + 1);
            }
            tree[size++] = value;
        }

        return true;
    }

    bool build(BitReader& bits, uint32_t allocSize)
    {
        tree.clear();
        size = 0;
        isEmpty = true;

        // Check presence bit
        if (!bits.readBit())
        {
            // Empty tree - single element
            tree.push_back(0);
            size = 1;
            isEmpty = false;
            return true;
        }

        isEmpty = false;

        // Build low 8-bit tree
        Huff8Tree low8;
        if (!low8.build(bits))
        {
            return false;
        }

        // Build high 8-bit tree
        Huff8Tree hi8;
        if (!hi8.build(bits))
        {
            return false;
        }

        // Read 3 cache values (each is 16-bit: low byte then high byte)
        for (int i = 0; i < 3; i++)
        {
            uint8_t lo = static_cast<uint8_t>(bits.readBits(8));
            uint8_t hi = static_cast<uint8_t>(bits.readBits(8));
            cache[i] = static_cast<uint16_t>((hi << 8) | lo);
            originalCache[i] = cache[i]; // Store for reset
        }

        // Calculate limit from allocation size
        size_t limit = 0;
        if (allocSize >= 12 && (allocSize % 4) == 0)
        {
            limit = (allocSize - 12) / 4;
        }
        else
        {
            limit = 65536; // Large default if allocSize is unusual
        }

        tree.clear();
        tree.reserve(std::min(limit, size_t(65536)));

        // Build the big tree
        if (!buildRecursive(bits, low8, hi8, limit))
        {
            return false;
        }

        // Shrink to actual size
        tree.resize(size);
        tree.shrink_to_fit();

        // Check final terminator bit
        bits.readBit();

        return true;
    }

    uint16_t decode(BitReader& bits)
    {
        if (tree.empty())
        {
            return 0;
        }

        size_t index = 0;
        while (index < tree.size() && (tree[index] & HUFF16_BRANCH))
        {
            if (bits.atEnd())
            {
                return 0;
            }

            if (bits.readBit())
            {
                // Right branch
                index = tree[index] & HUFF16_LEAF_MASK;
            }
            else
            {
                // Left branch (next entry)
                index++;
            }
        }

        if (index >= tree.size())
        {
            return 0;
        }

        uint32_t value = tree[index];

        // Check for cache escape code
        if (value & HUFF16_CACHE)
        {
            value = cache[value & HUFF16_LEAF_MASK];
        }

        // Update cache (move to front)
        uint16_t result = static_cast<uint16_t>(value);
        if (cache[0] != result)
        {
            cache[2] = cache[1];
            cache[1] = cache[0];
            cache[0] = result;
        }

        return result;
    }

    void resetCache()
    {
        // Reset cache to original values read during tree building
        // This is required at the start of each frame
        cache[0] = originalCache[0];
        cache[1] = originalCache[1];
        cache[2] = originalCache[2];
    }
};

// ============================================================================
// SmackerDecoder Implementation
// ============================================================================

SmackerDecoder::SmackerDecoder()
{
    std::memset(&header_, 0, sizeof(header_));
    palette_.resize(768, 0);
}

SmackerDecoder::~SmackerDecoder() = default;

bool SmackerDecoder::load(const uint8_t* data, size_t size)
{
    reset();
    if (!data || size < sizeof(SmackerHeader))
    {
        return false;
    }

    data_.assign(data, data + size);
    return parseHeader() && buildTrees();
}

bool SmackerDecoder::load(const std::vector<uint8_t>& data)
{
    return load(data.data(), data.size());
}

bool SmackerDecoder::parseHeader()
{
    if (data_.size() < sizeof(SmackerHeader))
    {
        return false;
    }

    // Copy header
    std::memcpy(&header_, data_.data(), sizeof(SmackerHeader));

    // Validate magic
    if (std::memcmp(header_.magic, "SMK2", 4) == 0)
    {
        isV4_ = false;
    }
    else if (std::memcmp(header_.magic, "SMK4", 4) == 0)
    {
        isV4_ = true;
    }
    else
    {
        return false;
    }

    // Validate dimensions
    if (header_.width == 0 || header_.height == 0 || header_.width > 4096 || header_.height > 4096)
    {
        return false;
    }

    doubleHigh_ = (header_.flags & 1) != 0;
    if (doubleHigh_)
    {
        header_.height *= 2;
    }

    if (header_.frameCount == 0 || header_.frameCount > 100000)
    {
        return false;
    }

    // Read frame sizes (after header)
    size_t offset = sizeof(SmackerHeader);

    frameSizes_.resize(header_.frameCount);
    size_t frameSizesBytes = header_.frameCount * sizeof(uint32_t);
    if (offset + frameSizesBytes > data_.size())
    {
        return false;
    }

    std::memcpy(frameSizes_.data(), data_.data() + offset, frameSizesBytes);
    offset += frameSizesBytes;

    // Read frame types
    frameTypes_.resize(header_.frameCount);
    if (offset + header_.frameCount > data_.size())
    {
        return false;
    }

    std::memcpy(frameTypes_.data(), data_.data() + offset, header_.frameCount);
    offset += header_.frameCount;

    // Precompute frame offsets and keyframe flags
    frameOffsets_.clear();
    frameOffsets_.resize(header_.frameCount + 1, 0);
    frameKeyFlags_.clear();
    frameKeyFlags_.resize(header_.frameCount, false);
    keyframeIndices_.clear();

    frameOffsets_[0] = static_cast<uint32_t>(offset + header_.treesSize);
    for (uint32_t i = 0; i < header_.frameCount; i++)
    {
        const uint32_t sizeFlagged = frameSizes_[i];
        // Smacker stores keyframe flag in bit 0; bit 1 is reserved.
        frameKeyFlags_[i] = (sizeFlagged & 0x1u) != 0;
        const uint32_t size = sizeFlagged & 0xFFFFFFFCu;
        frameOffsets_[i + 1] = frameOffsets_[i] + size;
        if (frameKeyFlags_[i])
        {
            keyframeIndices_.push_back(i);
        }
    }
    if (frameOffsets_.back() > data_.size())
    {
        return false;
    }

    treesOffset_ = offset;

    // Skip trees for now (will be parsed in buildTrees)
    offset += header_.treesSize;

    dataOffset_ = offset;

    // Initialize frame buffer
    frameBuffer_.resize(header_.width * header_.height, 0);

    // Initialize with default grayscale palette
    for (int i = 0; i < 256; i++)
    {
        palette_[i * 3 + 0] = static_cast<uint8_t>(i);
        palette_[i * 3 + 1] = static_cast<uint8_t>(i);
        palette_[i * 3 + 2] = static_cast<uint8_t>(i);
    }

    lastDecodedFrame_ = UINT32_MAX;

    return true;
}

bool SmackerDecoder::buildTrees()
{
    if (header_.treesSize == 0)
    {
        return true;
    }

    if (treesOffset_ + header_.treesSize > data_.size())
    {
        return false;
    }

    BitReader bits(data_.data() + treesOffset_, header_.treesSize);

    // Build the four Huffman trees with their allocation sizes from header
    mmapTree_ = std::make_unique<BigHuffmanTree>();
    if (!mmapTree_->build(bits, header_.mmapSize))
    {
        return false;
    }

    mclrTree_ = std::make_unique<BigHuffmanTree>();
    if (!mclrTree_->build(bits, header_.mclrSize))
    {
        return false;
    }

    fullTree_ = std::make_unique<BigHuffmanTree>();
    if (!fullTree_->build(bits, header_.fullSize))
    {
        return false;
    }

    typeTree_ = std::make_unique<BigHuffmanTree>();
    if (!typeTree_->build(bits, header_.typeSize))
    {
        return false;
    }

    return true;
}

double SmackerDecoder::frameRate() const
{
    if (header_.frameRate == 0)
    {
        return 15.0;
    }

    // Smacker frame rate encoding:
    // Positive: microseconds per frame
    // Negative: fps = 100000 / abs(rate)

    if (header_.frameRate > 0)
    {
        return 1000000.0 / header_.frameRate;
    }
    return 100000.0 / (-header_.frameRate);
}

double SmackerDecoder::durationMs() const
{
    double fps = frameRate();
    if (fps <= 0)
    {
        fps = 15.0;
    }
    return (header_.frameCount * 1000.0) / fps;
}

bool SmackerDecoder::decodeFrame(uint32_t frameIndex, SmackerFrame& outFrame)
{
    if (frameIndex >= header_.frameCount)
    {
        return false;
    }

    uint32_t startFrame = 0;
    bool resetBuffer = false;

    if (lastDecodedFrame_ != UINT32_MAX && lastDecodedFrame_ < frameIndex)
    {
        startFrame = lastDecodedFrame_ + 1;
    }
    else
    {
        // Seek to nearest keyframe if available.
        if (!keyframeIndices_.empty())
        {
            auto it =
                std::upper_bound(keyframeIndices_.begin(), keyframeIndices_.end(), frameIndex);
            if (it != keyframeIndices_.begin())
            {
                startFrame = *(it - 1);
            }
        }
        resetBuffer = true;
    }

    if (resetBuffer)
    {
        std::fill(frameBuffer_.begin(), frameBuffer_.end(), 0);
    }

    for (uint32_t i = startFrame; i <= frameIndex; i++)
    {
        if (!decodeFrameInternal(i))
        {
            return false;
        }
    }

    lastDecodedFrame_ = frameIndex;

    outFrame.pixels = frameBuffer_;
    outFrame.width = header_.width;
    outFrame.height = header_.height;
    outFrame.isKeyframe = (!frameKeyFlags_.empty()) ? frameKeyFlags_[frameIndex]
                                                    : ((frameTypes_[frameIndex] & 1) != 0);

    return true;
}

bool SmackerDecoder::decodeFrameInternal(uint32_t frameIndex)
{
    if (frameIndex >= frameOffsets_.size() - 1)
    {
        return false;
    }

    size_t offset = frameOffsets_[frameIndex];
    uint32_t frameSize = frameSizes_[frameIndex] & 0xFFFFFFFCu;
    if (offset + frameSize > data_.size())
    {
        return false;
    }

    const uint8_t* frameData = data_.data() + offset;
    size_t frameOffset = 0;

    uint8_t frameType = frameTypes_[frameIndex];

    // Check for palette update (frame type bit 0)
    if (frameType & 1)
    {
        if (frameOffset >= frameSize)
        {
            return false;
        }

        uint8_t palRecordSize = frameData[frameOffset++];
        // Palette size byte represents (actual_bytes + 1) / 4
        size_t palBytes = static_cast<size_t>(palRecordSize) * 4 - 1;

        if (frameOffset + palBytes > frameSize)
        {
            palBytes = frameSize - frameOffset;
        }

        if (palBytes > 0)
        {
            decodePalette(frameData + frameOffset, palBytes);
            frameOffset += palBytes;
        }
    }

    // Skip audio data (bits 1-7 indicate audio tracks 0-6)
    for (int track = 0; track < 7; track++)
    {
        if (frameType & (2 << track))
        {
            if (frameOffset + 4 > frameSize)
            {
                break;
            }

            uint32_t audioSize = 0;
            std::memcpy(&audioSize, frameData + frameOffset, 4);
            frameOffset += 4;

            audioSize &= 0x00FFFFFFu;
            if (audioSize > 4)
            {
                frameOffset += audioSize - 4;
            }
        }
    }

    // Remaining data is video
    if (frameOffset >= frameSize)
    {
        return true;
    }

    size_t videoSize = frameSize - frameOffset;
    BitReader bits(frameData + frameOffset, videoSize);

    // Reset tree caches for new frame
    if (mmapTree_)
        mmapTree_->resetCache();
    if (mclrTree_)
        mclrTree_->resetCache();
    if (fullTree_)
        fullTree_->resetCache();
    if (typeTree_)
        typeTree_->resetCache();

    return decodeVideoData(bits, (frameType & 1) != 0);
}

bool SmackerDecoder::decodeVideoData(BitReader& bits, bool /*hasKeyframe*/)
{
    if (!typeTree_)
    {
        return true;
    }

    uint32_t blocksWide = (header_.width + 3) / 4;
    uint32_t originalHeight = doubleHigh_ ? header_.height / 2 : header_.height;
    uint32_t blocksHigh = (originalHeight + 3) / 4;
    uint32_t totalBlocks = blocksWide * blocksHigh;
    uint32_t blockIdx = 0;

    while (blockIdx < totalBlocks)
    {
        uint16_t typeDesc = typeTree_->decode(bits);
        // Type descriptor format per libsmacker:
        // - bits 0-1: block type (0-3)
        // - bits 2-7: run length - 1 (6 bits)
        // - bits 8-15: type data (8 bits, used as fill color for SOLID blocks)
        BlockType blockType = static_cast<BlockType>(typeDesc & 3);
        uint32_t runLength = ((typeDesc >> 2) & 0x3F) + 1;
        uint8_t typeData = static_cast<uint8_t>((typeDesc >> 8) & 0xFF);

        for (uint32_t r = 0; r < runLength && blockIdx < totalBlocks; r++, blockIdx++)
        {
            uint32_t bx = blockIdx % blocksWide;
            uint32_t by = blockIdx / blocksWide;
            uint32_t x = bx * 4;
            uint32_t y = by * (doubleHigh_ ? 8 : 4);

            switch (blockType)
            {
            case BLOCK_SKIP:
                decodeBlockSkip(x, y);
                break;

            case BLOCK_FILL:
            {
                // SOLID/FILL blocks
                if (isV4_)
                {
                    // In v4, fill color usually comes from MMAP tree
                    uint16_t val = mmapTree_ ? mmapTree_->decode(bits) : 0;
                    typeData = static_cast<uint8_t>(val & 0xFF);
                }
                decodeBlockFill(x, y, typeData);
                break;
            }

            case BLOCK_MONO:
            {
                // MONO blocks: colors from MCLR, bitmap from MMAP
                uint16_t colors = mclrTree_ ? mclrTree_->decode(bits) : 0;
                uint8_t c0 = static_cast<uint8_t>(colors & 0xFF);
                uint8_t c1 = static_cast<uint8_t>((colors >> 8) & 0xFF);
                uint16_t bitmap = mmapTree_ ? mmapTree_->decode(bits) : 0;
                decodeBlockMono(x, y, c0, c1, bitmap);
                break;
            }

            case BLOCK_FULL:
                decodeBlockFull(bits, x, y);
                break;
            }
        }
    }

    return true;
}

void SmackerDecoder::decodePalette(const uint8_t* data, size_t size)
{
    size_t srcPos = 0;
    size_t palIdx = 0;

    while (srcPos < size && palIdx < 256)
    {
        uint8_t code = data[srcPos++];

        if (code & 0x80)
        {
            // Skip (code & 0x7F) + 1 entries
            palIdx += (code & 0x7F) + 1;
        }
        else if (code & 0x40)
        {
            // Copy from earlier position
            if (srcPos >= size)
            {
                break;
            }
            uint8_t srcIdx = data[srcPos++];
            uint8_t count = (code & 0x3F) + 1;

            for (uint8_t i = 0; i < count && palIdx < 256; i++, palIdx++)
            {
                size_t src = (static_cast<size_t>(srcIdx) + i) * 3;
                size_t dst = palIdx * 3;
                if (src + 2 < palette_.size() && dst + 2 < palette_.size())
                {
                    palette_[dst + 0] = palette_[src + 0];
                    palette_[dst + 1] = palette_[src + 1];
                    palette_[dst + 2] = palette_[src + 2];
                }
            }
        }
        else
        {
            // New RGB entry (6-bit values)
            // The code byte itself is the R value (no control bits set)
            if (srcPos + 2 > size)
            {
                break;
            }

            uint8_t r = code & 0x3F;
            uint8_t g = data[srcPos++] & 0x3F;
            uint8_t b = data[srcPos++] & 0x3F;

            // Expand 6-bit to 8-bit
            palette_[palIdx * 3 + 0] = static_cast<uint8_t>((r << 2) | (r >> 4));
            palette_[palIdx * 3 + 1] = static_cast<uint8_t>((g << 2) | (g >> 4));
            palette_[palIdx * 3 + 2] = static_cast<uint8_t>((b << 2) | (b >> 4));
            palIdx++;
        }
    }
}

void SmackerDecoder::decodeBlockSkip(uint32_t /*x*/, uint32_t /*y*/)
{
    // Keep previous frame data
}

void SmackerDecoder::decodeBlockFill(uint32_t x, uint32_t y, uint8_t color)
{
    uint32_t blockHeight = doubleHigh_ ? 8 : 4;
    for (uint32_t py = 0; py < blockHeight && (y + py) < header_.height; py++)
    {
        for (uint32_t px = 0; px < 4 && (x + px) < header_.width; px++)
        {
            size_t idx = (y + py) * header_.width + (x + px);
            frameBuffer_[idx] = color;
        }
    }
}

void SmackerDecoder::decodeBlockMono(uint32_t x, uint32_t y, uint8_t c0, uint8_t c1,
                                     uint16_t bitmap)
{
    for (uint32_t py = 0; py < 4; py++)
    {
        for (uint32_t px = 0; px < 4 && (x + px) < header_.width; px++)
        {
            uint32_t bitIdx = py * 4 + px;
            uint8_t color = (bitmap & (1 << bitIdx)) ? c1 : c0;

            uint32_t targetY = y + py * (doubleHigh_ ? 2 : 1);
            if (targetY < header_.height)
            {
                frameBuffer_[targetY * header_.width + (x + px)] = color;
                if (doubleHigh_ && (targetY + 1) < header_.height)
                {
                    frameBuffer_[(targetY + 1) * header_.width + (x + px)] = color;
                }
            }
        }
    }
}

void SmackerDecoder::decodeBlockFull(BitReader& bits, uint32_t x, uint32_t y)
{
    if (!fullTree_)
    {
        return;
    }

    // Per libsmacker/FFmpeg: "The 4x4 block is divided into 4 2x2 blocks."
    for (int i = 0; i < 4; i++)
    {
        uint32_t subX = x + (i & 1) * 2;
        uint32_t subY = y + (i >> 1) * 2 * (doubleHigh_ ? 2 : 1);

        bool isSolid = false;
        if (isV4_)
        {
            isSolid = bits.readBit();
        }

        if (isSolid)
        {
            // v4 Solid sub-block
            uint16_t val = mclrTree_ ? mclrTree_->decode(bits) : 0;
            uint8_t color = static_cast<uint8_t>(val & 0xFF);
            // uint8_t colorHi = static_cast<uint8_t>(val >> 8);
            // Note: libsmacker uses low byte. Some sources say high byte is another color?
            // FFmpeg says: val = get_vlc2(... mclr ...); *p++ = val; *p++ = val; ...
            // So it fills with the value.
            // But wait, mclrTree returns 16-bit.
            // In v4 mono blocks, it returns 2 colors (low/high).
            // Here, it seems to be just one color for solid fill.
            // Let's assume low byte.

            uint32_t targetY = subY;
            if (targetY < header_.height)
            {
                frameBuffer_[targetY * header_.width + subX] = color;
                frameBuffer_[targetY * header_.width + subX + 1] = color;
                if (doubleHigh_ && (targetY + 1) < header_.height)
                {
                    frameBuffer_[(targetY + 1) * header_.width + subX] = color;
                    frameBuffer_[(targetY + 1) * header_.width + subX + 1] = color;
                }
            }

            targetY = subY + (doubleHigh_ ? 2 : 1);
            if (targetY < header_.height)
            {
                frameBuffer_[targetY * header_.width + subX] = color;
                frameBuffer_[targetY * header_.width + subX + 1] = color;
                if (doubleHigh_ && (targetY + 1) < header_.height)
                {
                    frameBuffer_[(targetY + 1) * header_.width + subX] = color;
                    frameBuffer_[(targetY + 1) * header_.width + subX + 1] = color;
                }
            }
        }
        else
        {
            // Standard pixel decoding (2 pairs of pixels)
            for (int j = 0; j < 2; j++)
            {
                uint16_t val = fullTree_->decode(bits);
                uint8_t p0 = static_cast<uint8_t>(val & 0xFF);
                uint8_t p1 = static_cast<uint8_t>((val >> 8) & 0xFF);

                uint32_t px = subX + j;
                if (px < header_.width)
                {
                    if (subY < header_.height)
                    {
                        frameBuffer_[subY * header_.width + px] = p0;
                        if (doubleHigh_ && (subY + 1) < header_.height)
                        {
                            frameBuffer_[(subY + 1) * header_.width + px] = p0;
                        }
                    }

                    uint32_t targetY1 = subY + (doubleHigh_ ? 2 : 1);
                    if (targetY1 < header_.height)
                    {
                        frameBuffer_[targetY1 * header_.width + px] = p1;
                        if (doubleHigh_ && (targetY1 + 1) < header_.height)
                        {
                            frameBuffer_[(targetY1 + 1) * header_.width + px] = p1;
                        }
                    }
                }
            }
        }
    }
}

std::vector<uint8_t> SmackerDecoder::getFrameRGBA(uint32_t frameIndex)
{
    SmackerFrame frame;
    if (!decodeFrame(frameIndex, frame))
    {
        return {};
    }

    if (rgbaBuffer_.size() != frame.width * frame.height * 4)
    {
        rgbaBuffer_.resize(frame.width * frame.height * 4);
    }

    for (size_t i = 0; i < frame.pixels.size(); i++)
    {
        uint8_t palIdx = frame.pixels[i];
        rgbaBuffer_[i * 4 + 0] = palette_[palIdx * 3 + 0];
        rgbaBuffer_[i * 4 + 1] = palette_[palIdx * 3 + 1];
        rgbaBuffer_[i * 4 + 2] = palette_[palIdx * 3 + 2];
        rgbaBuffer_[i * 4 + 3] = 255;
    }

    return rgbaBuffer_;
}

void SmackerDecoder::reset()
{
    std::fill(frameBuffer_.begin(), frameBuffer_.end(), 0);
    lastDecodedFrame_ = UINT32_MAX;
    frameOffsets_.clear();
    frameKeyFlags_.clear();
    keyframeIndices_.clear();

    for (auto& state : binkAudioStates_)
    {
        state.frameLen = 0;
        state.channels = 0;
        state.lastFrame = UINT32_MAX;
        state.overlap.clear();
    }
}

namespace
{
// Smacker embedded Bink audio frequency bands (25 entries, no 24000 terminal band).
static const uint16_t kSmackerBinkAudioBands[] = {
    0,    100,  200,  300,  400,  510,  630,  770,  920,  1080, 1270,  1480, 1720,
    2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500};

static float getWindow(size_t i, size_t n)
{
    constexpr float kPi = 3.14159265358979323846f;
    return std::sin(kPi * (static_cast<float>(i) + 0.5f) / static_cast<float>(n));
}

static void rdft(float* data, size_t n, bool inverse)
{
    util::FFT::rdft(std::span<float>(data, n), inverse);
}

} // namespace

SmackerAudioInfo SmackerDecoder::getAudioInfo(int track) const
{
    SmackerAudioInfo info;
    if (track < 0 || track >= 7)
    {
        return info;
    }

    uint32_t rate = header_.audioRate[track];
    if (rate == 0)
    {
        return info;
    }

    info.hasAudio = (rate & 0x40000000) != 0;                           // Bit 30: has data
    info.isCompressed = (rate & 0x80000000) != 0;                       // Bit 31: compressed
    info.is16Bit = (rate & 0x20000000) != 0;                            // Bit 29: 16-bit
    info.isStereo = (rate & 0x10000000) != 0;                           // Bit 28: stereo
    info.isBinkAudio = info.isCompressed && ((rate & 0x08000000) != 0); // Bit 27: Bink audio
    info.sampleRate = rate & 0x00FFFFFF;                                // Bits 0-23: sample rate

    return info;
}

bool SmackerDecoder::hasAudio(int track) const
{
    if (track < 0 || track >= 7)
    {
        return false;
    }
    uint32_t rate = header_.audioRate[track];
    return (rate & 0x40000000) != 0;
}

bool SmackerDecoder::decodeAudio(uint32_t frameIndex, int track, SmackerAudioFrame& outAudio)
{
    if (frameIndex >= header_.frameCount || track < 0 || track >= 7)
    {
        return false;
    }

    SmackerAudioInfo info = getAudioInfo(track);
    if (!info.hasAudio)
    {
        return false;
    }

    if (frameIndex >= frameOffsets_.size() - 1)
    {
        return false;
    }

    size_t offset = frameOffsets_[frameIndex];
    uint32_t frameSize = frameSizes_[frameIndex] & 0xFFFFFFFCu;
    if (offset + frameSize > data_.size())
    {
        return false;
    }

    const uint8_t* frameData = data_.data() + offset;
    size_t frameOffset = 0;

    uint8_t frameType = frameTypes_[frameIndex];

    // Skip palette data if present
    if (frameType & 1)
    {
        if (frameOffset >= frameSize)
        {
            return false;
        }
        uint8_t palRecordSize = frameData[frameOffset++];
        // Palette size byte represents (actual_bytes + 1) / 4
        size_t palBytes = static_cast<size_t>(palRecordSize) * 4 - 1;
        frameOffset += std::min(palBytes, frameSize - frameOffset);
    }

    // Find the audio track data
    for (int t = 0; t < 7; t++)
    {
        if (frameType & (2 << t))
        {
            if (frameOffset + 4 > frameSize)
            {
                return false;
            }

            uint32_t audioSize = 0;
            std::memcpy(&audioSize, frameData + frameOffset, 4);
            frameOffset += 4;

            if (t == track)
            {
                // Found our track - decode it
                audioSize &= 0x00FFFFFFu;
                if (audioSize > 4 && frameOffset + audioSize - 4 <= frameSize)
                {
                    if (info.isBinkAudio)
                    {
                        return decodeBinkAudioTrack(frameData + frameOffset, audioSize - 4,
                                                    frameIndex, track, outAudio);
                    }
                    return decodeAudioTrack(frameData + frameOffset, audioSize - 4, track,
                                            outAudio);
                }
                return false;
            }
            else if (audioSize > 4)
            {
                // Skip other tracks
                frameOffset += audioSize - 4;
            }
        }
    }

    return false;
}

bool SmackerDecoder::decodeBinkAudioTrack(const uint8_t* data, size_t size, uint32_t frameIndex,
                                          int track, SmackerAudioFrame& outAudio)
{
    SmackerAudioInfo info = getAudioInfo(track);
    if (!info.hasAudio || !info.isBinkAudio || size < 4)
    {
        return false;
    }

    uint8_t channels = info.isStereo ? 2 : 1;
    uint32_t sampleRate = info.sampleRate;

    size_t frameLen = 0;
    if (sampleRate < 22050)
        frameLen = 512;
    else if (sampleRate < 44100)
        frameLen = 1024;
    else
        frameLen = 2048;

    size_t overlapLen = frameLen / 16;
    size_t halfFrameLen = frameLen / 2;

    auto& state = binkAudioStates_[track];
    if (state.frameLen != frameLen || state.channels != channels)
    {
        state.frameLen = frameLen;
        state.channels = channels;
        state.overlap.assign(overlapLen * channels, 0.0f);
        state.lastFrame = UINT32_MAX;
    }
    else if (state.lastFrame != UINT32_MAX && state.lastFrame + 1 != frameIndex)
    {
        std::fill(state.overlap.begin(), state.overlap.end(), 0.0f);
    }

    BitReader bits(data, size);

    uint32_t sampleCount = bits.readBits(32);
    if (sampleCount == 0 || sampleCount > 10 * 1024 * 1024)
    {
        return false;
    }

    outAudio.sampleRate = sampleRate;
    outAudio.channels = channels;
    outAudio.is16Bit = true;

    outAudio.samples.resize(static_cast<size_t>(sampleCount) * channels);

    constexpr size_t kBandCount =
        sizeof(kSmackerBinkAudioBands) / sizeof(kSmackerBinkAudioBands[0]);
    size_t numBands = 1;
    for (size_t i = 1; i < kBandCount; i++)
    {
        if (kSmackerBinkAudioBands[i] * halfFrameLen / 22050 >= halfFrameLen)
        {
            break;
        }
        numBands = i + 1;
    }

    std::vector<float> coeffs(frameLen);
    std::vector<float> window(frameLen);

    for (size_t i = 0; i < frameLen; i++)
    {
        window[i] = getWindow(i, frameLen);
    }

    size_t outPos = 0;
    size_t remaining = outAudio.samples.size();

    while (remaining > 0 && !bits.atEnd())
    {
        for (uint16_t ch = 0; ch < channels; ch++)
        {
            if (bits.atEnd())
                break;

            std::fill(coeffs.begin(), coeffs.end(), 0.0f);

            std::vector<float> quant(numBands);
            for (size_t i = 0; i < numBands; i++)
            {
                uint32_t q = bits.readBits(8);
                if (q > 0)
                {
                    quant[i] = std::pow(2.0f, static_cast<float>(q) - 127.0f);
                }
                else
                {
                    quant[i] = 0.0f;
                }
            }

            for (size_t band = 0; band < numBands; band++)
            {
                size_t startBin = kSmackerBinkAudioBands[band] * halfFrameLen / 22050;
                size_t endBin = (band + 1 < numBands)
                                    ? (kSmackerBinkAudioBands[band + 1] * halfFrameLen / 22050)
                                    : halfFrameLen;

                if (startBin >= halfFrameLen)
                    break;
                if (endBin > halfFrameLen)
                    endBin = halfFrameLen;

                for (size_t bin = startBin; bin < endBin; bin++)
                {
                    if (bits.atEnd())
                        break;

                    if (quant[band] == 0.0f)
                    {
                        coeffs[bin] = 0.0f;
                        continue;
                    }

                    uint32_t zeros = 0;
                    while (!bits.atEnd() && !bits.readBit())
                    {
                        zeros++;
                        if (zeros > 100)
                            break;
                    }

                    if (zeros > 0)
                    {
                        bin += zeros - 1;
                        if (bin >= endBin)
                            break;
                    }

                    int sign = bits.readBit() ? -1 : 1;
                    coeffs[bin] = quant[band] * static_cast<float>(sign);
                }
            }

            rdft(coeffs.data(), frameLen, true);

            size_t overlapOffset = ch * overlapLen;
            for (size_t i = 0; i < frameLen; i++)
            {
                size_t idx = outPos + i * channels + ch;
                if (idx >= outAudio.samples.size())
                {
                    break;
                }

                float sample = coeffs[i] * window[i];

                if (i < overlapLen)
                {
                    sample += state.overlap[overlapOffset + i];
                }

                float scaled = sample * 32767.0f;
                if (scaled > 32767.0f)
                    scaled = 32767.0f;
                if (scaled < -32768.0f)
                    scaled = -32768.0f;

                outAudio.samples[idx] = static_cast<int16_t>(scaled);
            }

            for (size_t i = 0; i < overlapLen; i++)
            {
                size_t srcIdx = frameLen - overlapLen + i;
                state.overlap[overlapOffset + i] = coeffs[srcIdx] * window[srcIdx];
            }
        }

        outPos += (frameLen - overlapLen) * channels;
        if (outPos >= remaining)
            break;
    }

    for (size_t i = outPos; i < outAudio.samples.size(); i++)
    {
        outAudio.samples[i] = 0;
    }

    state.lastFrame = frameIndex;
    return true;
}

bool SmackerDecoder::decodeAudioTrack(const uint8_t* data, size_t size, int track,
                                      SmackerAudioFrame& outAudio)
{
    SmackerAudioInfo info = getAudioInfo(track);
    if (!info.hasAudio || size < 4)
    {
        return false;
    }

    outAudio.sampleRate = info.sampleRate;
    outAudio.channels = info.isStereo ? 2 : 1;
    outAudio.is16Bit = info.is16Bit;

    if (!info.isCompressed)
    {
        // Uncompressed PCM - just copy
        size_t sampleSize = info.is16Bit ? 2 : 1;
        size_t numSamples = size / sampleSize;

        outAudio.samples.resize(numSamples);

        if (info.is16Bit)
        {
            for (size_t i = 0; i < numSamples && i * 2 + 1 < size; i++)
            {
                outAudio.samples[i] = static_cast<int16_t>(data[i * 2] | (data[i * 2 + 1] << 8));
            }
        }
        else
        {
            for (size_t i = 0; i < numSamples && i < size; i++)
            {
                // Convert 8-bit unsigned to 16-bit signed
                outAudio.samples[i] = static_cast<int16_t>((data[i] - 128) << 8);
            }
        }
        return true;
    }

    // Compressed DPCM audio
    BitReader bits(data, size);

    // Read uncompressed length
    uint32_t unpackedLength = bits.readBits(32);
    if (unpackedLength == 0 || unpackedLength > 10 * 1024 * 1024)
    {
        return false;
    }

    // Check data presence
    if (!bits.readBit())
    {
        return false; // No data
    }

    bool isStereo = bits.readBit();
    bool is16Bit = bits.readBit();

    // Determine number of trees needed
    int numTrees = 1;
    if (is16Bit)
        numTrees *= 2;
    if (isStereo)
        numTrees *= 2;

    // Build Huffman trees for audio
    std::vector<Huff8Tree> audioTrees(numTrees);
    for (int i = 0; i < numTrees; i++)
    {
        if (!audioTrees[i].build(bits))
        {
            return false;
        }
    }

    // Read base values
    std::vector<int16_t> bases(isStereo ? 2 : 1, 0);

    if (is16Bit)
    {
        // 16-bit: high byte first, then low byte
        // Right channel first if stereo, then left
        if (isStereo)
        {
            int16_t rightHi = static_cast<int16_t>(bits.readBits(8));
            int16_t rightLo = static_cast<int16_t>(bits.readBits(8));
            bases[1] = static_cast<int16_t>((rightHi << 8) | rightLo);

            int16_t leftHi = static_cast<int16_t>(bits.readBits(8));
            int16_t leftLo = static_cast<int16_t>(bits.readBits(8));
            bases[0] = static_cast<int16_t>((leftHi << 8) | leftLo);
        }
        else
        {
            int16_t hi = static_cast<int16_t>(bits.readBits(8));
            int16_t lo = static_cast<int16_t>(bits.readBits(8));
            bases[0] = static_cast<int16_t>((hi << 8) | lo);
        }
    }
    else
    {
        // 8-bit base values
        if (isStereo)
        {
            bases[1] = static_cast<int16_t>((bits.readBits(8) - 128) << 8);
            bases[0] = static_cast<int16_t>((bits.readBits(8) - 128) << 8);
        }
        else
        {
            bases[0] = static_cast<int16_t>((bits.readBits(8) - 128) << 8);
        }
    }

    // Calculate sample count
    size_t bytesPerSample = is16Bit ? 2 : 1;
    size_t channelCount = isStereo ? 2 : 1;
    size_t sampleCount = unpackedLength / (bytesPerSample * channelCount);

    outAudio.samples.resize(sampleCount * channelCount);

    // Output base values as first samples
    size_t outIdx = 0;
    for (size_t ch = 0; ch < channelCount && outIdx < outAudio.samples.size(); ch++)
    {
        outAudio.samples[outIdx++] = bases[ch];
    }

    // Decode remaining samples
    while (outIdx < outAudio.samples.size() && !bits.atEnd())
    {
        for (size_t ch = 0; ch < channelCount && outIdx < outAudio.samples.size(); ch++)
        {
            if (is16Bit)
            {
                // Decode low byte delta, then high byte delta.
                // Smacker relies on wraparound arithmetic, not clamping.
                int treeIdxLo = static_cast<int>(ch * 2);
                int treeIdxHi = static_cast<int>(ch * 2 + 1);

                uint16_t deltaLo = static_cast<uint16_t>(audioTrees[treeIdxLo].lookup(bits));
                uint16_t deltaHi = static_cast<uint16_t>(audioTrees[treeIdxHi].lookup(bits));
                uint16_t delta = static_cast<uint16_t>((deltaHi << 8) | deltaLo);

                uint16_t prev = static_cast<uint16_t>(bases[ch]);
                uint16_t next = static_cast<uint16_t>(prev + delta);
                bases[ch] = static_cast<int16_t>(next);
                outAudio.samples[outIdx++] = bases[ch];
            }
            else
            {
                // 8-bit: single delta per sample
                int8_t delta = static_cast<int8_t>(audioTrees[ch].lookup(bits));
                uint8_t prev = static_cast<uint8_t>((bases[ch] >> 8) + 128); // Convert to unsigned
                uint8_t next = static_cast<uint8_t>(prev + delta);
                bases[ch] = static_cast<int16_t>((static_cast<int>(next) - 128) << 8);
                outAudio.samples[outIdx++] = bases[ch];
            }
        }
    }

    return true;
}

} // namespace runeharbor::media
