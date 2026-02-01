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
 * Contains map files (.blv, .odm, .dlv, .ddm):
 * - .blv = Indoor maps (Building Level Volume)
 * - .odm = Outdoor maps
 * - .dlv = Dungeon level state/save data
 * - .ddm = Dungeon dynamic data
 *
 * Directory entry format (32 bytes):
 * - 12 bytes: filename (null-padded)
 * - 4 bytes: attributes/flags
 * - 4 bytes: data offset (absolute position in file)
 * - 4 bytes: unknown (possibly decompressed size)
 * - 4 bytes: compressed size
 * - 4 bytes: reserved (zeros)
 */

#pragma pack(push, 1)
struct GameLODHeader
{
    char magic[4];  // "LOD\0"
    char gameId[4]; // "Game" for GAMES.LOD
    uint8_t unknown[248];
};

struct GameLODDirectoryEntry
{
    char name[8];         // Filename (null-padded, 8 bytes)
    uint32_t attributes;  // Attributes/flags
    uint32_t unknown1;    // Unknown (same for all BLV: 0x610200)
    uint32_t offset;      // Data offset relative to data section start
    uint32_t size;        // File size in bytes
    uint32_t reserved[2]; // Reserved (zeros)
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
    std::optional<GameLODDirectoryEntry> getFileInfo(const std::string& filename) const;

  private:
    bool readHeader();
    bool readDirectory();
    std::string buildFilename(const GameLODDirectoryEntry& entry) const;

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    bool opened = false;

    std::vector<GameLODDirectoryEntry> entries;
    std::streamoff dataSectionStart = 0;
};

} // namespace runeharbor::formats
