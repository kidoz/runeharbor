// SPDX-License-Identifier: MIT
#include "sprite_lod_archive.hpp"

#include <algorithm>
#include <format>

#include <cstring>
#include <zlib.h>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

SpriteLODArchive::SpriteLODArchive(util::ILogger& logger) : logger(logger) {}

SpriteLODArchive::~SpriteLODArchive()
{
    close();
}

bool SpriteLODArchive::open(const std::filesystem::path& path)
{
    if (opened)
    {
        close();
    }

    archivePath = path;
    file.open(path, std::ios::binary);

    if (!file.is_open())
    {
        logger.error(std::format("Failed to open sprite archive: {}", path.string()));
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
    logger.info(std::format("Successfully loaded sprite archive with {} sprites", entries.size()));
    return true;
}

void SpriteLODArchive::close()
{
    if (file.is_open())
    {
        file.close();
        logger.debug(std::format("Closed sprite archive: {}", archivePath.string()));
    }
    entries.clear();
    opened = false;
}

bool SpriteLODArchive::isOpen() const
{
    return opened;
}

bool SpriteLODArchive::readHeader()
{
    SpriteLODHeader header;
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(&header), sizeof(SpriteLODHeader));

    if (!file.good())
    {
        logger.error("Failed to read sprite LOD header");
        return false;
    }

    if (std::strncmp(header.magic, "LOD", 3) != 0)
    {
        logger.error(std::format("Invalid LOD magic: expected 'LOD', got '{}'",
                                 std::string(header.magic, 3)));
        return false;
    }

    std::string gameId(header.gameId, 4);
    logger.debug(std::format("Sprite LOD archive game ID: {}", gameId));

    return true;
}

bool SpriteLODArchive::readDirectory()
{
    // Directory starts at offset 0x100 (256 bytes)
    constexpr std::streamoff directoryOffset = 0x100;
    file.seekg(directoryOffset, std::ios::beg);

    // First entry is metadata (like "sprites08")
    // Format: [name:12][unknown:4][dirStart:4][unknown:4][zero:4][entryCount:4]
    SpriteLODDirectoryEntry metaEntry;
    file.read(reinterpret_cast<char*>(&metaEntry), sizeof(SpriteLODDirectoryEntry));

    if (!file.good())
    {
        logger.error("Failed to read sprite directory metadata");
        return false;
    }

    logger.debug(std::format("Sprite archive metadata: {}", buildFilename(metaEntry)));

    // Entry count is in the last 4 bytes of the metadata entry (reserved[1])
    uint32_t entryCount = metaEntry.reserved[1];
    logger.debug(std::format("Expected {} sprite entries", entryCount));

    if (entryCount == 0 || entryCount > 100000)
    {
        logger.warning(std::format("Invalid entry count {}, falling back to scan", entryCount));
        entryCount = 0; // Will scan until non-text
    }

    // Read directory entries
    entries.reserve(entryCount > 0 ? entryCount : 10000);

    for (uint32_t i = 0; i < (entryCount > 0 ? entryCount : 100000); i++)
    {
        SpriteLODDirectoryEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(SpriteLODDirectoryEntry));

        if (!file.good())
        {
            break;
        }

        // If no entry count, check for end of directory
        if (entryCount == 0)
        {
            if (entry.name[0] == '\0' || (static_cast<unsigned char>(entry.name[0]) < 0x20 ||
                                          static_cast<unsigned char>(entry.name[0]) > 0x7E))
            {
                break;
            }
        }

        entries.push_back(entry);
    }

    // Data section starts after all entries
    dataSectionStart = directoryOffset + sizeof(SpriteLODDirectoryEntry) +
                       entries.size() * sizeof(SpriteLODDirectoryEntry);

    logger.debug(std::format("Read {} sprite directory entries, data starts at 0x{:X}",
                             entries.size(), static_cast<uint64_t>(dataSectionStart)));

    if (entries.empty())
    {
        logger.error("No sprites found in archive directory");
        return false;
    }

    return true;
}

std::string SpriteLODArchive::buildFilename(const SpriteLODDirectoryEntry& entry) const
{
    std::string name;
    for (int i = 0; i < 12 && entry.name[i] != '\0'; i++)
    {
        name += entry.name[i];
    }
    return name;
}

std::vector<std::string> SpriteLODArchive::listFiles() const
{
    std::vector<std::string> filenames;
    filenames.reserve(entries.size());

    for (const auto& entry : entries)
    {
        filenames.push_back(buildFilename(entry));
    }

    return filenames;
}

std::streamoff SpriteLODArchive::calculateDataOffset(size_t entryIndex) const
{
    // Sprites are stored sequentially in directory order starting at dataSectionStart
    std::streamoff offset = dataSectionStart;

    for (size_t i = 0; i < entryIndex; i++)
    {
        offset += entries[i].size;
    }

    return offset;
}

std::optional<std::vector<uint8_t>> SpriteLODArchive::extractFile(const std::string& filename)
{
    if (!opened)
    {
        logger.error("Archive not open");
        return std::nullopt;
    }

    // Find entry index (case-insensitive)
    size_t entryIndex = 0;
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
            entryIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        logger.error(std::format("Sprite not found: {}", filename));
        return std::nullopt;
    }

    // Calculate offset based on directory order
    std::streamoff dataOffset = calculateDataOffset(entryIndex);
    logger.debug(std::format("Extracting sprite: {} (calculated offset: 0x{:X}, size: {})",
                             filename, static_cast<uint64_t>(dataOffset),
                             entries[entryIndex].size));

    // Seek to sprite data
    file.seekg(dataOffset, std::ios::beg);
    if (!file.good())
    {
        logger.error(
            std::format("Failed to seek to offset 0x{:X}", static_cast<uint64_t>(dataOffset)));
        return std::nullopt;
    }

    // Read sprite header (32 bytes)
    SpriteFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(SpriteFileHeader));

    if (!file.good())
    {
        logger.error("Failed to read sprite header");
        return std::nullopt;
    }

    std::string headerName(header.name, 8);
    headerName = headerName.substr(0, headerName.find('\0'));
    logger.debug(std::format("Sprite header: name='{}', size={}x{}, compressed={}, decompressed={}",
                             headerName, header.width, header.height, header.compressedSize,
                             header.decompressedSize));

    // Validate header
    if (header.width == 0 || header.height == 0 || header.width > 1024 || header.height > 1024)
    {
        logger.error(std::format("Invalid sprite dimensions: {}x{}", header.width, header.height));
        return std::nullopt;
    }

    // Line info table: 8 bytes per line
    // Format per line: [x_start:2][x_end:2][data_offset:4]
    size_t lineInfoSize = header.height * 8;
    std::vector<uint8_t> lineInfo(lineInfoSize);
    file.read(reinterpret_cast<char*>(lineInfo.data()), lineInfoSize);

    if (!file.good())
    {
        logger.error("Failed to read line info table");
        return std::nullopt;
    }

    // Read compressed pixel data
    if (header.compressedSize == 0)
    {
        logger.warning("Sprite has no pixel data");
        return std::nullopt;
    }

    std::vector<uint8_t> compressed(header.compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), header.compressedSize);

    if (!file.good())
    {
        logger.error("Failed to read compressed sprite data");
        return std::nullopt;
    }

    // Decompress pixel data
    std::vector<uint8_t> decompressed;
    if (compressed.size() >= 2 && compressed[0] == 0x78 &&
        (compressed[1] == 0x9C || compressed[1] == 0x01 || compressed[1] == 0xDA))
    {
        // The header's decompressedSize field is sometimes incorrect or means something else
        // Use a larger buffer and retry with increasing sizes if needed
        size_t bufferSize =
            std::max(static_cast<size_t>(header.decompressedSize), compressed.size()) * 4;
        int result = Z_BUF_ERROR;

        for (int attempt = 0; attempt < 3 && result == Z_BUF_ERROR; attempt++)
        {
            decompressed.resize(bufferSize);
            unsigned long destLen = bufferSize;

            result =
                uncompress(decompressed.data(), &destLen, compressed.data(), compressed.size());

            if (result == Z_OK)
            {
                decompressed.resize(destLen);
                logger.debug(
                    std::format("Decompressed sprite: {} -> {} bytes", compressed.size(), destLen));
            }
            else if (result == Z_BUF_ERROR)
            {
                bufferSize *= 2;
            }
        }

        if (result != Z_OK)
        {
            logger.error(std::format("zlib decompression failed: error code {}", result));
            return std::nullopt;
        }
    }
    else
    {
        // Not compressed, use as-is
        decompressed = std::move(compressed);
        logger.debug(std::format("Sprite data not compressed: {} bytes", decompressed.size()));
    }

    // Build result with metadata and decompressed pixel data
    // Format: [width:2][height:2][centerX:2][centerY:2][paletteId:2][lineInfo:height*8][pixels...]
    std::vector<uint8_t> result_data;
    result_data.reserve(10 + lineInfo.size() + decompressed.size());

    // Add metadata
    result_data.push_back(header.width & 0xFF);
    result_data.push_back((header.width >> 8) & 0xFF);
    result_data.push_back(header.height & 0xFF);
    result_data.push_back((header.height >> 8) & 0xFF);
    result_data.push_back(header.centerX & 0xFF);
    result_data.push_back((header.centerX >> 8) & 0xFF);
    result_data.push_back(header.centerY & 0xFF);
    result_data.push_back((header.centerY >> 8) & 0xFF);
    result_data.push_back(header.paletteId & 0xFF);
    result_data.push_back((header.paletteId >> 8) & 0xFF);

    // Add line info
    result_data.insert(result_data.end(), lineInfo.begin(), lineInfo.end());

    // Add decompressed pixel data
    result_data.insert(result_data.end(), decompressed.begin(), decompressed.end());

    return result_data;
}

std::optional<SpriteFileHeader> SpriteLODArchive::getFileInfo(const std::string& filename)
{
    if (!opened)
    {
        return std::nullopt;
    }

    // Find entry index
    size_t entryIndex = 0;
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
            entryIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        return std::nullopt;
    }

    // Calculate offset based on directory order
    std::streamoff dataOffset = calculateDataOffset(entryIndex);

    // Seek to file and read header
    file.seekg(dataOffset, std::ios::beg);
    if (!file.good())
    {
        return std::nullopt;
    }

    SpriteFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(SpriteFileHeader));

    if (!file.good())
    {
        return std::nullopt;
    }

    return header;
}

} // namespace runeharbor::formats
