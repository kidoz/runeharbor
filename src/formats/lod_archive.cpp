// SPDX-License-Identifier: MIT
#include "lod_archive.hpp"

#include <algorithm>
#include <format>
#include <map>

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
    std::vector<std::string> filenames;
    filenames.reserve(entries.size());

    for (const auto& entry : entries)
    {
        filenames.emplace_back(entry.filename);
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

    // Create list sorted by directory offset field (remove duplicates, keep last occurrence)
    // The offset field is used as a sort key, not an actual file position
    std::map<std::string, LODDirectoryEntry> uniqueFiles;
    for (const auto& entry : entries)
    {
        // Extract clean filename (up to null terminator)
        std::string cleanName;
        for (int i = 0; i < 16 && entry.filename[i] != '\0'; i++)
        {
            cleanName += entry.filename[i];
        }
        uniqueFiles[cleanName] = entry; // Overwrites duplicates
    }

    // Sort by directory offset field
    std::vector<std::pair<std::string, LODDirectoryEntry>> sortedFiles;
    for (const auto& pair : uniqueFiles)
    {
        sortedFiles.push_back(pair);
    }
    std::sort(sortedFiles.begin(), sortedFiles.end(),
              [](const auto& a, const auto& b) { return a.second.offset < b.second.offset; });

    // Filter out files with unreasonably large sizes (likely garbage/corrupt entries)
    // Note: Map files can be several MB, so we use a high threshold
    constexpr uint32_t MAX_REASONABLE_SIZE = 50000000; // 50MB threshold
    std::vector<std::pair<std::string, LODDirectoryEntry>> validFiles;
    for (const auto& pair : sortedFiles)
    {
        if (pair.second.size < MAX_REASONABLE_SIZE)
        {
            validFiles.push_back(pair);
        }
        else
        {
            logger.debug(std::format("Skipping corrupt entry: {} ({} bytes - unreasonably large)",
                                     pair.first, pair.second.size));
        }
    }
    sortedFiles = std::move(validFiles);

    // Debug: log first few files after filtering
    // (Commented out to reduce log verbosity - uncomment if needed for debugging)
    // logger.debug(std::format("Filtered file list (first 10):"));
    // for (size_t i = 0; i < sortedFiles.size() && i < 10; i++)
    // {
    //     logger.debug(std::format("  [{}] {} (offset={}, size={})",
    //                              i, sortedFiles[i].first,
    //                              sortedFiles[i].second.offset,
    //                              sortedFiles[i].second.size));
    // }

    // Find target file (case-insensitive) in filtered list
    size_t fileIndex = 0;
    bool found = false;

    for (size_t i = 0; i < sortedFiles.size(); i++)
    {
        const auto& name = sortedFiles[i].first;
        if (name.size() != filename.size())
            continue;

        bool match = true;
        for (size_t j = 0; j < name.size(); j++)
        {
            if (std::tolower(name[j]) != std::tolower(filename[j]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            fileIndex = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        logger.error(std::format("File not found in archive: {}", filename));
        return std::nullopt;
    }

    const auto& targetEntry = sortedFiles[fileIndex].second;
    logger.debug(std::format("Extracting file #{}: {} (compressed size: {})", fileIndex,
                             sortedFiles[fileIndex].first, targetEntry.size));

    // Calculate position in data section
    // Size field semantics:
    // - File #0: size = compressed data only
    // - File #1+: size = 8-byte header + compressed data + next file's 40-byte prefix
    //
    // Layout:
    // - File #0: [8-byte header][compressed data]
    // - File #1+: [32-byte filename][8-byte padding][data from size field]

    std::streamoff currentPos = dataSectionStart;

    if (fileIndex == 0)
    {
        // File #0: header starts at dataSectionStart
        // No additional seeking needed
    }
    else
    {
        // Skip file #0: 8-byte header + compressed data + next file's 40-byte prefix
        currentPos += 8 + sortedFiles[0].second.size + 40;

        // Skip files #1 to fileIndex-1: just add their size (already includes everything)
        for (size_t i = 1; i < fileIndex; i++)
        {
            currentPos += sortedFiles[i].second.size;
        }
    }

    logger.debug(std::format("Seeking to header at: 0x{:X}", currentPos));
    file.seekg(currentPos, std::ios::beg);

    if (!file.good())
    {
        logger.error(std::format("Failed to seek to offset 0x{:X}", currentPos));
        return std::nullopt;
    }

    // Read 8-byte header
    uint32_t uncompressedSize = 0;
    uint32_t unknown = 0;
    file.read(reinterpret_cast<char*>(&uncompressedSize), 4);
    file.read(reinterpret_cast<char*>(&unknown), 4);

    logger.debug(
        std::format("File header: uncompressed={}, unknown={}", uncompressedSize, unknown));

    // Read compressed data
    std::vector<uint8_t> compressed(targetEntry.size);
    file.read(reinterpret_cast<char*>(compressed.data()), targetEntry.size);

    if (!file.good())
    {
        logger.error(std::format("Failed to read compressed data"));
        return std::nullopt;
    }

    // Decompress
    std::vector<uint8_t> decompressed = decompressZlib(compressed);

    if (decompressed.empty())
    {
        logger.error(std::format("Failed to decompress file"));
        return std::nullopt;
    }

    if (decompressed.size() != uncompressedSize)
    {
        logger.error(std::format("Size mismatch: expected {}, got {}", uncompressedSize,
                                 decompressed.size()));
    }

    logger.info(std::format("Successfully extracted: {} ({} bytes decompressed)",
                            sortedFiles[fileIndex].first, decompressed.size()));
    return decompressed;
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
