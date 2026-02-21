// SPDX-License-Identifier: MIT
#include "game_lod_archive.hpp"

#include <algorithm>
#include <format>

#include <cctype>
#include <cstring>
#include <zlib.h>

namespace runeharbor::formats
{
namespace
{
#pragma pack(push, 1)
struct GameLODChunkHeader
{
    uint32_t unknown0;
    uint32_t unknown1;
    uint32_t compressedSize;
    uint32_t decompressedSize;
};
#pragma pack(pop)

static_assert(sizeof(GameLODChunkHeader) == 16, "GameLODChunkHeader must be 16 bytes");

bool looksLikeZlibData(const std::vector<uint8_t>& data)
{
    if (data.size() < 2 || data[0] != 0x78)
    {
        return false;
    }

    const uint16_t header = static_cast<uint16_t>((data[0] << 8) | data[1]);
    return (header % 31) == 0;
}
} // namespace

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
    directoryMode = DirectoryMode::Standard;
    mapDataSectionStart = 0;
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

    logger.debug(std::format("Game LOD archive game ID: {}",
                             std::string(header_.gameId, sizeof(header_.gameId))));
    return true;
}

bool GameLODArchive::readDirectory()
{
    if (readMapChapterDirectory())
    {
        return true;
    }

    logger.debug("Map chapter directory not detected, falling back to standard directory");
    return readStandardDirectory();
}

bool GameLODArchive::readMapChapterDirectory()
{
    // Directory starts right after the 256-byte header
    constexpr std::streamoff kDirOffset = 0x100;
    file.clear();
    file.seekg(kDirOffset, std::ios::beg);

    GameLODDirectoryEntry chapterEntry{};
    file.read(reinterpret_cast<char*>(&chapterEntry), sizeof(chapterEntry));
    if (!file.good())
    {
        return false;
    }

    const std::string chapterName = entryName(chapterEntry);
    if (!equalsIgnoreCase(chapterName, "maps"))
    {
        return false;
    }

    uint32_t count = chapterEntry.reserved;
    if ((count == 0 || count > 10000) && (chapterEntry.reserved & 0xFFFFu) != 0)
    {
        count = chapterEntry.reserved & 0xFFFFu;
    }

    const uint32_t subdirOffset = chapterEntry.offset;
    if (count == 0 || count > 10000 || subdirOffset < 0x120)
    {
        return false;
    }

    file.clear();
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    if (!file.good() || fileSize <= 0)
    {
        return false;
    }

    const std::streamoff subdirEnd = static_cast<std::streamoff>(subdirOffset) +
                                     static_cast<std::streamoff>(count) *
                                         static_cast<std::streamoff>(sizeof(GameLODDirectoryEntry));
    if (static_cast<std::streamoff>(subdirOffset) >= fileSize || subdirEnd > fileSize)
    {
        return false;
    }

    file.clear();
    file.seekg(static_cast<std::streamoff>(subdirOffset), std::ios::beg);

    entries.resize(count);
    file.read(reinterpret_cast<char*>(entries.data()),
              static_cast<std::streamsize>(count * sizeof(GameLODDirectoryEntry)));
    if (!file.good())
    {
        entries.clear();
        return false;
    }

    std::erase_if(entries, [](const GameLODDirectoryEntry& e) { return e.name[0] == '\0'; });

    directoryMode = DirectoryMode::MapChapter;
    mapDataSectionStart = subdirEnd;

    logger.debug(std::format("Game archive metadata: {}", chapterName));
    logger.debug(std::format("Expected {} file entries", count));
    logger.debug(
        std::format("Data section starts at 0x{:X}", static_cast<uint64_t>(mapDataSectionStart)));
    logger.debug(std::format("Read {} game directory entries", entries.size()));
    return !entries.empty();
}

bool GameLODArchive::readStandardDirectory()
{
    constexpr std::streamoff kDirOffset = 0x100;
    file.clear();
    file.seekg(kDirOffset, std::ios::beg);

    uint32_t count = header_.numDirEntries;
    if (count == 0 || count > 10000)
    {
        logger.debug("numDirEntries unreliable, scanning directory");
        count = 0;
        while (count < 10000)
        {
            GameLODDirectoryEntry probe{};
            file.read(reinterpret_cast<char*>(&probe), sizeof(probe));
            if (!file.good() || probe.name[0] == '\0')
            {
                break;
            }
            count++;
        }
        file.clear();
        file.seekg(kDirOffset, std::ios::beg);
    }

    if (count == 0)
    {
        logger.error("No files found in game archive directory");
        return false;
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

    std::erase_if(entries, [](const GameLODDirectoryEntry& e) { return e.name[0] == '\0'; });
    directoryMode = DirectoryMode::Standard;
    mapDataSectionStart = 0;
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

bool GameLODArchive::equalsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); i++)
    {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i])))
        {
            return false;
        }
    }

    return true;
}

std::optional<size_t> GameLODArchive::findEntryIndex(const std::string& filename) const
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (equalsIgnoreCase(entryName(entries[i]), filename))
        {
            return i;
        }
    }

    return std::nullopt;
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

    const auto entryIndex = findEntryIndex(filename);
    if (!entryIndex.has_value())
    {
        logger.debug(std::format("File not found: {}", filename));
        return std::nullopt;
    }

    if (directoryMode == DirectoryMode::MapChapter)
    {
        return extractMapChapterFile(*entryIndex, filename);
    }

    return extractStandardFile(entries[*entryIndex], filename);
}

std::optional<std::vector<uint8_t>>
GameLODArchive::extractMapChapterFile(size_t entryIndex, const std::string& filename)
{
    if (mapDataSectionStart <= 0)
    {
        logger.error("Invalid map data section start");
        return std::nullopt;
    }

    file.clear();
    file.seekg(mapDataSectionStart, std::ios::beg);
    if (!file.good())
    {
        logger.error(std::format("Failed to seek to map data section for '{}'", filename));
        return std::nullopt;
    }

    for (size_t i = 0; i <= entryIndex; i++)
    {
        GameLODChunkHeader chunkHeader{};
        file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));
        if (!file.good())
        {
            logger.error(std::format("Failed to read chunk header for '{}'", filename));
            return std::nullopt;
        }

        if (chunkHeader.compressedSize == 0)
        {
            logger.error(std::format("Invalid compressed size for '{}'", filename));
            return std::nullopt;
        }

        if (i != entryIndex)
        {
            file.seekg(static_cast<std::streamoff>(chunkHeader.compressedSize), std::ios::cur);
            if (!file.good())
            {
                logger.error(std::format("Failed to skip chunk payload for '{}'", filename));
                return std::nullopt;
            }
            continue;
        }

        std::vector<uint8_t> compressed(chunkHeader.compressedSize);
        file.read(reinterpret_cast<char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
        if (!file.good())
        {
            logger.error(std::format("Failed to read compressed payload for '{}'", filename));
            return std::nullopt;
        }

        if (chunkHeader.decompressedSize > 0 || looksLikeZlibData(compressed))
        {
            const uLongf targetSize = chunkHeader.decompressedSize > 0
                                          ? static_cast<uLongf>(chunkHeader.decompressedSize)
                                          : static_cast<uLongf>(compressed.size() * 8);
            std::vector<uint8_t> decompressed(targetSize);
            uLongf destLen = targetSize;
            const int result = uncompress(decompressed.data(), &destLen, compressed.data(),
                                          static_cast<uLong>(compressed.size()));
            if (result == Z_OK)
            {
                decompressed.resize(destLen);
                logger.debug(std::format("Extracting: {} (compressed: {}, decompressed: {})",
                                         filename, chunkHeader.compressedSize, destLen));
                return decompressed;
            }

            logger.warning(std::format("zlib decompression failed for '{}' (err={}), returning raw",
                                       filename, result));
        }

        logger.debug(std::format("Extracted '{}': {} bytes (raw)", filename, compressed.size()));
        return compressed;
    }

    logger.error(std::format("Failed to locate map chunk for '{}'", filename));
    return std::nullopt;
}

std::optional<std::vector<uint8_t>>
GameLODArchive::extractStandardFile(const GameLODDirectoryEntry& entry, const std::string& filename)
{
    if (entry.offset == 0 || entry.size == 0)
    {
        logger.error(std::format("Invalid entry for '{}': offset=0x{:X}, size={}", filename,
                                 entry.offset, entry.size));
        return std::nullopt;
    }

    file.clear();
    file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!file.good())
    {
        logger.error(
            std::format("Failed to seek to offset 0x{:X} for '{}'", entry.offset, filename));
        return std::nullopt;
    }

    const bool hasHeader = (entry.decompressedSize > 0);

    std::vector<uint8_t> payload;
    uint32_t metaUncompressed = 0;

    if (hasHeader)
    {
        uint32_t metaFlags = 0;
        file.read(reinterpret_cast<char*>(&metaUncompressed), 4);
        file.read(reinterpret_cast<char*>(&metaFlags), 4);
        if (!file.good())
        {
            logger.error(std::format("Failed to read data header for '{}'", filename));
            return std::nullopt;
        }

        const uint32_t payloadSize = (entry.size > 8) ? (entry.size - 8) : 0;
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
        payload.resize(entry.size);
        file.read(reinterpret_cast<char*>(payload.data()),
                  static_cast<std::streamsize>(entry.size));
        metaUncompressed = entry.size;
    }

    if (!file.good())
    {
        logger.error(
            std::format("Failed to read {} bytes of data for '{}'", payload.size(), filename));
        return std::nullopt;
    }

    if (payload.size() >= 2 && payload[0] == 0x78)
    {
        uLongf destLen = metaUncompressed > 0 ? static_cast<uLongf>(metaUncompressed)
                                              : static_cast<uLongf>(payload.size() * 4);
        std::vector<uint8_t> decompressed(destLen);
        const int result = uncompress(decompressed.data(), &destLen, payload.data(),
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

    logger.debug(std::format("Extracted '{}': {} bytes (raw)", filename, payload.size()));
    return payload;
}

} // namespace runeharbor::formats
