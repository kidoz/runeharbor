// SPDX-License-Identifier: MIT
#include "snd_archive.hpp"

#include <algorithm>
#include <format>
#include <cstring>
#include <zlib.h>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

SndArchive::SndArchive(util::ILogger& logger) : logger(logger) {}

SndArchive::~SndArchive()
{
    close();
}

bool SndArchive::open(const std::filesystem::path& path)
{
    close();

    archivePath = path;
    file.open(path, std::ios::binary);

    if (!file.is_open())
    {
        logger.error(std::format("Failed to open SND archive: {}", path.string()));
        return false;
    }

    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    if (!file || count == 0 || count > 20000)
    {
        logger.error("Invalid SND archive: count is 0 or too large");
        return false;
    }

    entries.reserve(count);
    for (uint32_t i = 0; i < count; i++)
    {
        SndEntryRaw raw;
        file.read(reinterpret_cast<char*>(&raw), sizeof(SndEntryRaw));
        if (!file) break;

        SndEntry entry;
        entry.offset = raw.offset;
        entry.compressedSize = raw.compressedSize;
        entry.uncompressedSize = raw.uncompressedSize;

        // Extract name and handle potential extension
        std::string baseName = raw.name;
        std::string ext;
        
        // Look for extension after the first null
        size_t baseLen = baseName.length();
        if (baseLen + 1 < 40 && raw.name[baseLen + 1] != '\0')
        {
            ext = &raw.name[baseLen + 1];
        }

        if (!ext.empty())
        {
            entry.name = baseName + "." + ext;
        }
        else
        {
            entry.name = baseName;
        }

        entries.push_back(std::move(entry));
    }

    opened = true;
    logger.info(std::format("Opened SND archive '{}' with {} entries", path.string(), entries.size()));
    return true;
}

void SndArchive::close()
{
    if (file.is_open())
    {
        file.close();
    }
    entries.clear();
    opened = false;
}

bool SndArchive::isOpen() const
{
    return opened;
}

std::vector<std::string> SndArchive::listFiles() const
{
    std::vector<std::string> names;
    names.reserve(entries.size());
    for (const auto& entry : entries)
    {
        names.push_back(entry.name);
    }
    return names;
}

std::optional<std::vector<uint8_t>> SndArchive::extractFile(const std::string& filename)
{
    if (!opened) return std::nullopt;

    auto it = std::find_if(entries.begin(), entries.end(), [&](const SndEntry& e) {
        return util::equalsIgnoreCase(e.name, filename);
    });

    // Also try without extension if the filename has none
    if (it == entries.end() && filename.find('.') == std::string::npos)
    {
        it = std::find_if(entries.begin(), entries.end(), [&](const SndEntry& e) {
            std::string_view base(e.name.data(), e.name.find('.'));
            return util::equalsIgnoreCase(base, filename);
        });
    }

    if (it == entries.end())
    {
        logger.error(std::format("File not found in SND: {}", filename));
        return std::nullopt;
    }

    file.seekg(it->offset);
    std::vector<uint8_t> data(it->compressedSize);
    file.read(reinterpret_cast<char*>(data.data()), it->compressedSize);

    if (!file)
    {
        logger.error(std::format("Failed to read data for {}", it->name));
        return std::nullopt;
    }

    if (it->isCompressed())
    {
        return decompressZlib(data, it->uncompressedSize);
    }

    return data;
}

std::vector<uint8_t> SndArchive::decompressZlib(const std::vector<uint8_t>& data, uint32_t uncompressedSize)
{
    if (data.empty()) return {};

    std::vector<uint8_t> result(uncompressedSize);
    uLongf destLen = uncompressedSize;
    
    // We use uncompress which handles the zlib header
    int res = uncompress(result.data(), &destLen, data.data(), data.size());

    if (res != Z_OK)
    {
        logger.error(std::format("zlib decompression failed: {}", res));
        return {};
    }

    if (destLen != uncompressedSize)
    {
        logger.warning(std::format("Decompressed size mismatch: expected {}, got {}", uncompressedSize, destLen));
        result.resize(destLen);
    }

    return result;
}

} // namespace runeharbor::formats
