// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fstream>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

/**
 * Game/archive LOD format used for:
 * - map container files (e.g. GAMES.LOD with "maps" chapter + sequential chunks)
 * - save archives (save###.mm7 with standard LOD directory and per-file data headers)
 *
 * Directory entry (32 bytes):
 *   0x00  name[16]          Null-terminated filename
 *   0x10  offset(u32)       Entry offset (semantics depend on archive mode)
 *   0x14  size(u32)         Stored data size metadata
 *   0x18  decompressedSize  Uncompressed size hint
 *   0x1C  reserved(u32)     Flags / counters (mode-dependent)
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
    enum class DirectoryMode
    {
        Standard,
        MapChapter,
    };

    bool readHeader();
    bool readDirectory();
    bool readStandardDirectory();
    bool readMapChapterDirectory();

    static std::string entryName(const GameLODDirectoryEntry& entry);
    static bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs);

    std::optional<size_t> findEntryIndex(const std::string& filename) const;
    std::optional<std::vector<uint8_t>> extractStandardFile(const GameLODDirectoryEntry& entry,
                                                            const std::string& filename);
    std::optional<std::vector<uint8_t>> extractMapChapterFile(size_t entryIndex,
                                                              const std::string& filename);

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    bool opened = false;

    GameLODHeader header_{};
    std::vector<GameLODDirectoryEntry> entries;
    DirectoryMode directoryMode = DirectoryMode::Standard;
    std::streamoff mapDataSectionStart = 0;
};

} // namespace runeharbor::formats
