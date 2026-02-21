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
 * GAMES.LOD Archive Format
 *
 * Standard LOD container holding map files (.blv, .odm, .dlv, .ddm).
 * Uses the same 256-byte header and 32-byte directory entry format as
 * all other LOD archives (Events.lod, BITMAPS.LOD, etc.).
 *
 * Directory entry (32 bytes):
 *   0x00  name[16]          Null-terminated filename
 *   0x10  offset(u32)       Absolute byte offset to data block
 *   0x14  size(u32)         Total data block size (8-byte header + payload)
 *   0x18  decompressedSize  Uncompressed size (0 = not compressed)
 *   0x1C  reserved(u32)     Padding / flags
 *
 * Each data block starts with an 8-byte metadata header:
 *   [4 bytes: uncompressed size][4 bytes: flags]
 * followed by the payload (zlib-compressed or raw).
 */

#pragma pack(push, 1)
struct GameLODHeader
{
    char magic[4];          // 0x000: "LOD\0"
    char gameId[4];         // 0x004: "MMVII" etc.
    char description[80];   // 0x008: human-readable
    char chapterName[80];   // 0x058: default chapter name
    uint32_t fileSize;      // 0x0A8: total file size (may be 0)
    uint32_t dataStart;     // 0x0AC: byte offset where data section begins
    uint32_t numDirEntries; // 0x0B0: number of top-level directory entries
    uint8_t reserved[76];   // 0x0B4: padding
};

struct GameLODDirectoryEntry
{
    char name[16];             // 0x00: null-terminated filename
    uint32_t offset;           // 0x10: absolute byte offset to data block
    uint32_t size;             // 0x14: total data block size
    uint32_t decompressedSize; // 0x18: uncompressed size (0 = raw)
    uint32_t reserved;         // 0x1C: padding / flags
};
#pragma pack(pop)

static_assert(sizeof(GameLODHeader) == 256, "GameLODHeader must be 256 bytes");
static_assert(sizeof(GameLODDirectoryEntry) == 32, "GameLODDirectoryEntry must be 32 bytes");

class GameLODArchive
{
  public:
    explicit GameLODArchive(util::ILogger& logger);
    ~GameLODArchive();

    // Non-copyable and non-movable
    GameLODArchive(const GameLODArchive&) = delete;
    GameLODArchive& operator=(const GameLODArchive&) = delete;
    GameLODArchive(GameLODArchive&&) = delete;
    GameLODArchive& operator=(GameLODArchive&&) = delete;

    bool open(const std::filesystem::path& path);
    void close();
    bool isOpen() const;

    std::vector<std::string> listFiles() const;
    std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);

  private:
    bool readHeader();
    bool readDirectory();

    static std::string entryName(const GameLODDirectoryEntry& entry);

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    bool opened = false;

    GameLODHeader header_{};
    std::vector<GameLODDirectoryEntry> entries;
};

} // namespace runeharbor::formats
