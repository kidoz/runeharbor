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
 * Parser for MM7 image LOD archives (BITMAPS.LOD, ICONS.LOD, SPRITES.LOD)
 *
 * Two archive variants exist:
 *
 * 1. Mixed archives (BITMAPS.LOD): Some entries have a UTF-16 "LIB." marker
 *    indicating custom compressed format with 48-byte ImageFileHeader + zlib data.
 *    Other entries are External format stored with the same layout.
 *
 * 2. External-only archives (ICONS.LOD): No entries have the "LIB." marker.
 *    Entry offsets are sort keys (not absolute file positions).
 *    Data is stored sequentially: 8-byte prefix (decompressedSize + flags) + zlib data.
 *    Names span the full 16 bytes of the directory entry name field.
 */

#pragma pack(push, 1)
struct ImageLODHeader
{
    char magic[4];  // "LOD\0"
    char gameId[4]; // "MMVI" for MM7
    uint8_t unknown[248];
};

struct ImageLODDirectoryEntry
{
    char name[8]; // Filename (first 8 bytes, null-terminated for short names)
    char extensionMarker[8];
    // For Custom format (LIB.): UTF-16 LE "LIB." = 4C 00 49 00 42 00 2E 00
    // For External format: continuation of name (e.g., "Title.pcx" spans name+marker)
    uint32_t offset;      // Sort key / sequential position (NOT absolute file offset)
    uint32_t size;        // Total data size including any prefix (8-byte or 48-byte)
    uint32_t reserved[2]; // Reserved (zeros)
};

struct ImageFileHeader
{
    char name[16];             // File name (offset 0-15)
    uint32_t dataSize;         // Uncompressed pixel size: width*height (offset 16-19)
    uint32_t compressedSize;   // Compressed data size (offset 20-23)
    uint16_t width;            // Image width (offset 24-25)
    uint16_t height;           // Image height (offset 26-27)
    int16_t widthLn2;          // log2(width) (offset 28-29)
    int16_t heightLn2;         // log2(height) (offset 30-31)
    int16_t widthMinus1;       // width - 1 (offset 32-33)
    int16_t heightMinus1;      // height - 1 (offset 34-35)
    int16_t paletteId;         // Primary palette index (offset 36-37)
    int16_t paletteId2;        // Secondary palette index (offset 38-39)
    uint32_t decompressedSize; // Total decompressed size with mipmaps (offset 40-43)
    uint32_t flags;            // Texture flags (offset 44-47)
};
#pragma pack(pop)

static_assert(sizeof(ImageLODHeader) == 256, "ImageLODHeader must be 256 bytes");
static_assert(sizeof(ImageLODDirectoryEntry) == 32, "ImageLODDirectoryEntry must be 32 bytes");
static_assert(sizeof(ImageFileHeader) == 48, "ImageFileHeader must be 48 bytes");

enum class ImageEntryType
{
    CustomFormat,  // Has "LIB." marker, 48-byte ImageFileHeader + zlib data
    ExternalFormat // No marker, part of sequential data with 8-byte prefix + zlib
};

class ImageLODArchive
{
  public:
    explicit ImageLODArchive(util::ILogger& logger);
    ~ImageLODArchive();

    ImageLODArchive(const ImageLODArchive&) = delete;
    ImageLODArchive& operator=(const ImageLODArchive&) = delete;
    ImageLODArchive(ImageLODArchive&&) = delete;
    ImageLODArchive& operator=(ImageLODArchive&&) = delete;

    bool open(const std::filesystem::path& path);
    void close();
    bool isOpen() const;

    std::vector<std::string> listFiles() const;
    std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);
    std::optional<ImageFileHeader> getFileInfo(const std::string& filename);

    /// Extract the 768-byte embedded palette from an image entry.
    /// Layout: ImageFileHeader(48B) + compressed_pixels(compressedSize) + palette(768B)
    std::optional<std::vector<uint8_t>> extractPalette(const std::string& filename);

  private:
    bool readHeader();
    bool readDirectory();
    ImageEntryType detectEntryType(const ImageLODDirectoryEntry& entry) const;
    std::string buildFilename(size_t index) const;
    std::streamoff calculateDataOffset(const ImageLODDirectoryEntry& entry) const;

    /// Extract from External-only archive (ICONS.LOD pattern):
    /// offset + delta → 8-byte prefix + zlib compressed data
    std::optional<std::vector<uint8_t>> extractExternal(const ImageLODDirectoryEntry& entry,
                                                        const std::string& filename);

    /// Extract from mixed archive Custom format (BITMAPS.LOD pattern):
    /// sequential 48-byte ImageFileHeader + zlib compressed data
    std::optional<std::vector<uint8_t>> extractCustom(const ImageLODDirectoryEntry& entry,
                                                      const std::string& filename);

    bool resolveEntryNames();

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    std::vector<ImageLODDirectoryEntry> entries;
    std::vector<std::string> resolvedNames;
    std::streamoff dataSectionStart = 0;
    int64_t offsetDelta = 0;   // Delta to add to entry.offset for actual file position
    bool externalOnly = false; // True if no entries have LIB. marker (ICONS.LOD pattern)
    bool opened = false;
};

} // namespace runeharbor::formats