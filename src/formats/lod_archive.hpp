// SPDX-License-Identifier: MIT
#pragma once

#include "../util/ilogger.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace runeharbor::formats
{

#pragma pack(push, 1)
struct LODHeader
{
    char magic[4];      // "LOD\0"
    char gameId[4];     // "MMVI", "MMVII", or "MMVIII"
    uint8_t reserved[248]; // Reserved header space
};

struct LODDirectoryEntry
{
    char filename[16];  // Null-terminated filename (uppercase)
    uint32_t offset;    // Absolute offset from start of file
    uint32_t size;      // Size of file in bytes
    uint32_t unknown1;  // Possibly compression flags or timestamp
    uint32_t unknown2;  // Possibly additional metadata
};
#pragma pack(pop)

class LODArchive
{
public:
    explicit LODArchive(util::ILogger& logger);
    ~LODArchive();

    // Prevent copying and moving (contains reference member)
    LODArchive(const LODArchive&) = delete;
    LODArchive& operator=(const LODArchive&) = delete;
    LODArchive(LODArchive&&) = delete;
    LODArchive& operator=(LODArchive&&) = delete;

    bool open(const std::filesystem::path& path);
    void close();
    bool isOpen() const;

    std::vector<std::string> listFiles() const;
    std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);

private:
    bool readHeader();
    bool readDirectory();
    std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& data);
    bool isZlibCompressed(const std::vector<uint8_t>& data) const;

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    std::vector<LODDirectoryEntry> entries;
    std::streamoff dataSectionStart = 0;
    bool opened = false;
};

} // namespace runeharbor::formats
