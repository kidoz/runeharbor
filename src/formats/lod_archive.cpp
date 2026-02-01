// SPDX-License-Identifier: MIT
#include "lod_archive.hpp"

#include <algorithm>
#include <format>

#include <cstring>
#include <zlib.h>

namespace runeharbor::formats
{

LODArchive::LODArchive(util::ILogger& logger) : logger(logger) {}

LODArchive::~LODArchive()
{
    close();
}

bool LODArchive::open(const std::filesystem::path& path)
{
    if (opened)
    {
        logger.warning("LODArchive already opened, closing previous file");
        close();
    }

    archivePath = path;
    file.open(path, std::ios::binary);

    if (!file.is_open())
    {
        logger.error(std::format("Failed to open LOD archive: {}", path.string()));
        return false;
    }

    logger.debug(std::format("Opened LOD archive: {}", path.string()));

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
    logger.info(std::format("Successfully loaded LOD archive with {} files", entries.size()));
    return true;
}

void LODArchive::close()
{
    if (file.is_open())
    {
        file.close();
        logger.debug(std::format("Closed LOD archive: {}", archivePath.string()));
    }
    entries.clear();
    dataEntries.clear();
    dataIndexBuilt = false;
    opened = false;
}

bool LODArchive::isOpen() const
{
    return opened;
}

bool LODArchive::readHeader()
{
    LODHeader header;
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(&header), sizeof(LODHeader));

    if (!file.good())
    {
        logger.error("Failed to read LOD header");
        return false;
    }

    // Verify magic number
    if (std::strncmp(header.magic, "LOD", 3) != 0)
    {
        logger.error(std::format("Invalid LOD magic number: expected 'LOD', got '{}'",
                                 std::string(header.magic, 3)));
        return false;
    }

    // Log game ID
    std::string gameId(header.gameId, 4);
    logger.debug(std::format("LOD archive game ID: {}", gameId));

    return true;
}

bool LODArchive::readDirectory()
{
    // Directory starts at offset 0x100 (256 bytes)
    constexpr std::streamoff directoryOffset = 0x100;

    file.seekg(directoryOffset, std::ios::beg);

    while (file.good())
    {
        std::streamoff entryPos = file.tellg();
        LODDirectoryEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(LODDirectoryEntry));

        if (!file.good())
        {
            break;
        }

        // Check for end of directory (null filename)
        if (entry.filename[0] == '\0')
        {
            // Data section starts at the null entry position + 8 bytes
            // (the null entry is only 8 zero bytes, not a full 32-byte entry)
            dataSectionStart = entryPos + 8;
            logger.debug(std::format("End of directory at offset 0x{:X}, data starts at 0x{:X}",
                                     entryPos, dataSectionStart));
            break;
        }

        // Note: entry.offset is a SORT KEY (not file position)
        // entry.size semantics vary by position (see extractFile for details)
        entries.push_back(entry);
    }

    if (entries.empty())
    {
        logger.error("No files found in LOD archive directory");
        return false;
    }

    return true;
}

std::vector<std::string> LODArchive::listFiles() const
{
    if (!dataIndexBuilt)
    {
        const_cast<LODArchive*>(this)->buildDataIndex();
    }

    std::vector<std::string> filenames;
    filenames.reserve(dataEntries.size());

    for (const auto& entry : dataEntries)
    {
        filenames.push_back(entry.name);
    }

    return filenames;
}

std::optional<std::vector<uint8_t>> LODArchive::extractFile(const std::string& filename)
{
    if (!opened)
    {
        logger.error("Cannot extract file: archive not opened");
        return std::nullopt;
    }

    if (!dataIndexBuilt && !buildDataIndex())
    {
        logger.error("Failed to build LOD data index");
        return std::nullopt;
    }

    const DataEntry* target = nullptr;
    for (const auto& entry : dataEntries)
    {
        if (entry.name.size() != filename.size())
        {
            continue;
        }

        bool match = true;
        for (size_t j = 0; j < entry.name.size(); j++)
        {
            if (std::tolower(entry.name[j]) != std::tolower(filename[j]))
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
        logger.error(std::format("File not found in archive: {}", filename));
        return std::nullopt;
    }

    if (target->dataOffset <= 0 || target->compressedSize == 0)
    {
        logger.error(std::format("Invalid entry for file: {}", filename));
        return std::nullopt;
    }

    file.seekg(target->dataOffset, std::ios::beg);
    if (!file.good())
    {
        logger.error(std::format("Failed to seek to data offset 0x{:X}",
                                 static_cast<uint64_t>(target->dataOffset)));
        return std::nullopt;
    }

    std::vector<uint8_t> compressed(target->compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), target->compressedSize);
    if (!file.good())
    {
        logger.error("Failed to read compressed data");
        return std::nullopt;
    }

    if (isZlibCompressed(compressed))
    {
        auto decompressed = decompressZlib(compressed);
        if (decompressed.empty())
        {
            logger.error("Failed to decompress file");
            return std::nullopt;
        }

        if (target->uncompressedSize != 0 && decompressed.size() != target->uncompressedSize)
        {
            logger.warning(std::format("Size mismatch for {}: expected {}, got {}", target->name,
                                       target->uncompressedSize, decompressed.size()));
        }

        logger.info(std::format("Successfully extracted: {} ({} bytes decompressed)", target->name,
                                decompressed.size()));
        return decompressed;
    }

    logger.info(std::format("Successfully extracted: {} ({} bytes raw)", target->name,
                            compressed.size()));
    return compressed;
}

bool LODArchive::buildDataIndex()
{
    if (dataIndexBuilt)
    {
        return true;
    }

    if (!opened)
    {
        logger.error("Cannot build LOD data index: archive not opened");
        return false;
    }

    dataEntries.clear();

    file.seekg(0, std::ios::end);
    std::streamoff fileSize = file.tellg();
    if (fileSize <= 0 || dataSectionStart <= 0 || dataSectionStart >= fileSize)
    {
        logger.error("Invalid data section start for LOD archive");
        return false;
    }

    // Find first file name by smallest directory offset (used as order key)
    std::string firstName;
    uint32_t firstCompressedSize = 0;
    if (!entries.empty())
    {
        const LODDirectoryEntry* first = &entries.front();
        for (const auto& entry : entries)
        {
            if (entry.offset < first->offset)
            {
                first = &entry;
            }
        }

        for (int i = 0; i < 16 && first->filename[i] != '\0'; i++)
        {
            firstName += first->filename[i];
        }
        firstCompressedSize = first->size;
    }

    std::streamoff cursor = dataSectionStart;

    // Parse first file (no filename header)
    if (!firstName.empty() && firstCompressedSize > 0 && cursor + 8 < fileSize &&
        cursor + 8 + firstCompressedSize <= fileSize)
    {
        uint32_t uncompressedSize = 0;
        uint32_t flags = 0;
        file.seekg(cursor, std::ios::beg);
        file.read(reinterpret_cast<char*>(&uncompressedSize), 4);
        file.read(reinterpret_cast<char*>(&flags), 4);

        std::streamoff dataOffset = cursor + 8;
        if (dataOffset + firstCompressedSize <= fileSize && firstCompressedSize > 0)
        {
            dataEntries.push_back({firstName, firstCompressedSize, uncompressedSize, dataOffset,
                                   flags});
            cursor = dataOffset + firstCompressedSize;
        }
    }

    // Parse subsequent files with 48-byte headers
    while (cursor + 48 < fileSize)
    {
        char nameBuf[16] = {};
        file.seekg(cursor, std::ios::beg);
        file.read(nameBuf, 16);

        if (!file.good())
        {
            break;
        }

        if (nameBuf[0] == '\0')
        {
            break;
        }

        std::string name;
        for (int i = 0; i < 16 && nameBuf[i] != '\0'; i++)
        {
            name += nameBuf[i];
        }

        uint32_t meta[8] = {};
        file.read(reinterpret_cast<char*>(meta), sizeof(meta));
        if (!file.good())
        {
            break;
        }

        uint32_t compressedSize = meta[1];
        uint32_t uncompressedSize = meta[6];
        uint32_t flags = meta[7];

        std::streamoff dataOffset = cursor + 48;
        if (compressedSize == 0 || dataOffset + compressedSize > fileSize)
        {
            break;
        }

        dataEntries.push_back({name, compressedSize, uncompressedSize, dataOffset, flags});
        cursor = dataOffset + compressedSize;
    }

    dataIndexBuilt = !dataEntries.empty();
    if (!dataIndexBuilt)
    {
        logger.error("Failed to parse any LOD data entries");
    }

    return dataIndexBuilt;
}

bool LODArchive::isZlibCompressed(const std::vector<uint8_t>& data) const
{
    // zlib compressed data starts with 0x78 0x9C (default compression)
    // or 0x78 0x01 (no compression), 0x78 0xDA (best compression)
    return data.size() >= 2 && data[0] == 0x78 &&
           (data[1] == 0x9C || data[1] == 0x01 || data[1] == 0xDA);
}

std::vector<uint8_t> LODArchive::decompressZlib(const std::vector<uint8_t>& data)
{
    z_stream stream = {};
    stream.next_in = const_cast<Bytef*>(data.data());
    stream.avail_in = static_cast<uInt>(data.size());

    if (inflateInit(&stream) != Z_OK)
    {
        logger.error("Failed to initialize zlib decompression");
        return {};
    }

    std::vector<uint8_t> decompressed;
    constexpr size_t bufferSize = 32768; // 32 KB chunks
    std::vector<uint8_t> buffer(bufferSize);

    int result;
    do
    {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(bufferSize);

        result = inflate(&stream, Z_NO_FLUSH);

        if (result == Z_STREAM_ERROR || result == Z_DATA_ERROR || result == Z_MEM_ERROR)
        {
            logger.error(std::format("zlib decompression error: {}", result));
            inflateEnd(&stream);
            return {};
        }

        size_t decompressedSize = bufferSize - stream.avail_out;
        decompressed.insert(decompressed.end(), buffer.begin(), buffer.begin() + decompressedSize);

    } while (result != Z_STREAM_END);

    inflateEnd(&stream);
    logger.debug(
        std::format("Decompressed {} bytes to {} bytes", data.size(), decompressed.size()));
    return decompressed;
}

} // namespace runeharbor::formats
