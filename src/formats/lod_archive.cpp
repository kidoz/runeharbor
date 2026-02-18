#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include <algorithm>
#include <format>

#include <cstring>
#include <zlib.h>

#include "lod_archive.hpp"

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

        // entry.offset = absolute file position of the data block
        // entry.size = total data block size (8-byte header + compressed data)
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
        logger.debug(std::format("File not found in archive: {}", filename));
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

    logger.info(
        std::format("Successfully extracted: {} ({} bytes raw)", target->name, compressed.size()));
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
    dataEntries.reserve(entries.size());

    file.seekg(0, std::ios::end);
    std::streamoff fileSize = file.tellg();

    for (const auto& entry : entries)
    {
        std::string name;
        for (int i = 0; i < 16 && entry.filename[i] != '\0'; i++)
        {
            name += entry.filename[i];
        }

        if (name.empty())
        {
            continue;
        }

        // In MM7 LOD format, directory entry 'offset' is absolute.
        // Each file data block starts with 8 bytes of metadata:
        // [4 bytes uncompressed size][4 bytes flags]
        // The directory entry 'size' is the COMPRESSED size including these 8 bytes?
        // Actually, usually 'size' is compressed size of data only, and metadata is extra.
        // Let's assume size is total data block size.

        if (entry.offset >= fileSize)
        {
            logger.warning(std::format("Entry '{}' has invalid offset 0x{:X}", name, entry.offset));
            continue;
        }

        file.seekg(entry.offset, std::ios::beg);
        uint32_t uncompressedSize = 0;
        uint32_t flags = 0;
        file.read(reinterpret_cast<char*>(&uncompressedSize), 4);
        file.read(reinterpret_cast<char*>(&flags), 4);

        if (!file.good())
        {
            logger.warning(std::format("Failed to read metadata for entry '{}'", name));
            continue;
        }

        std::streamoff dataOffset = static_cast<std::streamoff>(entry.offset) + 8;
        uint32_t compressedSize = (entry.size > 8) ? (entry.size - 8) : 0;

        dataEntries.push_back({name, compressedSize, uncompressedSize, dataOffset, flags});
    }

    dataIndexBuilt = !dataEntries.empty();
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
