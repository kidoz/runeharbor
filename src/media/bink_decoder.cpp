// SPDX-License-Identifier: MIT
// Software Bink decoder (clean-room, cross-platform).
#include "bink_decoder.hpp"

#include <algorithm>

#include <cmath>
#include <cstring>

#include "../util/fft.hpp"

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
     0,  1,  8,  9,  2,  3, 10, 11,
     4,  5, 12, 13,  6,  7, 14, 15,
    20, 21, 28, 29, 22, 23, 30, 31,
    16, 17, 24, 25, 32, 33, 40, 41,
    34, 35, 42, 43, 48, 49, 56, 57,
    50, 51, 58, 59, 18, 19, 26, 27,
    36, 37, 44, 45, 38, 39, 46, 47,
    52, 53, 60, 61, 54, 55, 62, 63
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
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

// Bink patterns for version 'f' and later
static const uint64_t binkPatterns[64] = {
    0x0000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
    0x3333333333333333ULL, 0xCCCCCCCCCCCCCCCCULL, 0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL,
    0x00FF00FF00FF00FFULL, 0xFF00FF00FF00FF00ULL, 0x0000FFFF0000FFFFULL, 0xFFFF0000FFFF0000ULL,
    0x00000000FFFFFFFFULL, 0xFFFFFFFF00000000ULL, 0x3131313131313131ULL, 0x4B4B4B4B4B4B4B4BULL,
    0x314B314B314B314BULL, 0x4B314B314B314B31ULL, 0x31314B4B31314B4BULL, 0x4B4B31314B4B3131ULL,
    0x314B4B31314B4B31ULL, 0x4B31314B4B31314BULL, 0x0101010101010101ULL, 0x8080808080808080ULL,
    0x0180018001800180ULL, 0x8001800180018001ULL, 0x0101808001018080ULL, 0x8080010180800101ULL,
    0x0180800101808001ULL, 0x8001018080010180ULL, 0x1313131313131313ULL, 0x4848484848484848ULL,
    0x1348134813481348ULL, 0x4813481348134813ULL, 0x1313484813134848ULL, 0x4848131348481313ULL,
    0x1348481313484813ULL, 0x4813134848131348ULL, 0x0303030303030303ULL, 0xC0C0C0C0C0C0C0C0ULL,
    0x03C003C003C003C0ULL, 0xC003C003C003C003ULL, 0x0303C0C00303C0C0ULL, 0xC0C00303C0C00303ULL,
    0x03C0C00303C0C003ULL, 0xC00303C0C00303C0ULL, 0x0707070707070707ULL, 0xE0E0E0E0E0E0E0E0ULL,
    0x07E007E007E007E0ULL, 0xE007E007E007E007ULL, 0x0707E0E00707E0E0ULL, 0xE0E00707E0E00707ULL,
    0x07E0E00707E0E007ULL, 0xE00707E0E00707E0ULL, 0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL,
    0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL, 0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL,
    0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL, 0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL};

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

    size_t byteIdx = bitPos_ / 8;
    size_t bitIdx = bitPos_ % 8;
    bitPos_++;

    return (data_[byteIdx] >> bitIdx) & 1;
}

bool BinkBitReader::peekBit(int offset)
{
    size_t pos = bitPos_ + offset;
    if (pos >= maxBits_)
    {
        return false;
    }

    size_t byteIdx = pos / 8;
    size_t bitIdx = pos % 8;

    return (data_[byteIdx] >> bitIdx) & 1;
}

uint32_t BinkBitReader::readBits(int count)
{
    if (count <= 0)
        return 0;
    if (bitPos_ + count > maxBits_)
        count = static_cast<int>(maxBits_ - bitPos_);

    uint32_t result = 0;

    // Optimization: if we are byte aligned and reading multiple of 8 bits
    if ((bitPos_ & 7) == 0 && (count & 7) == 0 && count <= 32)
    {
        for (int i = 0; i < count; i += 8)
        {
            result |= (static_cast<uint32_t>(data_[bitPos_ / 8]) << i);
            bitPos_ += 8;
        }
        return result;
    }

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
    if (treeIdx >= 16)
        return false;

    // Initialize symbols in order
    uint8_t lens[MAX_SYMBOLS];
    for (int i = 0; i < MAX_SYMBOLS; i++)
    {
        symbols_[i] = i;
        lens[i] = binkTreeLens[treeIdx][i];
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

    // Sort symbols by length for canonical Huffman
    // and build the firstCode/firstSymbolIdx tables
    int revLens[17] = {0};
    for (int i = 0; i < MAX_SYMBOLS; i++)
    {
        if (lens[i] > 0 && lens[i] <= 16)
        {
            revLens[lens[i]]++;
        }
    }

    int code = 0;
    int symbolIdx = 0;
    for (int i = 1; i <= 16; i++)
    {
        firstCode_[i] = static_cast<uint16_t>(code);
        firstSymbolIdx_[i] = symbolIdx;
        code = (code + revLens[i]) << 1;
        symbolIdx += revLens[i];
    }

    // Build sortedSymbols_ array
    symbolIdx = 0;
    for (int l = 1; l <= 16; l++)
    {
        for (int i = 0; i < MAX_SYMBOLS; i++)
        {
            if (binkTreeLens[treeIdx][i] == l)
            {
                sortedSymbols_[symbolIdx++] = static_cast<uint8_t>(symbols_[i]);
            }
        }
    }

    // Populate lookup table for first 8 bits
    for (int i = 0; i < 256; i++)
    {
        lookup_[i].len = 0; // Not found

        // Reverse bits of i to get Huffman-style code (MSB-first for canonical matching)
        uint16_t code = 0;
        for (int b = 0; b < 8; b++)
        {
            code = (code << 1) | ((i >> b) & 1);
        }

        // Find match in lengths 1-8
        for (int l = 1; l <= 8; l++)
        {
            uint16_t currentCode = code >> (8 - l);
            if (l < 16)
            {
                uint16_t nextFirst = firstCode_[l + 1];
                if (currentCode < nextFirst)
                {
                    int idx = firstSymbolIdx_[l] + (currentCode - firstCode_[l]);
                    if (idx >= 0 && idx < MAX_SYMBOLS)
                    {
                        lookup_[i].symbol = sortedSymbols_[idx];
                        lookup_[i].len = static_cast<uint8_t>(l);
                        break;
                    }
                }
            }
        }
    }

    return true;
}

int BinkTree::decode(BinkBitReader& bits) const
{
    if (numSymbols_ <= 0)
    {
        return 0;
    }

    // Try 8-bit lookup first
    uint16_t peeked = 0;
    int peekLen = 0;
    for (int i = 0; i < 8; i++)
    {
        if (bits.peekBit(i))
            peeked |= (1 << i);
        peekLen++;
        if (bits.bitsRemaining() <= static_cast<size_t>(i + 1))
            break;
    }

    if (peekLen >= 8)
    {
        uint8_t byte = static_cast<uint8_t>(peeked & 0xFF);
        if (lookup_[byte].len > 0 && lookup_[byte].len <= 8)
        {
            bits.skipBits(lookup_[byte].len);
            return lookup_[byte].symbol;
        }
    }

    // Fallback to bit-by-bit for lengths > 8
    uint16_t code = 0;
    for (int len = 1; len <= 16; len++)
    {
        if (bits.atEnd())
            return 0;
        code = (code << 1) | (bits.readBit() ? 1 : 0);

        if (len < 16)
        {
            uint16_t nextFirst = firstCode_[len + 1];
            if (code < nextFirst)
            {
                int idx = firstSymbolIdx_[len] + (code - firstCode_[len]);
                if (idx >= 0 && idx < MAX_SYMBOLS)
                {
                    return sortedSymbols_[idx];
                }
            }
        }
        else
        {
            int idx = firstSymbolIdx_[len] + (code - firstCode_[len]);
            if (idx >= 0 && idx < MAX_SYMBOLS)
            {
                return sortedSymbols_[idx];
            }
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

void BinkBundle::buildTree(BinkBitReader& bits, BinkBundleType type)
{
    if (type == BinkBundleType::Colors)
    {
        for (int i = 0; i < 16; i++)
        {
            treeHigh_[i].build(bits, 4);
        }
        lastColorHigh_ = 0;
    }
    if (type != BinkBundleType::IntraDC && type != BinkBundleType::InterDC)
    {
        tree_.build(bits, 4);
    }
}

int BinkBundle::getValue()
{
    if (readPos_ < data_.size())
    {
        return data_[readPos_++];
    }
    return 0;
}

bool BinkBundle::readBlockTypes(BinkBitReader& bits, int lenBits)
{
    data_.clear();
    readPos_ = 0;
    int t = bits.readBits(lenBits);
    if (!t) return true;
    
    if (bits.readBit()) {
        int v = bits.readBits(4);
        for (int i=0; i<t; i++) data_.push_back(v);
    } else {
        while (data_.size() < (size_t)t) {
            int v = tree_.decode(bits);
            if (v < 12) {
                data_.push_back(v);
            } else {
                static const uint8_t rlelens[4] = { 4, 8, 12, 32 };
                int run = rlelens[v - 12];
                int last = data_.empty() ? 0 : data_.back();
                for (int i=0; i<run; i++) data_.push_back(last);
            }
        }
    }
    return true;
}

bool BinkBundle::readColors(BinkBitReader& bits, int lenBits)
{
    data_.clear();
    readPos_ = 0;
    int t = bits.readBits(lenBits);
    if (!t) return true;
    
    if (bits.readBit()) {
        lastColorHigh_ = treeHigh_[lastColorHigh_].decode(bits);
        int v = tree_.decode(bits);
        v = (lastColorHigh_ << 4) | v;
        
        int sign = ((int8_t)v) >> 7;
        v = ((v & 0x7F) ^ sign) - sign;
        v += 0x80;
        
        for (int i=0; i<t; i++) data_.push_back(v);
    } else {
        while (data_.size() < (size_t)t) {
            lastColorHigh_ = treeHigh_[lastColorHigh_].decode(bits);
            int v = tree_.decode(bits);
            v = (lastColorHigh_ << 4) | v;
            
            int sign = ((int8_t)v) >> 7;
            v = ((v & 0x7F) ^ sign) - sign;
            v += 0x80;
            
            data_.push_back(v);
        }
    }
    return true;
}

bool BinkBundle::readPatterns(BinkBitReader& bits, int lenBits)
{
    data_.clear();
    readPos_ = 0;
    int t = bits.readBits(lenBits);
    if (!t) return true;
    
    while (data_.size() < (size_t)t) {
        int v = tree_.decode(bits);
        v |= (tree_.decode(bits) << 4);
        data_.push_back(v);
    }
    return true;
}

bool BinkBundle::readMotionValues(BinkBitReader& bits, int lenBits)
{
    data_.clear();
    readPos_ = 0;
    int t = bits.readBits(lenBits);
    if (!t) return true;
    
    if (bits.readBit()) {
        int v = bits.readBits(4);
        if (v) {
            int sign = -bits.readBit();
            v = (v ^ sign) - sign;
        }
        for (int i=0; i<t; i++) data_.push_back(v);
    } else {
        while (data_.size() < (size_t)t) {
            int v = tree_.decode(bits);
            if (v) {
                int sign = -bits.readBit();
                v = (v ^ sign) - sign;
            }
            data_.push_back(v);
        }
    }
    return true;
}

bool BinkBundle::readDCs(BinkBitReader& bits, int lenBits, int startBits, bool hasSign)
{
    data_.clear();
    readPos_ = 0;
    int t = bits.readBits(lenBits);
    if (!t) return true;
    
    int v = bits.readBits(startBits - hasSign);
    if (v && hasSign) {
        int sign = -bits.readBit();
        v = (v ^ sign) - sign;
    }
    data_.push_back(v);
    int len = t - 1;
    
    for (int i = 0; i < len; i += 8) {
        int len2 = std::min(len - i, 8);
        int bsize = bits.readBits(4);
        if (bsize) {
            for (int j = 0; j < len2; j++) {
                int v2 = bits.readBits(bsize);
                if (v2) {
                    int sign = -bits.readBit();
                    v2 = (v2 ^ sign) - sign;
                }
                v += v2;
                data_.push_back(v);
            }
        } else {
            for (int j = 0; j < len2; j++) {
                data_.push_back(v);
            }
        }
    }
    return true;
}

bool BinkBundle::readRuns(BinkBitReader& bits, int lenBits)
{
    data_.clear();
    readPos_ = 0;
    int t = bits.readBits(lenBits);
    if (!t) return true;
    
    if (bits.readBit()) {
        int v = bits.readBits(4);
        for (int i=0; i<t; i++) data_.push_back(v);
    } else {
        while (data_.size() < (size_t)t) {
            data_.push_back(tree_.decode(bits));
        }
    }
    return true;
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
    reset();
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
    planeWidthY_ = (header_.width + 7) & ~7; // Align to 8
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
    // Bink audio track info: 12 bytes per track
    //   - 4 bytes: max decoded size (unused)
    //   - 4 bytes: sample rate (low 16) + flags (high 16)
    //   - 4 bytes: unknown/reserved (skipped)
    //
    // Flags: 0x8000 = has audio, 0x2000 = stereo, 0x1000 = DCT, 0x4000 = 16-bit

    audioTracks_.resize(header_.audioTrackCount);

    if (header_.audioTrackCount > 0)
    {
        size_t audioInfoSize = header_.audioTrackCount * 12; // Corrected to 12 bytes per track
        if (offset + audioInfoSize > data_.size())
        {
            return false;
        }

        for (uint32_t t = 0; t < header_.audioTrackCount; t++)
        {
            // Skip max decoded size (4 bytes)
            offset += 4;

            // Read 32-bit sample rate + embedded flags (4 bytes)
            uint32_t val;
            std::memcpy(&val, data_.data() + offset, 4);
            offset += 4;

            // Skip additional 4 bytes (unknown/reserved)
            offset += 4;

            uint32_t sampleRate = val & 0xFFFF;
            uint16_t audioFlags = (val >> 16) & 0xFFFF;
            
            audioTracks_[t].sampleRate = sampleRate;
            audioTracks_[t].isDCT = (audioFlags & 0x1000) != 0;
            audioTracks_[t].channels = (audioFlags & 0x2000) ? 2 : 1;
            audioTracks_[t].trackId = t;
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
                              uint32_t height, bool isChroma)
{
    // Bink plane data starts with lengths (but we just align)
    if (header_.magic[3] >= 'i') {
        bits.skipBits(32);
    }
    for (int i = 0; i < 6; i++) {
        bits.readBits(32);
    }
    if (header_.magic[3] >= 'i') {
        bits.skipBits(32);
    }

    for (int i = 0; i < static_cast<int>(BinkBundleType::Count); i++) {
        bundles_[i].buildTree(bits, static_cast<BinkBundleType>(i));
    }

    int stride = static_cast<int>(width);
    int bw = width / 8;
    int bh = height / 8;

    auto ilog2 = [](uint32_t v) {
        int r = 0;
        while (v >>= 1) r++;
        return r;
    };
    uint32_t alignedWidth = width;
    int bt_len = ilog2((alignedWidth >> 3) + 511) + 1;
    int sbt_len = ilog2((alignedWidth >> 4) + 511) + 1;
    int col_len = ilog2(bw * 64 + 511) + 1;
    int dc_len = ilog2((alignedWidth >> 3) + 511) + 1;
    int pat_len = ilog2((bw << 3) + 511) + 1;
    int run_len = ilog2(bw * 48 + 511) + 1;

    for (int by = 0; by < bh; by++)
    {
        bundles_[static_cast<int>(BinkBundleType::BlockTypes)].readBlockTypes(bits, bt_len);
        bundles_[static_cast<int>(BinkBundleType::SubBlockTypes)].readBlockTypes(bits, sbt_len);
        bundles_[static_cast<int>(BinkBundleType::Colors)].readColors(bits, col_len);
        bundles_[static_cast<int>(BinkBundleType::Pattern)].readPatterns(bits, pat_len);
        bundles_[static_cast<int>(BinkBundleType::MotionX)].readMotionValues(bits, dc_len);
        bundles_[static_cast<int>(BinkBundleType::MotionY)].readMotionValues(bits, dc_len);
        bundles_[static_cast<int>(BinkBundleType::IntraDC)].readDCs(bits, dc_len, isChroma ? 10 : 11, false);
        bundles_[static_cast<int>(BinkBundleType::InterDC)].readDCs(bits, dc_len, isChroma ? 10 : 11, true);
        bundles_[static_cast<int>(BinkBundleType::Run)].readRuns(bits, run_len);

        uint8_t* dst = plane + by * 8 * stride;
        const uint8_t* prevPtr = prev + by * 8 * stride;

        for (int bx = 0; bx < bw; bx++, dst += 8, prevPtr += 8)
        {
            int blockType = bundles_[static_cast<int>(BinkBundleType::BlockTypes)].getValue();

            if (((by & 1) || (bx & 1)) && blockType == BINK_BLOCK_SCALED) {
                bx++;
                dst += 8;
                prevPtr += 8;
                continue;
            }

            switch (blockType)
            {
            case BINK_BLOCK_SKIP:
                decodeBlockSkip(dst, prevPtr, stride);
                break;

            case BINK_BLOCK_SCALED:
                decodeBlockScaled(dst, prevPtr, stride, bits, width, height);
                bx++;
                dst += 8;
                prevPtr += 8;
                break;

            case BINK_BLOCK_MOTION:
            {
                int mvX = bundles_[static_cast<int>(BinkBundleType::MotionX)].getValue();
                int mvY = bundles_[static_cast<int>(BinkBundleType::MotionY)].getValue();
                if (mvX >= 8) mvX -= 16;
                if (mvY >= 8) mvY -= 16;
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
                if (mvX >= 8) mvX -= 16;
                if (mvY >= 8) mvY -= 16;
                int dc = bundles_[static_cast<int>(BinkBundleType::InterDC)].getValue();
                decodeBlockInterDCT(dst, prevPtr, stride, bits, mvX, mvY, dc, width, height);
                break;
            }

            case BINK_BLOCK_FILL:
            {
                int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                decodeBlockFill(dst, stride, static_cast<uint8_t>(color));
                break;
            }

            case BINK_BLOCK_RESIDUE:
            {
                int mvX = bundles_[static_cast<int>(BinkBundleType::MotionX)].getValue();
                int mvY = bundles_[static_cast<int>(BinkBundleType::MotionY)].getValue();
                if (mvX >= 8) mvX -= 16;
                if (mvY >= 8) mvY -= 16;
                decodeBlockResidue(dst, prevPtr, stride, bits, mvX, mvY, width, height);
                break;
            }

            default:
                decodeBlockSkip(dst, prevPtr, stride);
                break;
            }
        }
    }

    bits.align32();
    return true;
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
        if (run < 1)
            run = 1; // Safety against infinite loop

        // Apply run
        for (int i = 0; i < run && pos < 64; i++, pos++)
        {
            int x = binkPatternScan[pos] % 8;
            int y = binkPatternScan[pos] / 8;
            dst[y * stride + x] = static_cast<uint8_t>(color);
        }
    }

    // Fill remaining with last color (if any)
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
    uint64_t bitmap;
    if (header_.magic[3] >= 'f')
    {
        bitmap = binkPatterns[pattern & 0x3F];
    }
    else
    {
        bitmap = 0;
        for (int i = 0; i < 8; i++)
        {
            bitmap |= (static_cast<uint64_t>(pattern) << (i * 8));
        }
    }

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            dst[y * stride + x] = (bitmap & (1ULL << (y * 8 + x))) ? c1 : c0;
        }
    }
}

void BinkDecoder::decodeBlockRaw(uint8_t* dst, int stride, BinkBitReader& /*bits*/)
{
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
    ptrdiff_t offset = dst - prev;
    int srcY = static_cast<int>(offset / stride);
    int srcX = static_cast<int>(offset % stride);

    int refX = srcX + mvX;
    int refY = srcY + mvY;

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
    block[0] = dc * 8;
    readDCTCoeffs(bits, block);
    idct8x8(block);
    addBlock(dst, stride, block);
}

void BinkDecoder::decodeBlockInterDCT(uint8_t* dst, const uint8_t* prev, int stride,
                                      BinkBitReader& bits, int mvX, int mvY, int dc,
                                      uint32_t planeWidth, uint32_t planeHeight)
{
    decodeBlockMotion(dst, prev, stride, mvX, mvY, planeWidth, planeHeight);
    int block[64] = {0};
    block[0] = dc * 8;
    readDCTCoeffs(bits, block);
    idct8x8(block);
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            int val = dst[y * stride + x] + block[y * 8 + x];
            dst[y * stride + x] = static_cast<uint8_t>(std::clamp(val, 0, 255));
        }
    }
}

void BinkDecoder::decodeBlockScaled(uint8_t* dst, const uint8_t* prev, int stride,
                                    BinkBitReader& /*bits*/, uint32_t /*planeWidth*/,
                                    uint32_t /*planeHeight*/)
{
    for (int sy = 0; sy < 2; sy++)
    {
        for (int sx = 0; sx < 2; sx++)
        {
            int subType = bundles_[static_cast<int>(BinkBundleType::SubBlockTypes)].getValue();
            uint8_t* subDst = dst + sy * 4 * stride + sx * 4;
            const uint8_t* subPrev = prev + sy * 4 * stride + sx * 4;

            switch (subType)
            {
            case BINK_BLOCK_SKIP:
                for (int y = 0; y < 4; y++)
                    std::memcpy(subDst + y * stride, subPrev + y * stride, 4);
                break;
            case BINK_BLOCK_FILL:
            {
                int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                for (int y = 0; y < 4; y++)
                    std::memset(subDst + y * stride, color, 4);
                break;
            }
            case BINK_BLOCK_PATTERN:
            {
                int c0 = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                int c1 = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                int pattern = bundles_[static_cast<int>(BinkBundleType::Pattern)].getValue();
                for (int y = 0; y < 4; y++)
                {
                    for (int x = 0; x < 4; x++)
                    {
                        subDst[y * stride + x] = (pattern & (1 << (y * 4 + x))) ? c1 : c0;
                    }
                }
                break;
            }
            case BINK_BLOCK_RAW:
                for (int i = 0; i < 16; i++)
                {
                    int color = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
                    subDst[(i / 4) * stride + (i % 4)] = static_cast<uint8_t>(color);
                }
                break;
            default:
                for (int y = 0; y < 4; y++)
                    std::memcpy(subDst + y * stride, subPrev + y * stride, 4);
                break;
            }
        }
    }
}

void BinkDecoder::decodeBlockResidue(uint8_t* dst, const uint8_t* prev, int stride,
                                     BinkBitReader& /*bits*/, int mvX, int mvY, uint32_t planeWidth,
                                     uint32_t planeHeight)
{
    decodeBlockMotion(dst, prev, stride, mvX, mvY, planeWidth, planeHeight);
    for (int i = 0; i < 64; i++)
    {
        int residue = bundles_[static_cast<int>(BinkBundleType::Colors)].getValue();
        if (residue >= 128)
            residue -= 256;
        int x = binkPatternScan[i] % 8;
        int y = binkPatternScan[i] / 8;
        int val = dst[y * stride + x] + residue;
        dst[y * stride + x] = static_cast<uint8_t>(std::clamp(val, 0, 255));
    }
}

void BinkDecoder::readDCTCoeffs(BinkBitReader& /*bits*/, int* block)
{
    // Simple coefficient reading - just get from bundles
    // Full implementation would use proper entropy decoding
    for (int i = 1; i < 64; i++) // Removed bits.atEnd() check
    {
        int coeff = bundles_[static_cast<int>(BinkBundleType::Run)].getValue(); // Changed to Run
        if (coeff >= 128)
            coeff -= 256;
        block[binkScan[i]] = coeff;
    }
}

void BinkDecoder::idct8x8(int* block)
{
    int i;
    int temp[64];
    for (i = 0; i < 8; i++) {
        int a0 = block[i*8 + 0] + block[i*8 + 4];
        int a1 = block[i*8 + 0] - block[i*8 + 4];
        int a2 = block[i*8 + 2] + block[i*8 + 6];
        int a3 = block[i*8 + 2] - block[i*8 + 6];
        int a4 = block[i*8 + 1] + block[i*8 + 5];
        int a5 = block[i*8 + 1] - block[i*8 + 5];
        int a6 = block[i*8 + 3] + block[i*8 + 7];
        int a7 = block[i*8 + 3] - block[i*8 + 7];
        int b0 = a0 + a2;
        int b1 = a1 + a3;
        int b2 = a1 - a3;
        int b3 = a0 - a2;
        int b4 = a4 + a6;
        int b5 = a5 + a7;
        int b6 = a5 - a7;
        int b7 = a4 - a6;
        temp[i*8 + 0] = b0 + b4;
        temp[i*8 + 1] = b1 + b5;
        temp[i*8 + 2] = b2 + b6;
        temp[i*8 + 3] = b3 + b7;
        temp[i*8 + 4] = b3 - b7;
        temp[i*8 + 5] = b2 - b6;
        temp[i*8 + 6] = b1 - b5;
        temp[i*8 + 7] = b0 - b4;
    }
    for (i = 0; i < 8; i++) {
        int a0 = temp[0*8 + i] + temp[4*8 + i];
        int a1 = temp[0*8 + i] - temp[4*8 + i];
        int a2 = temp[2*8 + i] + temp[6*8 + i];
        int a3 = temp[2*8 + i] - temp[6*8 + i];
        int a4 = temp[1*8 + i] + temp[5*8 + i];
        int a5 = temp[1*8 + i] - temp[5*8 + i];
        int a6 = temp[3*8 + i] + temp[7*8 + i];
        int a7 = temp[3*8 + i] - temp[7*8 + i];
        int b0 = a0 + a2;
        int b1 = a1 + a3;
        int b2 = a1 - a3;
        int b3 = a0 - a2;
        int b4 = a4 + a6;
        int b5 = a5 + a7;
        int b6 = a5 - a7;
        int b7 = a4 - a6;
        block[0*8 + i] = b0 + b4;
        block[1*8 + i] = b1 + b5;
        block[2*8 + i] = b2 + b6;
        block[3*8 + i] = b3 + b7;
        block[4*8 + i] = b3 - b7;
        block[5*8 + i] = b2 - b6;
        block[6*8 + i] = b1 - b5;
        block[7*8 + i] = b0 - b4;
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
            val = (val + 128) >> 4; // Scale down and add DC offset
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

std::optional<BinkYUVPlanes> BinkDecoder::getYUVPlanes() const
{
    if (planeY_.empty())
    {
        return std::nullopt;
    }

    BinkYUVPlanes planes;
    planes.y = planeY_.data();
    planes.u = planeU_.data();
    planes.v = planeV_.data();
    planes.yStride = planeWidthY_;
    planes.uvStride = planeWidthC_;
    planes.width = header_.width;
    planes.height = header_.height;
    return planes;
}

void BinkDecoder::reset()
{
    data_.clear();
    frameOffsets_.clear();
    frameKeyFlags_.clear();
    audioTracks_.clear();

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
                return false; // No audio data for this track
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

// Bink audio frequency bands (25 critical bands)
static const uint16_t binkAudioBands[26] = {0,    100,  200,  300,  400,  510,   630,   770,  920,
                                            1080, 1270, 1480, 1720, 2000, 2320,  2700,  3150, 3700,
                                            4400, 5300, 6400, 7700, 9500, 12000, 15500, 24000};

// Bink quantization table (96 entries, matching FFmpeg/RAD spec)
// Formula: bink_quant[i] = exp(i * 0.15289164788) * 0.066399999708
static float binkQuantTable[96] = {};
static bool binkQuantInit = false;

static void initBinkQuantTable()
{
    if (binkQuantInit)
        return;
    for (int i = 0; i < 96; i++)
    {
        binkQuantTable[i] = std::exp(static_cast<float>(i) * 0.15289164788f) * 0.066399999708f;
    }
    binkQuantInit = true;
}

// Window coefficients for overlap-add (generated from sine window)
static float getWindow(size_t i, size_t n)
{
    constexpr float PI = 3.14159265358979323846f;
    return std::sin(PI * (static_cast<float>(i) + 0.5f) / static_cast<float>(n));
}

bool BinkDecoder::decodeAudioTrack(BinkBitReader& bits, uint32_t track, BinkAudioFrame& outAudio)
{
    if (track >= audioTracks_.size())
    {
        return false;
    }

    initBinkQuantTable();

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
    size_t frameLen = audioFrameSize_;
    if (frameLen == 0)
    {
        if (trackInfo.sampleRate < 22050)
            frameLen = 2048;
        else if (trackInfo.sampleRate < 44100)
            frameLen = 4096;
        else
            frameLen = 8192;
    }

    size_t overlapLen = frameLen / 16;
    size_t halfFrameLen = frameLen / 2;

    // Initialize overlap buffer if needed
    size_t overlapSize = overlapLen * trackInfo.channels;
    if (audioOverlap_.size() != overlapSize)
    {
        audioOverlap_.resize(overlapSize, 0.0f);
    }

    // Calculate band boundaries scaled to this sample rate
    std::vector<size_t> bandBins;
    bandBins.push_back(0);
    for (size_t i = 1; i < 26; i++)
    {
        size_t bin = static_cast<size_t>(binkAudioBands[i]) * halfFrameLen / 22050;
        if (bin >= halfFrameLen)
        {
            bin = halfFrameLen;
            bandBins.push_back(bin);
            break;
        }
        bandBins.push_back(bin);
    }
    size_t numBands = bandBins.size() - 1;

    // Allocate output and working buffers
    outAudio.samples.resize(sampleCount * trackInfo.channels);
    std::vector<float> coeffs(frameLen);
    std::vector<float> window(frameLen);

    // Pre-compute window
    for (size_t i = 0; i < frameLen; i++)
    {
        window[i] = getWindow(i, frameLen);
    }

    size_t outPos = 0;
    size_t remaining = sampleCount * trackInfo.channels;

    auto get_float = [&bits]() -> float {
        int power = bits.readBits(5);
        float f = std::ldexp(static_cast<float>(bits.readBits(23)), power - 23);
        if (bits.readBit()) f = -f;
        return f;
    };
    
    static const uint8_t rle_length_tab[16] = {
        2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64
    };

    while (remaining > 0 && !bits.atEnd())
    {
        if (trackInfo.isDCT) bits.skipBits(2); // Unused for RDFT, skip 2 bits for DCT
        
        for (uint16_t ch = 0; ch < trackInfo.channels; ch++)
        {
            if (bits.atEnd()) break;

            std::fill(coeffs.begin(), coeffs.end(), 0.0f);
            
            // Read first two coefficients explicitly
            coeffs[0] = get_float() * (trackInfo.isDCT ? (1.0f / frameLen) : (2.0f / (std::sqrt(static_cast<float>(frameLen)) * 32768.0f)));
            coeffs[1] = get_float() * (trackInfo.isDCT ? (1.0f / frameLen) : (2.0f / (std::sqrt(static_cast<float>(frameLen)) * 32768.0f)));

            std::vector<float> quant(numBands);
            for (size_t i = 0; i < numBands; i++)
            {
                uint32_t q = bits.readBits(8);
                quant[i] = binkQuantTable[std::min(q, 95u)];
            }

            size_t k = 0;
            float q = quant[0];
            
            size_t i = 2;
            while (i < frameLen)
            {
                size_t j = i + 8;
                if (bits.readBit()) {
                    uint32_t v = bits.readBits(4);
                    j = i + rle_length_tab[v] * 8;
                }
                
                j = std::min(j, frameLen);
                
                uint32_t w = bits.readBits(4);
                if (w == 0) {
                    std::fill(coeffs.begin() + i, coeffs.begin() + j, 0.0f);
                    i = j;
                    while (k < numBands && bandBins[k] < i) q = quant[k++];
                } else {
                    while (i < j) {
                        while (k < numBands && bandBins[k] <= i) q = quant[k++];
                        uint32_t coeff = bits.readBits(w);
                        if (coeff) {
                            if (bits.readBit()) coeffs[i] = -q * coeff;
                            else coeffs[i] = q * coeff;
                        } else {
                            coeffs[i] = 0.0f;
                        }
                        i++;
                    }
                }
            }

            if (trackInfo.isDCT) dct(coeffs.data(), frameLen, true);
            else rdft(coeffs.data(), frameLen, true);

            size_t overlapOffset = ch * overlapLen;
            for (size_t idx = 0; idx < frameLen && outPos + idx < remaining; idx++)
            {
                float sample = coeffs[idx] * window[idx];
                if (idx < overlapLen) sample += audioOverlap_[overlapOffset + idx];
                
                float scaled = sample * 32767.0f;
                scaled = std::clamp(scaled, -32768.0f, 32767.0f);

                size_t outIndex = outPos + idx * trackInfo.channels + ch;
                if (outIndex < outAudio.samples.size()) outAudio.samples[outIndex] = static_cast<int16_t>(scaled);
            }

            for (size_t idx = 0; idx < overlapLen; idx++)
            {
                size_t srcIdx = frameLen - overlapLen + idx;
                audioOverlap_[overlapOffset + idx] = coeffs[srcIdx] * window[srcIdx];
            }
        }

        outPos += (frameLen - overlapLen) * trackInfo.channels;
        if (outPos >= remaining) break;
    }

    // Fill any remaining with silence
    for (size_t i = outPos; i < outAudio.samples.size(); i++)
    {
        outAudio.samples[i] = 0;
    }

    return true;
}

void BinkDecoder::rdft(float* data, size_t n, bool inverse)
{
    util::FFT::rdft(std::span<float>(data, n), inverse);
}

void BinkDecoder::dct(float* data, size_t n, bool inverse)
{
    util::FFT::dct(std::span<float>(data, n), inverse);
}

} // namespace runeharbor::media
