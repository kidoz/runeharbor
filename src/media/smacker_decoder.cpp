// SPDX-License-Identifier: MIT
// Based on publicly available Smacker format documentation and libsmacker algorithms
#include "smacker_decoder.hpp"

#include <algorithm>
#include <cstring>

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
    size_t size = 0;

    bool buildRecursive(BitReader& bits)
    {
        if (size >= 511)
        {
            return false;  // Max size exceeded
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
            tree.push_back(static_cast<uint16_t>(bits.readBits(8)));
            size++;
        }

        return true;
    }

    bool build(BitReader& bits)
    {
        tree.clear();
        size = 0;

        // Check presence bit
        if (!bits.readBit())
        {
            // Empty tree - single leaf with value 0
            tree.push_back(0);
            size = 1;
            return true;
        }

        // Build tree recursively
        if (!buildRecursive(bits))
        {
            return false;
        }

        // Consume terminator bit
        // Note: While spec says should be 0, some files have 1
        // We accept either to maximize compatibility
        bits.readBit();

        return true;
    }

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
    bool isEmpty = true;

    bool buildRecursive(BitReader& bits, const Huff8Tree& low8, const Huff8Tree& hi8,
                        size_t limit)
    {
        // Allow building past limit - we'll handle this later
        // Some files have tree structures larger than allocated space
        if (bits.atEnd())
        {
            return true;  // Ran out of bits - tree is complete
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
        }

        // Calculate limit from allocation size
        size_t limit = 0;
        if (allocSize >= 12 && (allocSize % 4) == 0)
        {
            limit = (allocSize - 12) / 4;
        }
        else
        {
            limit = 65536;  // Large default if allocSize is unusual
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
        // Note: When sub-trees are empty and tree has only 1 node,
        // the terminator may not be 0 (observed in some files)
        bits.readBit();  // Consume terminator without strict check

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
        // Keep original cache values but reset order
        // (not strictly necessary but matches libsmacker behavior)
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
    if (header_.width == 0 || header_.height == 0 || header_.width > 4096 ||
        header_.height > 4096)
    {
        return false;
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
    // Negative (> -10000): microseconds per frame (absolute value)
    // Negative (<= -10000): 100000 / (|rate| - 10000) fps

    if (header_.frameRate > 0)
    {
        return 1000000.0 / header_.frameRate;
    }
    else if (header_.frameRate > -10000)
    {
        // Absolute value is microseconds per frame
        return 1000000.0 / (-header_.frameRate);
    }
    else
    {
        return 100000.0 / (-header_.frameRate - 10000);
    }
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

    // For delta frames, decode from last keyframe or beginning
    if (frameIndex < lastDecodedFrame_ || lastDecodedFrame_ == UINT32_MAX)
    {
        std::fill(frameBuffer_.begin(), frameBuffer_.end(), 0);

        for (uint32_t i = 0; i <= frameIndex; i++)
        {
            if (!decodeFrameInternal(i))
            {
                return false;
            }
        }
    }
    else
    {
        for (uint32_t i = lastDecodedFrame_ + 1; i <= frameIndex; i++)
        {
            if (!decodeFrameInternal(i))
            {
                return false;
            }
        }
    }

    lastDecodedFrame_ = frameIndex;

    outFrame.pixels = frameBuffer_;
    outFrame.width = header_.width;
    outFrame.height = header_.height;
    outFrame.isKeyframe = (frameTypes_[frameIndex] & 1) != 0;

    return true;
}

bool SmackerDecoder::decodeFrameInternal(uint32_t frameIndex)
{
    // Calculate offset to frame data
    size_t offset = dataOffset_;
    for (uint32_t i = 0; i < frameIndex; i++)
    {
        offset += frameSizes_[i] & 0x3FFFFFFF;
    }

    uint32_t frameSize = frameSizes_[frameIndex] & 0x3FFFFFFF;
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
        size_t palBytes = (static_cast<size_t>(palRecordSize) + 1) * 3;

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
    if (!typeTree_ || typeTree_->isEmpty)
    {
        return true;
    }

    uint32_t blocksWide = (header_.width + 3) / 4;
    uint32_t blocksHigh = (header_.height + 3) / 4;
    uint32_t totalBlocks = blocksWide * blocksHigh;
    uint32_t blockIdx = 0;

    while (blockIdx < totalBlocks && !bits.atEnd())
    {
        uint16_t typeDesc = typeTree_->decode(bits);
        BlockType blockType = static_cast<BlockType>(typeDesc & 3);
        uint32_t runLength = (typeDesc >> 2) + 1;

        for (uint32_t r = 0; r < runLength && blockIdx < totalBlocks; r++, blockIdx++)
        {
            uint32_t bx = blockIdx % blocksWide;
            uint32_t by = blockIdx / blocksWide;
            uint32_t x = bx * 4;
            uint32_t y = by * 4;

            switch (blockType)
            {
            case BLOCK_SKIP:
                decodeBlockSkip(x, y);
                break;

            case BLOCK_FILL:
            {
                uint16_t color = mmapTree_ ? mmapTree_->decode(bits) : 0;
                decodeBlockFill(x, y, static_cast<uint8_t>(color & 0xFF));
                break;
            }

            case BLOCK_MONO:
            {
                uint16_t colors = mclrTree_ ? mclrTree_->decode(bits) : 0;
                uint8_t c0 = static_cast<uint8_t>(colors & 0xFF);
                uint8_t c1 = static_cast<uint8_t>((colors >> 8) & 0xFF);
                uint16_t bitmap = fullTree_ ? fullTree_->decode(bits) : 0;
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
                size_t src = (srcIdx + i) * 3;
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
            if (srcPos + 2 > size)
            {
                break;
            }

            uint8_t r = data[srcPos++] & 0x3F;
            uint8_t g = data[srcPos++] & 0x3F;
            uint8_t b = (srcPos < size) ? (data[srcPos++] & 0x3F) : 0;

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
    for (uint32_t py = 0; py < 4 && (y + py) < header_.height; py++)
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
    for (uint32_t py = 0; py < 4 && (y + py) < header_.height; py++)
    {
        for (uint32_t px = 0; px < 4 && (x + px) < header_.width; px++)
        {
            size_t idx = (y + py) * header_.width + (x + px);
            uint32_t bitIdx = py * 4 + px;
            frameBuffer_[idx] = (bitmap & (1 << bitIdx)) ? c1 : c0;
        }
    }
}

void SmackerDecoder::decodeBlockFull(BitReader& bits, uint32_t x, uint32_t y)
{
    if (!fullTree_)
    {
        return;
    }

    if (isV4_)
    {
        // SMK4: first value determines mode
        uint16_t first = fullTree_->decode(bits);

        if (first == fullTree_->cache[0] && (fullTree_->tree[0] & HUFF16_CACHE) == 0)
        {
            // Might be escape code - check original cache values
        }

        // Check for v4 escape codes stored in original cache
        // Note: escape codes are the ORIGINAL cache values, not current

        // For now, treat as regular 16-pixel block
        frameBuffer_[y * header_.width + x] = static_cast<uint8_t>(first & 0xFF);

        for (uint32_t p = 1; p < 16; p++)
        {
            uint16_t color = fullTree_->decode(bits);
            uint32_t py = p / 4;
            uint32_t px = p % 4;

            if ((y + py) < header_.height && (x + px) < header_.width)
            {
                size_t idx = (y + py) * header_.width + (x + px);
                frameBuffer_[idx] = static_cast<uint8_t>(color & 0xFF);
            }
        }
    }
    else
    {
        // SMK2: Always 16 individual colors
        for (uint32_t p = 0; p < 16; p++)
        {
            uint16_t color = fullTree_->decode(bits);
            uint32_t py = p / 4;
            uint32_t px = p % 4;

            if ((y + py) < header_.height && (x + px) < header_.width)
            {
                size_t idx = (y + py) * header_.width + (x + px);
                frameBuffer_[idx] = static_cast<uint8_t>(color & 0xFF);
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

    std::vector<uint8_t> rgba(frame.width * frame.height * 4);

    for (size_t i = 0; i < frame.pixels.size(); i++)
    {
        uint8_t palIdx = frame.pixels[i];
        rgba[i * 4 + 0] = palette_[palIdx * 3 + 0];
        rgba[i * 4 + 1] = palette_[palIdx * 3 + 1];
        rgba[i * 4 + 2] = palette_[palIdx * 3 + 2];
        rgba[i * 4 + 3] = 255;
    }

    return rgba;
}

void SmackerDecoder::reset()
{
    std::fill(frameBuffer_.begin(), frameBuffer_.end(), 0);
    lastDecodedFrame_ = UINT32_MAX;
}

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

    info.hasAudio = (rate & 0x40000000) != 0;      // Bit 30: has data
    info.isCompressed = (rate & 0x80000000) != 0;  // Bit 31: compressed
    info.is16Bit = (rate & 0x20000000) != 0;       // Bit 29: 16-bit
    info.isStereo = (rate & 0x10000000) != 0;      // Bit 28: stereo
    info.sampleRate = rate & 0x00FFFFFF;           // Bits 0-23: sample rate

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

    // Calculate offset to frame data
    size_t offset = dataOffset_;
    for (uint32_t i = 0; i < frameIndex; i++)
    {
        offset += frameSizes_[i] & 0x3FFFFFFF;
    }

    uint32_t frameSize = frameSizes_[frameIndex] & 0x3FFFFFFF;
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
        size_t palBytes = (static_cast<size_t>(palRecordSize) + 1) * 3;
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
                if (audioSize > 4 && frameOffset + audioSize - 4 <= frameSize)
                {
                    return decodeAudioTrack(frameData + frameOffset, audioSize - 4, track, outAudio);
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
        return false;  // No data
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
                // Decode low byte delta, then high byte delta
                int treeIdxLo = static_cast<int>(ch * 2);
                int treeIdxHi = static_cast<int>(ch * 2 + 1);

                int deltaLo = audioTrees[treeIdxLo].lookup(bits);
                int deltaHi = audioTrees[treeIdxHi].lookup(bits);

                // Apply deltas with overflow handling
                int16_t prev = bases[ch];
                int newLo = (prev & 0xFF) + deltaLo;
                int newHi = ((prev >> 8) & 0xFF) + deltaHi;

                // Handle overflow from low to high byte
                if (newLo > 255)
                {
                    newLo -= 256;
                    newHi++;
                }
                else if (newLo < 0)
                {
                    newLo += 256;
                    newHi--;
                }

                bases[ch] = static_cast<int16_t>((newHi << 8) | (newLo & 0xFF));
                outAudio.samples[outIdx++] = bases[ch];
            }
            else
            {
                // 8-bit: single delta per sample
                int delta = audioTrees[ch].lookup(bits);
                int prev = (bases[ch] >> 8) + 128;  // Convert back to unsigned
                int newVal = prev + delta;

                // Clamp to 0-255
                if (newVal > 255)
                    newVal = 255;
                if (newVal < 0)
                    newVal = 0;

                bases[ch] = static_cast<int16_t>((newVal - 128) << 8);
                outAudio.samples[outIdx++] = bases[ch];
            }
        }
    }

    return true;
}

} // namespace runeharbor::media
