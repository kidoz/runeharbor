// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fstream>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

/**
 * SPRITES.LOD Archive Format
 *
 * Similar to BITMAPS.LOD but with different directory entry format:
 * - 16-byte sprite names (vs 4-byte + extension marker)
 * - Sprite-specific header with animation frame data
 * - zlib compressed sprite data
 */

#pragma pack(push, 1)
struct SpriteLODHeader
{
    char magic[4];  // "LOD\0"
    char gameId[4]; // "MMVI" for MM7
    uint8_t unknown[248];
};

struct SpriteLODDirectoryEntry
{
    char name[16];     // Sprite name (up to 16 chars, null-padded)
    uint32_t offset;   // Offset relative to the archive's offset delta
    uint32_t size;     // Total sprite block size
    uint32_t unknown1; // Archive metadata
    uint32_t unknown2; // Archive metadata
};

struct SpriteFileHeader
{
    char name[8];              // Sprite name (8 bytes, null-padded)
    uint32_t dataOffset;       // Sprite data pointer/offset metadata
    uint32_t compressedSize;   // Compressed pixel data size
    uint16_t width;            // Sprite width
    uint16_t height;           // Sprite height
    uint16_t paletteId;        // Palette ID
    uint16_t unknown1;         // Unknown
    uint16_t centerX;          // Sprite center X
    uint16_t centerY;          // Sprite center Y
    uint32_t decompressedSize; // Decompressed pixel data size
    // Followed by line info table (height * 8 bytes)
    // Then zlib compressed pixel data
};
#pragma pack(pop)

static_assert(sizeof(SpriteLODHeader) == 256, "SpriteLODHeader must be 256 bytes");
static_assert(sizeof(SpriteLODDirectoryEntry) == 32, "SpriteLODDirectoryEntry must be 32 bytes");

class SpriteLODArchive
{
  public:
    explicit SpriteLODArchive(util::ILogger& logger);
    ~SpriteLODArchive();

    // Non-copyable and non-movable
    SpriteLODArchive(const SpriteLODArchive&) = delete;
    SpriteLODArchive& operator=(const SpriteLODArchive&) = delete;
    SpriteLODArchive(SpriteLODArchive&&) = delete;
    SpriteLODArchive& operator=(SpriteLODArchive&&) = delete;

    bool open(const std::filesystem::path& path);
    void close();
    bool isOpen() const;

    std::vector<std::string> listFiles() const;
    std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);
    std::optional<SpriteFileHeader> getFileInfo(const std::string& filename);

  private:
    bool readHeader();
    bool readDirectory();
    std::string buildFilename(const SpriteLODDirectoryEntry& entry) const;
    std::streamoff calculateDataOffset(size_t entryIndex) const;

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    bool opened = false;

    std::vector<SpriteLODDirectoryEntry> entries;
    std::streamoff dataSectionStart = 0;
    std::streamoff offsetDelta = 0;
};

} // namespace runeharbor::formats
