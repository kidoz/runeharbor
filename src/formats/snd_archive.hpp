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
 * SND Archive Format (Audio.snd)
 * 
 * Used in MM7/8 for audio assets.
 * Structure:
 * - Header: uint32_t entryCount
 * - Directory: Array of SndEntry[entryCount]
 * - Data: File data (often zlib compressed)
 */

#pragma pack(push, 1)
struct SndEntryRaw
{
    char name[40];             // Null-terminated name (often "name\0ext\0")
    uint32_t offset;           // Absolute offset in file
    uint32_t compressedSize;   // Size in archive
    uint32_t uncompressedSize; // Size after decompression (if compressed)
};
#pragma pack(pop)

struct SndEntry
{
    std::string name;
    uint32_t offset;
    uint32_t compressedSize;
    uint32_t uncompressedSize;

    bool isCompressed() const { return compressedSize != uncompressedSize; }
};

class SndArchive
{
  public:
    explicit SndArchive(util::ILogger& logger);
    ~SndArchive();

    // Prevent copying and moving
    SndArchive(const SndArchive&) = delete;
    SndArchive& operator=(const SndArchive&) = delete;
    SndArchive(SndArchive&&) = delete;
    SndArchive& operator=(SndArchive&&) = delete;

    bool open(const std::filesystem::path& path);
    void close();
    bool isOpen() const;

    std::vector<std::string> listFiles() const;
    std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);

  private:
    std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& data, uint32_t uncompressedSize);

    util::ILogger& logger;
    std::ifstream file;
    std::filesystem::path archivePath;
    std::vector<SndEntry> entries;
    bool opened = false;
};

} // namespace runeharbor::formats
