// SPDX-License-Identifier: MIT
#include "image_lod_archive.hpp"

#include <algorithm>
#include <format>

#include <cstring>
#include <zlib.h>

namespace runeharbor::formats
{

ImageLODArchive::ImageLODArchive(util::ILogger& logger) : logger(logger) {}

ImageLODArchive::~ImageLODArchive()
{
    close();
}

bool ImageLODArchive::open(const std::filesystem::path& path)
{
    if (opened)
    {
        logger.warning("Archive already open, closing first");
        close();
    }

    archivePath = path;
    file.open(archivePath, std::ios::binary);

    if (!file.is_open())
    {
        logger.error(std::format("Failed to open file: {}", archivePath.string()));
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
    logger.info(std::format("Successfully loaded image LOD archive with {} files", entries.size()));

    return true;
}

void ImageLODArchive::close()
{
    if (file.is_open())
    {
        file.close();
        logger.debug(std::format("Closed image LOD archive: {}", archivePath.string()));
    }
    entries.clear();
    opened = false;
}

bool ImageLODArchive::isOpen() const
{
    return opened;
}

bool ImageLODArchive::readHeader()
{
    ImageLODHeader header;
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(&header), sizeof(ImageLODHeader));

    if (!file.good())
    {
        logger.error("Failed to read image LOD header");
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
    logger.debug(std::format("Image LOD archive game ID: {}", gameId));

    return true;
}

bool ImageLODArchive::readDirectory()
{
    // Directory starts at offset 0x100 (256 bytes)
    constexpr std::streamoff directoryOffset = 0x100;

    file.seekg(directoryOffset, std::ios::beg);

    while (file.good())
    {
        std::streamoff entryPos = file.tellg();
        ImageLODDirectoryEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(ImageLODDirectoryEntry));

        if (!file.good())
        {
            break;
        }

        // Check for end of directory (null filename)
        if (entry.shortName[0] == '\0')
        {
            // Data section starts at the null entry position + 8 bytes
            // (the null entry is only 8 zero bytes, not a full 32-byte entry)
            dataSectionStart = entryPos + 8;
            logger.debug(std::format("End of directory at offset 0x{:X}, data starts at 0x{:X}",
                                     static_cast<uint64_t>(entryPos),
                                     static_cast<uint64_t>(dataSectionStart)));
            break;
        }

        entries.push_back(entry);
    }

    if (entries.empty())
    {
        logger.error("No files found in image LOD archive directory");
        return false;
    }

    logger.debug(std::format("Read {} directory entries", entries.size()));
    return true;
}

ImageEntryType ImageLODArchive::detectEntryType(const ImageLODDirectoryEntry& entry) const
{
    // Check for "LIB." marker in UTF-16 LE
    // 0x4C 0x00 0x49 0x00 0x42 0x00 0x2E 0x00 = "LIB."
    const uint8_t* marker = reinterpret_cast<const uint8_t*>(entry.extensionMarker);

    if (marker[0] == 0x4C && marker[1] == 0x00 && marker[2] == 0x49 && marker[3] == 0x00 &&
        marker[4] == 0x42 && marker[5] == 0x00 && marker[6] == 0x2E && marker[7] == 0x00)
    {
        return ImageEntryType::CustomFormat;
    }

    return ImageEntryType::ExternalFormat;
}

std::string ImageLODArchive::buildFilename(const ImageLODDirectoryEntry& entry) const
{
    // Extract short name (up to 4 bytes, null-terminated)
    std::string filename;
    for (int i = 0; i < 4 && entry.shortName[i] != '\0'; i++)
    {
        filename += entry.shortName[i];
    }

    // Check if this is a custom format file (has LIB. marker)
    if (detectEntryType(entry) == ImageEntryType::CustomFormat)
    {
        filename += ".LIB";
    }

    return filename;
}

std::vector<std::string> ImageLODArchive::listFiles() const
{
    std::vector<std::string> filenames;
    filenames.reserve(entries.size());

    for (const auto& entry : entries)
    {
        filenames.push_back(buildFilename(entry));
    }

    return filenames;
}

std::streamoff ImageLODArchive::calculateDataOffset(const ImageLODDirectoryEntry& targetEntry) const
{
    // CRITICAL: Files are stored in DIRECTORY ORDER (not sorted by sortKey)
    // First entry ("bitm") is skipped - actual file data starts at 0xCAE0
    // (0xCB10 is where the first zlib block starts, 48 bytes after the header)
    // This offset was discovered through manual analysis
    constexpr std::streamoff actualDataStart = 0xCAE0;

    // Find target entry index in directory order
    auto it = std::find_if(entries.begin(), entries.end(), [&targetEntry](const auto& e)
                           { return std::strncmp(e.shortName, targetEntry.shortName, 4) == 0; });

    if (it == entries.end())
    {
        logger.error("Failed to find entry in directory");
        return actualDataStart;
    }

    size_t fileIndex = std::distance(entries.begin(), it);

    // Skip first entry (index 0, "bitm") - it's metadata, not actual file data
    if (fileIndex == 0)
    {
        logger.warning("Attempted to extract first entry 'bitm' - this is likely metadata");
        return actualDataStart;
    }

    // Calculate offset by summing size2 of entries 1 through fileIndex-1
    // size2 = compressed data size INCLUDING the 48-byte image header
    std::streamoff offset = actualDataStart;
    for (size_t i = 1; i < fileIndex; i++)
    {
        offset += entries[i].size2;
    }

    return offset;
}

std::optional<std::vector<uint8_t>> ImageLODArchive::extractFile(const std::string& filename)
{
    if (!opened)
    {
        logger.error("Archive not open");
        return std::nullopt;
    }

    // Find entry (case-insensitive search)
    const ImageLODDirectoryEntry* targetEntry = nullptr;
    for (const auto& entry : entries)
    {
        std::string entryName = buildFilename(entry);

        // Case-insensitive comparison
        if (entryName.size() != filename.size())
            continue;

        bool match = true;
        for (size_t i = 0; i < entryName.size(); i++)
        {
            if (std::tolower(entryName[i]) != std::tolower(filename[i]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry)
    {
        logger.error(std::format("File not found in archive: {}", filename));
        return std::nullopt;
    }

    ImageEntryType entryType = detectEntryType(*targetEntry);
    logger.debug(std::format("Extracting file: {} (type: {}, size1: {}, size2: {})", filename,
                             entryType == ImageEntryType::CustomFormat ? "Custom" : "External",
                             targetEntry->size1, targetEntry->size2));

    // Handle External format files (palettes, PCX, etc.) differently
    if (entryType == ImageEntryType::ExternalFormat)
    {
        // For External format, size1 contains the file offset, size2 contains total size
        // The data includes a 48-byte header followed by raw (uncompressed) data
        std::streamoff dataOffset = static_cast<std::streamoff>(targetEntry->size1);
        logger.debug(
            std::format("External file offset: 0x{:X}", static_cast<uint64_t>(dataOffset)));

        file.seekg(dataOffset, std::ios::beg);
        if (!file.good())
        {
            logger.error(
                std::format("Failed to seek to offset 0x{:X}", static_cast<uint64_t>(dataOffset)));
            return std::nullopt;
        }

        // Skip the 48-byte header for External files
        file.seekg(sizeof(ImageFileHeader), std::ios::cur);

        // Read raw data (size2 - header size)
        uint32_t dataSize = targetEntry->size2 - sizeof(ImageFileHeader);
        std::vector<uint8_t> data(dataSize);
        file.read(reinterpret_cast<char*>(data.data()), dataSize);

        if (!file.good())
        {
            logger.error(std::format("Failed to read {} bytes", dataSize));
            return std::nullopt;
        }

        logger.debug(std::format("Read {} raw bytes from External file", data.size()));
        return data;
    }

    // Calculate file offset for Custom format files
    std::streamoff dataOffset = calculateDataOffset(*targetEntry);
    logger.debug(std::format("Calculated data offset: 0x{:X}", static_cast<uint64_t>(dataOffset)));

    // Seek to file header
    file.seekg(dataOffset, std::ios::beg);
    if (!file.good())
    {
        logger.error(
            std::format("Failed to seek to offset 0x{:X}", static_cast<uint64_t>(dataOffset)));
        return std::nullopt;
    }

    // Read 48-byte image header
    ImageFileHeader imgHeader;
    file.read(reinterpret_cast<char*>(&imgHeader), sizeof(ImageFileHeader));

    if (!file.good())
    {
        logger.error("Failed to read image header");
        return std::nullopt;
    }

    // Log image info
    std::string headerName(imgHeader.name, 16);
    headerName = headerName.substr(0, headerName.find('\0'));
    logger.debug(std::format("Image header: name='{}', size={}x{}, decompSize={}", headerName,
                             imgHeader.width, imgHeader.height, imgHeader.decompressedSize));

    // Read compressed data (size2 includes the 48-byte header)
    uint32_t compressedSize = targetEntry->size2 - sizeof(ImageFileHeader);
    std::vector<uint8_t> compressed(compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

    if (!file.good())
    {
        logger.error(std::format("Failed to read {} bytes of compressed data", compressedSize));
        return std::nullopt;
    }

    // Check for zlib magic bytes
    if (compressed.size() < 2 || compressed[0] != 0x78 || compressed[1] != 0x9C)
    {
        logger.warning(
            std::format("File '{}' doesn't have zlib header - may not be compressed", filename));
        // Return raw data if not compressed
        return compressed;
    }

    // Decompress with zlib
    logger.debug(std::format("Decompressing {} bytes -> {} bytes", compressedSize,
                             imgHeader.decompressedSize));

    std::vector<uint8_t> decompressed(imgHeader.decompressedSize);

    // Use zlib uncompress function
    unsigned long destLen = imgHeader.decompressedSize;
    int result = uncompress(decompressed.data(), &destLen, compressed.data(), compressed.size());

    if (result != Z_OK)
    {
        logger.error(std::format("zlib decompression failed: error code {}", result));
        return std::nullopt;
    }

    if (destLen != imgHeader.decompressedSize)
    {
        logger.warning(std::format("Decompressed size mismatch: got {}, expected {}", destLen,
                                   imgHeader.decompressedSize));
        decompressed.resize(destLen);
    }

    logger.debug(
        std::format("Successfully decompressed {} bytes (includes mipmaps)", decompressed.size()));
    return decompressed;
}

std::optional<ImageFileHeader> ImageLODArchive::getFileInfo(const std::string& filename)
{
    if (!opened)
    {
        logger.error("Archive not open");
        return std::nullopt;
    }

    // Find entry (case-insensitive search)
    const ImageLODDirectoryEntry* targetEntry = nullptr;
    for (const auto& entry : entries)
    {
        std::string entryName = buildFilename(entry);

        // Case-insensitive comparison
        if (entryName.size() != filename.size())
            continue;

        bool match = true;
        for (size_t i = 0; i < entryName.size(); i++)
        {
            if (std::tolower(entryName[i]) != std::tolower(filename[i]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry)
    {
        return std::nullopt;
    }

    // External format files don't have the same header structure
    ImageEntryType entryType = detectEntryType(*targetEntry);
    if (entryType == ImageEntryType::ExternalFormat)
    {
        // For External files, return nullopt as they don't have image dimensions
        return std::nullopt;
    }

    // Calculate file offset for Custom format files
    std::streamoff dataOffset = calculateDataOffset(*targetEntry);

    // Seek to file header
    file.seekg(dataOffset, std::ios::beg);
    if (!file.good())
    {
        return std::nullopt;
    }

    // Read 48-byte image header
    ImageFileHeader imgHeader;
    file.read(reinterpret_cast<char*>(&imgHeader), sizeof(ImageFileHeader));

    if (!file.good())
    {
        return std::nullopt;
    }

    return imgHeader;
}

} // namespace runeharbor::formats
