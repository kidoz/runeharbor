// SPDX-License-Identifier: MIT
#include "vid_archive.hpp"

#include <algorithm>

#include <cctype>
#include <cstring>

namespace runeharbor::media
{

VidArchive::~VidArchive()
{
    close();
}

VidArchive::VidArchive(VidArchive&& other) noexcept
    : path_(std::move(other.path_)), file_(std::move(other.file_)),
      entries_(std::move(other.entries_)), fileSize_(other.fileSize_)
{
    other.fileSize_ = 0;
}

VidArchive& VidArchive::operator=(VidArchive&& other) noexcept
{
    if (this != &other)
    {
        close();
        path_ = std::move(other.path_);
        file_ = std::move(other.file_);
        entries_ = std::move(other.entries_);
        fileSize_ = other.fileSize_;
        other.fileSize_ = 0;
    }
    return *this;
}

bool VidArchive::open(const std::filesystem::path& path)
{
    close();

    file_.open(path, std::ios::binary);
    if (!file_.is_open())
    {
        return false;
    }

    path_ = path;

    // Get file size
    file_.seekg(0, std::ios::end);
    fileSize_ = static_cast<uint64_t>(file_.tellg());
    file_.seekg(0, std::ios::beg);

    // Read entry count
    uint32_t count = 0;
    file_.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!file_ || count == 0 || count > 10000)
    {
        close();
        return false;
    }

    // Read entries
    entries_.reserve(count);
    for (uint32_t i = 0; i < count; i++)
    {
        char nameBuffer[40] = {0};
        uint32_t offset = 0;

        file_.read(nameBuffer, 40);
        file_.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        if (!file_)
        {
            close();
            return false;
        }

        VidEntry entry;
        entry.name = nameBuffer;
        entry.offset = offset;
        entry.size = 0; // Will be calculated below
        entry.format = VideoFormat::Unknown;

        entries_.push_back(std::move(entry));
    }

    // Calculate sizes based on next entry offset or file end
    for (size_t i = 0; i < entries_.size(); i++)
    {
        uint64_t nextOffset;
        if (i + 1 < entries_.size())
        {
            nextOffset = entries_[i + 1].offset;
        }
        else
        {
            nextOffset = fileSize_;
        }

        if (nextOffset > entries_[i].offset)
        {
            entries_[i].size = static_cast<uint32_t>(nextOffset - entries_[i].offset);
        }

        // Detect video format
        entries_[i].format = detectFormat(entries_[i].offset);
    }

    return true;
}

void VidArchive::close()
{
    if (file_.is_open())
    {
        file_.close();
    }
    entries_.clear();
    fileSize_ = 0;
    path_.clear();
}

VideoFormat VidArchive::detectFormat(uint32_t offset)
{
    if (!file_.is_open())
    {
        return VideoFormat::Unknown;
    }

    auto currentPos = file_.tellg();
    file_.seekg(offset);

    char magic[4] = {0};
    file_.read(magic, 4);

    file_.seekg(currentPos);

    if (!file_)
    {
        return VideoFormat::Unknown;
    }

    // Check for Smacker format (SMK2 or SMK4)
    if (std::memcmp(magic, "SMK2", 4) == 0 || std::memcmp(magic, "SMK4", 4) == 0)
    {
        return VideoFormat::Smacker;
    }

    // Check for Bink format (BIKf, BIKi, etc.)
    if (magic[0] == 'B' && magic[1] == 'I' && magic[2] == 'K')
    {
        return VideoFormat::Bink;
    }

    return VideoFormat::Unknown;
}

std::string VidArchive::toLower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::optional<size_t> VidArchive::findEntry(const std::string& name) const
{
    std::string lowerName = toLower(name);

    for (size_t i = 0; i < entries_.size(); i++)
    {
        if (toLower(entries_[i].name) == lowerName)
        {
            return i;
        }
    }

    return std::nullopt;
}

const VidEntry* VidArchive::getEntry(size_t index) const
{
    if (index >= entries_.size())
    {
        return nullptr;
    }
    return &entries_[index];
}

std::vector<uint8_t> VidArchive::readVideoData(size_t index)
{
    if (index >= entries_.size() || !file_.is_open())
    {
        return {};
    }

    const auto& entry = entries_[index];
    if (entry.size == 0)
    {
        return {};
    }

    file_.seekg(entry.offset);

    std::vector<uint8_t> data(entry.size);
    file_.read(reinterpret_cast<char*>(data.data()), entry.size);

    if (!file_)
    {
        return {};
    }

    return data;
}

std::vector<uint8_t> VidArchive::readVideoData(const std::string& name)
{
    auto index = findEntry(name);
    if (!index)
    {
        return {};
    }
    return readVideoData(*index);
}

bool VidArchive::seekToVideo(size_t index)
{
    if (index >= entries_.size() || !file_.is_open())
    {
        return false;
    }

    file_.seekg(entries_[index].offset);
    return file_.good();
}

} // namespace runeharbor::media
