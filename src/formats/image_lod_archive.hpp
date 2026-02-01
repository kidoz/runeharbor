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
 * Format differences from Events.lod:
 * - Directory entries use 4-byte short names instead of 16-byte names
 * - Extensions marked with UTF-16 "LIB." (8 bytes) for custom format files
 * - Files use custom compression (NOT zlib)
 * - Two file types: Type 1 (with LIB. marker), Type 2 (without marker)
 *
 * Current implementation: Directory parsing and raw file extraction
 * TODO: Implement custom compression/decompression
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
    char shortName[4];       // Short filename (up to 4 chars, null-padded)
    uint32_t sortKey;        // Sort key (not used for offset calculation!)
    char extensionMarker[8]; // "LIB." in UTF-16 LE (0x4C 0x00 0x49 0x00 0x42 0x00 0x2E 0x00)
                             // or zeros for Type 2 files
    uint32_t size1;          // Unknown/unused
    uint32_t size2;          // COMPRESSED size INCLUDING 48-byte header
    uint32_t reserved[2];    // Reserved (zeros)
};

struct ImageFileHeader
{
    char name[16];             // File name (offset 0-15)
    uint32_t unknown1;         // Unknown field (offset 16-19)
    uint32_t unknown2;         // Unknown field (offset 20-23)
    uint16_t width;            // Image width (offset 24-25, power of 2)
    uint16_t height;           // Image height (offset 26-27, power of 2)
    uint16_t widthLn2;         // log2(width) (offset 28-29)
    uint16_t heightLn2;        // log2(height) (offset 30-31)
    uint32_t palette1;         // Palette ID 1 (offset 32-35)
    uint32_t palette2;         // Palette ID 2 (offset 36-39)
    uint32_t decompressedSize; // Decompressed size (offset 40-43, includes mipmaps!)
    uint32_t textureSize;      // Unknown field (offset 44-47)
};
#pragma pack(pop)

static_assert(sizeof(ImageLODHeader) == 256, "ImageLODHeader must be 256 bytes");
static_assert(sizeof(ImageLODDirectoryEntry) == 32, "ImageLODDirectoryEntry must be 32 bytes");
static_assert(sizeof(ImageFileHeader) == 48, "ImageFileHeader must be 48 bytes");

enum class ImageEntryType
{
    CustomFormat,  // Type 1: Has "LIB." marker, custom compressed format
    ExternalFormat // Type 2: No marker, PCX or other external format
};

class ImageLODArchive
{
  public:
    explicit ImageLODArchive(util::ILogger& logger);
    ~ImageLODArchive();

    // Non-copyable and non-movable (manages file handle and has logger reference)
    ImageLODArchive(const ImageLODArchive&) = delete;
    ImageLODArchive& operator=(const ImageLODArchive&) = delete;
    ImageLODArchive(ImageLODArchive&&) = delete;
    ImageLODArchive& operator=(ImageLODArchive&&) = delete;

    /**
     * Open an image LOD archive for reading
     * @param path Path to the .lod file
     * @return true if opened successfully
     */
    bool open(const std::filesystem::path& path);

    /**
     * Close the archive and release resources
     */
    void close();

    /**
     * Check if archive is currently open
     */
    bool isOpen() const;

    /**
     * List all files in the archive
     * @return Vector of filenames
     */
    std::vector<std::string> listFiles() const;

    /**
     * Extract a file from the archive
     * @param filename Name of the file to extract
     * @return File data, or nullopt if not found or extraction failed
     *
     * Note: Current implementation returns RAW data without decompression.
     * Files with custom compression (Type 1) will not be usable until
     * decompression is implemented.
     */
    std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);

    /**
     * Get file header information (dimensions, palette, etc.)
     * @param filename Name of the file
     * @return File header, or nullopt if not found or read failed
     */
    std::optional<ImageFileHeader> getFileInfo(const std::string& filename);

  private:
    /**
     * Read and validate LOD header
     */
    bool readHeader();

    /**
     * Read directory entries until null terminator
     */
    bool readDirectory();

    /**
     * Detect entry type based on extension marker
     * @param entry Directory entry to check
     * @return ImageEntryType indicating file format
     */
    ImageEntryType detectEntryType(const ImageLODDirectoryEntry& entry) const;

    /**
     * Build full filename from directory entry
     * @param entry Directory entry
     * @return Filename string (includes extension if present)
     */
    std::string buildFilename(size_t index) const;

    /**
     * Calculate file offset in data section
     * @param entry Directory entry for target file
     * @return Offset in bytes from start of file
     *
     * Note: Uses sort key to determine file order, then sums preceding file sizes
     */
    std::streamoff calculateDataOffset(const ImageLODDirectoryEntry& entry) const;

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    std::vector<ImageLODDirectoryEntry> entries;
    std::vector<std::string> resolvedNames;
    std::streamoff dataSectionStart = 0;
    bool opened = false;

    std::string buildShortFilename(const ImageLODDirectoryEntry& entry,
                                   ImageEntryType entryType) const;
    bool resolveEntryNames();
    std::optional<std::string> readExternalName(const ImageLODDirectoryEntry& entry);
};

} // namespace runeharbor::formats
