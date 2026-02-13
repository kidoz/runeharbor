#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "game_lod_archive.hpp"

#include <algorithm>
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
    GameLODHeader header;
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(&header), sizeof(GameLODHeader));

    if (!file.good())
    {
        logger.error("Failed to read game LOD header");
        return false;
    }

    if (std::strncmp(header.magic, "LOD", 3) != 0)
    {
        logger.error(std::format("Invalid LOD magic: expected 'LOD', got '{}'",
                                 std::string(header.magic, 3)));
        return false;
    }

    std::string gameId(header.gameId, 4);
    logger.debug(std::format("Game LOD archive game ID: {}", gameId));

    return true;
}

bool GameLODArchive::readDirectory()
{
    // Directory starts at offset 0x100 (256 bytes)
    constexpr std::streamoff directoryOffset = 0x100;
    file.seekg(directoryOffset, std::ios::beg);

    // First entry is metadata (like "maps")
    // Format: name(8) + fields(24) where last field (reserved[1]) is file count
    GameLODDirectoryEntry metaEntry;
    file.read(reinterpret_cast<char*>(&metaEntry), sizeof(GameLODDirectoryEntry));

    if (!file.good())
    {
        logger.error("Failed to read game directory metadata");
        return false;
    }

    logger.debug(std::format("Game archive metadata: {}", buildFilename(metaEntry)));

    // The file count is stored in the last field of metadata entry (reserved[1])
    uint32_t fileCount = metaEntry.reserved[1];
    logger.debug(std::format("Expected {} file entries", fileCount));

    if (fileCount == 0 || fileCount > 10000)
    {
        logger.error(std::format("Invalid file count: {}", fileCount));
        return false;
    }

    // Read exactly fileCount entries
    entries.reserve(fileCount);
    for (uint32_t i = 0; i < fileCount; i++)
    {
        GameLODDirectoryEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(GameLODDirectoryEntry));

        if (!file.good())
        {
            logger.error(std::format("Failed to read entry {}", i));
            break;
        }

        entries.push_back(entry);
    }

    // Data section starts right after the directory entries
    dataSectionStart = directoryOffset + sizeof(GameLODDirectoryEntry) * (1 + fileCount);
    logger.debug(
        std::format("Data section starts at 0x{:X}", static_cast<uint64_t>(dataSectionStart)));

    logger.debug(std::format("Read {} game directory entries", entries.size()));

    if (entries.empty())
    {
        logger.error("No files found in game archive directory");
        return false;
    }

    return true;
}

std::string GameLODArchive::buildFilename(const GameLODDirectoryEntry& entry) const
{
    std::string name;
    for (int i = 0; i < 8 && entry.name[i] != '\0'; i++)
    {
        name += entry.name[i];
    }

    // GAMES.LOD uses 8.3-style names; some entries truncate the final extension
    // (e.g., out01.od -> out01.odm, out01.dd -> out01.ddm).
    std::string lower = name;
    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.size() == 7 && lower.rfind("out", 0) == 0 &&
        (lower.size() >= 3 &&
         (lower.substr(lower.size() - 3) == ".od" || lower.substr(lower.size() - 3) == ".dd")))
    {
        name += "m";
    }

    return name;
}

std::vector<std::string> GameLODArchive::listFiles() const
{
    std::vector<std::string> filenames;
    filenames.reserve(entries.size());

    for (const auto& entry : entries)
    {
        filenames.push_back(buildFilename(entry));
    }

    return filenames;
}

std::optional<GameLODDirectoryEntry> GameLODArchive::getFileInfo(const std::string& filename) const
{
    if (!opened)
    {
        return std::nullopt;
    }

    // Case-insensitive search
    for (const auto& entry : entries)
    {
        std::string entryName = buildFilename(entry);

        if (entryName.size() != filename.size())
            continue;

        bool match = true;
        for (size_t j = 0; j < entryName.size(); j++)
        {
            if (std::tolower(entryName[j]) != std::tolower(filename[j]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            return entry;
        }
    }

    return std::nullopt;
}

std::optional<std::vector<uint8_t>> GameLODArchive::extractFile(const std::string& filename)
{
    if (!opened)
    {
        logger.error("Archive not open");
        return std::nullopt;
    }

    // Find entry index (case-insensitive)
    size_t targetIndex = 0;
    bool found = false;
    for (size_t i = 0; i < entries.size(); i++)
    {
        std::string entryName = buildFilename(entries[i]);

        if (entryName.size() != filename.size())
            continue;

        bool match = true;
        for (size_t j = 0; j < entryName.size(); j++)
        {
            if (std::tolower(entryName[j]) != std::tolower(filename[j]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            targetIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        logger.error(std::format("File not found: {}", filename));
        return std::nullopt;
    }

    // Files are stored sequentially, each with 16-byte header + zlib data
    // Need to skip through previous files to find the target
    std::streamoff currentPos = dataSectionStart;
    file.seekg(currentPos, std::ios::beg);

    for (size_t i = 0; i <= targetIndex; i++)
    {
        // Read 16-byte chunk header
        uint8_t chunkHeader[16];
        file.read(reinterpret_cast<char*>(chunkHeader), 16);
        if (!file.good())
        {
            logger.error(std::format("Failed to read chunk header for file {}", i));
            return std::nullopt;
        }

        // Parse header: [unknown:4][identifier:4][compressed_size:4][decompressed_size:4]
        uint32_t compressedSize = *reinterpret_cast<uint32_t*>(chunkHeader + 8);
        uint32_t decompressedSize = *reinterpret_cast<uint32_t*>(chunkHeader + 12);

        if (i == targetIndex)
        {
            // This is our file - read and decompress
            logger.debug(std::format("Extracting: {} (compressed: {}, decompressed: {})", filename,
                                     compressedSize, decompressedSize));

            std::vector<uint8_t> compressed(compressedSize);
            file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

            if (!file.good())
            {
                logger.error("Failed to read compressed data");
                return std::nullopt;
            }

            // Verify zlib header
            if (compressed.size() >= 2 && compressed[0] == 0x78)
            {
                std::vector<uint8_t> decompressed(decompressedSize);
                unsigned long destLen = decompressedSize;

                int result =
                    uncompress(decompressed.data(), &destLen, compressed.data(), compressed.size());

                if (result == Z_OK)
                {
                    decompressed.resize(destLen);
                    logger.debug(
                        std::format("Decompressed: {} -> {} bytes", compressedSize, destLen));
                    return decompressed;
                }
                else
                {
                    logger.error(std::format("zlib decompression failed: {}", result));
                    return std::nullopt;
                }
            }
            else
            {
                // Not compressed, return as-is
                logger.debug(
                    std::format("Data not compressed, returning {} bytes", compressedSize));
                return compressed;
            }
        }
        else
        {
            // Skip this file's compressed data
            file.seekg(compressedSize, std::ios::cur);
        }
    }

    logger.error("Unexpected end of extraction loop");
    return std::nullopt;
}

} // namespace runeharbor::formats
