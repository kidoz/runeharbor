// SPDX-License-Identifier: MIT
#include "game_lod_archive.hpp"

#include <format>

#include <cctype>
#include <cstring>
#include <zlib.h>

namespace runeharbor::formats
{

GameLODArchive::GameLODArchive(util::ILogger& logger) : logger(logger) {}

GameLODArchive::~GameLODArchive()
{
    close();
}

bool GameLODArchive::open(const std::filesystem::path& path)
{
    if (opened)
    {
        close();
    }

    archivePath = path;
    file.open(path, std::ios::binary);

    if (!file.is_open())
    {
        logger.error(std::format("Failed to open game archive: {}", path.string()));
        return false;
    }

    if (!readHeader())
    {
        close();
        return false;
    }

    if (!readDirectory())
    {
        close();
        return false;
    }

    opened = true;
    logger.info(std::format("Successfully loaded game archive with {} files", entries.size()));
    return true;
}

void GameLODArchive::close()
{
    if (file.is_open())
    {
        file.close();
        logger.debug(std::format("Closed game archive: {}", archivePath.string()));
    }
    entries.clear();
    opened = false;
}

bool GameLODArchive::isOpen() const
{
    return opened;
}

bool GameLODArchive::readHeader()
{
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(&header_), sizeof(GameLODHeader));

    if (!file.good())
    {
        logger.error("Failed to read game LOD header");
        return false;
    }

    if (std::strncmp(header_.magic, "LOD", 3) != 0)
    {
        logger.error(std::format("Invalid LOD magic: expected 'LOD', got '{}'",
                                 std::string(header_.magic, 3)));
        return false;
    }

    logger.debug(std::format("Game LOD: dataStart=0x{:X}, numEntries={}", header_.dataStart,
                             header_.numDirEntries));
    return true;
}

bool GameLODArchive::readDirectory()
{
    // Directory starts right after the 256-byte header
    constexpr std::streamoff kDirOffset = 0x100;
    file.seekg(kDirOffset, std::ios::beg);

    // Use the entry count from the header; fall back to scanning if zero
    uint32_t count = header_.numDirEntries;
    if (count == 0 || count > 10000)
    {
        // Scan for null-terminated directory
        logger.debug("numDirEntries unreliable, scanning directory");
        count = 0;
        while (file.good() && count < 10000)
        {
            GameLODDirectoryEntry probe;
            file.read(reinterpret_cast<char*>(&probe), sizeof(probe));
            if (!file.good() || probe.name[0] == '\0')
            {
                break;
            }
            count++;
        }
        file.seekg(kDirOffset, std::ios::beg);
    }

    entries.resize(count);
    file.read(reinterpret_cast<char*>(entries.data()),
              static_cast<std::streamsize>(count * sizeof(GameLODDirectoryEntry)));

    if (!file.good())
    {
        logger.error("Failed to read game directory entries");
        entries.clear();
        return false;
    }

    // Remove entries with empty names (shouldn't happen, but be safe)
    std::erase_if(entries, [](const GameLODDirectoryEntry& e) { return e.name[0] == '\0'; });

    logger.debug(std::format("Read {} game directory entries", entries.size()));

    if (entries.empty())
    {
        logger.error("No files found in game archive directory");
        return false;
    }

    return true;
}

std::string GameLODArchive::entryName(const GameLODDirectoryEntry& entry)
{
    // Extract null-terminated name from 16-byte field
    size_t len = 0;
    while (len < sizeof(entry.name) && entry.name[len] != '\0')
    {
        len++;
    }
    return std::string(entry.name, len);
}

std::vector<std::string> GameLODArchive::listFiles() const
{
    std::vector<std::string> filenames;
    filenames.reserve(entries.size());

    for (const auto& entry : entries)
    {
        filenames.push_back(entryName(entry));
    }

    return filenames;
}

std::optional<std::vector<uint8_t>> GameLODArchive::extractFile(const std::string& filename)
{
    if (!opened)
    {
        logger.error("Archive not open");
        return std::nullopt;
    }

    // Case-insensitive search
    const GameLODDirectoryEntry* target = nullptr;
    for (const auto& entry : entries)
    {
        std::string name = entryName(entry);
        if (name.size() != filename.size())
        {
            continue;
        }
        bool match = true;
        for (size_t j = 0; j < name.size(); j++)
        {
            if (std::tolower(static_cast<unsigned char>(name[j])) !=
                std::tolower(static_cast<unsigned char>(filename[j])))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            target = &entry;
            break;
        }
    }

    if (!target)
    {
        logger.debug(std::format("File not found: {}", filename));
        return std::nullopt;
    }

    if (target->offset == 0 || target->size == 0)
    {
        logger.error(std::format("Invalid entry for '{}': offset=0x{:X}, size={}", filename,
                                 target->offset, target->size));
        return std::nullopt;
    }

    // Seek to the data block at the absolute offset
    file.seekg(static_cast<std::streamoff>(target->offset), std::ios::beg);
    if (!file.good())
    {
        logger.error(
            std::format("Failed to seek to offset 0x{:X} for '{}'", target->offset, filename));
        return std::nullopt;
    }

    // Check if the file is compressed based on directory entry
    // Uncompressed files in GAMES.LOD (decompressedSize == 0) typically do NOT have the 8-byte
    // header. Compressed files usually do.
    bool hasHeader = (target->decompressedSize > 0);

    std::vector<uint8_t> payload;
    uint32_t metaUncompressed = 0;

    if (hasHeader)
    {
        // Read 8-byte metadata header: [uncompressedSize:4][flags:4]
        uint32_t metaFlags = 0;
        file.read(reinterpret_cast<char*>(&metaUncompressed), 4);
        file.read(reinterpret_cast<char*>(&metaFlags), 4);
        if (!file.good())
        {
            logger.error(std::format("Failed to read data header for '{}'", filename));
            return std::nullopt;
        }

        // Payload follows the 8-byte header
        uint32_t payloadSize = (target->size > 8) ? (target->size - 8) : 0;
        if (payloadSize == 0)
        {
            logger.warning(std::format("Zero payload size for '{}'", filename));
            return std::vector<uint8_t>{};
        }

        payload.resize(payloadSize);
        file.read(reinterpret_cast<char*>(payload.data()),
                  static_cast<std::streamsize>(payloadSize));
    }
    else
    {
        // Uncompressed: raw data
        payload.resize(target->size);
        file.read(reinterpret_cast<char*>(payload.data()),
                  static_cast<std::streamsize>(target->size));
        // For uncompressed files, metaUncompressed is effectively the file size (though we don't
        // use it for decompression)
        metaUncompressed = target->size;
    }

    if (!file.good())
    {
        logger.error(
            std::format("Failed to read {} bytes of data for '{}'", payload.size(), filename));
        return std::nullopt;
    }

    // Check for zlib compression (magic byte 0x78)
    if (payload.size() >= 2 && payload[0] == 0x78)
    {
        unsigned long destLen = metaUncompressed > 0
                                    ? metaUncompressed
                                    : static_cast<unsigned long>(payload.size() * 4);
        std::vector<uint8_t> decompressed(destLen);

        int result = uncompress(decompressed.data(), &destLen, payload.data(),
                                static_cast<uLong>(payload.size()));
        if (result == Z_OK)
        {
            decompressed.resize(destLen);
            logger.debug(std::format("Extracted '{}': {} -> {} bytes (decompressed)", filename,
                                     payload.size(), destLen));
            return decompressed;
        }

        logger.warning(std::format("zlib decompression failed for '{}' (err={}), returning raw",
                                   filename, result));
    }

    // Not compressed — return raw payload
    logger.debug(std::format("Extracted '{}': {} bytes (raw)", filename, payload.size()));
    return payload;
}

} // namespace runeharbor::formats
