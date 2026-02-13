// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fstream>

namespace runeharbor::media
{

enum class VideoFormat
{
    Unknown,
    Smacker, // .smk - SMK2 magic
    Bink     // .bik - BIKf/BIKi magic
};

struct VIDEntry
{
    std::string name;
    uint32_t offset;
    uint32_t size; // Calculated from next entry or file end
    VideoFormat format;
};

/**
 * VID Archive - Container for Bink and Smacker video files
 *
 * Format:
 *   4 bytes: uint32_t entry_count
 *   For each entry (44 bytes):
 *     40 bytes: filename (null-terminated)
 *     4 bytes:  uint32_t offset to video data
 */
class VIDArchive
{
  public:
    VIDArchive() = default;
    ~VIDArchive();

    VIDArchive(const VIDArchive&) = delete;
    VIDArchive& operator=(const VIDArchive&) = delete;
    VIDArchive(VIDArchive&&) noexcept;
    VIDArchive& operator=(VIDArchive&&) noexcept;

    bool open(const std::filesystem::path& path);
    void close();
    bool isOpen() const { return file_.is_open(); }

    size_t entryCount() const { return entries_.size(); }
    const std::vector<VIDEntry>& entries() const { return entries_; }

    // Find entry by name (case-insensitive)
    std::optional<size_t> findEntry(const std::string& name) const;

    // Get entry by index
    const VIDEntry* getEntry(size_t index) const;

    // Read video data into buffer
    std::vector<uint8_t> readVideoData(size_t index);
    std::vector<uint8_t> readVideoData(const std::string& name);

    // Get file stream positioned at video start (for streaming)
    bool seekToVideo(size_t index);
    std::ifstream& stream() { return file_; }

  private:
    std::filesystem::path path_;
    std::ifstream file_;
    std::vector<VIDEntry> entries_;
    uint64_t fileSize_ = 0;

    VideoFormat detectFormat(uint32_t offset);
    static std::string toLower(const std::string& str);
};

} // namespace runeharbor::media
