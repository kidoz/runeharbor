// SPDX-License-Identifier: MIT
#include "image_lod_archive.hpp"

#include <algorithm>
#include <format>

#include <cctype>
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

    if (!resolveEntryNames())
    {
        logger.warning("Failed to resolve entry names; continuing with short names");
    }

    opened = true;
    logger.info(std::format("Successfully loaded image LOD archive with {} files ({})",
                            entries.size(), externalOnly ? "external-only" : "mixed"));

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
    resolvedNames.clear();
    dataSectionStart = 0;
    offsetDelta = 0;
    externalOnly = false;
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

    if (std::strncmp(header.magic, "LOD", 3) != 0)
    {
        logger.error(std::format("Invalid LOD magic number: expected 'LOD', got '{}'",
                                 std::string(header.magic, 3)));
        return false;
    }

    std::string gameId(header.gameId, 4);
    logger.debug(std::format("Image LOD archive game ID: {}", gameId));

    return true;
}

bool ImageLODArchive::readDirectory()
{
    constexpr std::streamoff directoryOffset = 0x100;

    file.seekg(directoryOffset, std::ios::beg);

    bool hasLib = false;

    while (file.good())
    {
        std::streamoff entryPos = file.tellg();
        ImageLODDirectoryEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(ImageLODDirectoryEntry));

        if (!file.good())
        {
            break;
        }

        if (entry.name[0] == '\0')
        {
            dataSectionStart = entryPos + 8;
            logger.debug(std::format("End of directory at offset 0x{:X}, data starts at 0x{:X}",
                                     static_cast<uint64_t>(entryPos),
                                     static_cast<uint64_t>(dataSectionStart)));
            break;
        }

        if (detectEntryType(entry) == ImageEntryType::CustomFormat)
        {
            hasLib = true;
        }

        entries.push_back(entry);
    }

    if (entries.empty())
    {
        logger.error("No files found in image LOD archive directory");
        return false;
    }

    externalOnly = !hasLib;

    // For external-only archives (ICONS.LOD), the container entry (index 0)
    // stores the offset delta in its offset field. Each entry's actual data
    // position = entry.offset + delta, pointing to a 48-byte ImageFileHeader
    // followed by compressed pixel data.
    if (externalOnly && !entries.empty())
    {
        offsetDelta = static_cast<int64_t>(entries[0].offset);
        logger.debug(std::format("External-only archive: offsetDelta={} (0x{:X})", offsetDelta,
                                 static_cast<uint64_t>(offsetDelta)));
    }

    logger.debug(std::format("Read {} directory entries", entries.size()));
    return true;
}

ImageEntryType ImageLODArchive::detectEntryType(const ImageLODDirectoryEntry& entry) const
{
    const uint8_t* marker = reinterpret_cast<const uint8_t*>(entry.extensionMarker);

    if (marker[0] == 0x4C && marker[1] == 0x00 && marker[2] == 0x49 && marker[3] == 0x00 &&
        marker[4] == 0x42 && marker[5] == 0x00 && marker[6] == 0x2E && marker[7] == 0x00)
    {
        return ImageEntryType::CustomFormat;
    }

    return ImageEntryType::ExternalFormat;
}

std::string ImageLODArchive::buildFilename(size_t index) const
{
    if (index < resolvedNames.size() && !resolvedNames[index].empty())
    {
        return resolvedNames[index];
    }

    if (index >= entries.size())
    {
        return "";
    }

    const auto& entry = entries[index];
    ImageEntryType entryType = detectEntryType(entry);

    if (entryType == ImageEntryType::CustomFormat)
    {
        // Custom format: name is only the first 8 bytes (extensionMarker is "LIB.")
        std::string filename;
        for (int i = 0; i < 8 && entry.name[i] != '\0'; i++)
        {
            char c = entry.name[i];
            if (c >= 32 && c < 127)
            {
                filename += c;
            }
        }
        return filename;
    }

    // External format: name spans all 16 bytes (name[8] + extensionMarker[8])
    const char* fullName = entry.name; // 8 + 8 = 16 contiguous bytes
    std::string filename;
    for (int i = 0; i < 16; i++)
    {
        char c = fullName[i];
        if (c == '\0')
        {
            break;
        }
        if (c >= 32 && c < 127)
        {
            filename += c;
        }
    }
    return filename;
}

std::vector<std::string> ImageLODArchive::listFiles() const
{
    std::vector<std::string> filenames;
    filenames.reserve(entries.size());

    for (size_t i = 0; i < entries.size(); i++)
    {
        // Skip container entry (index 0) for external-only archives
        if (externalOnly && i == 0)
        {
            continue;
        }
        filenames.push_back(buildFilename(i));
    }

    return filenames;
}

std::streamoff ImageLODArchive::calculateDataOffset(const ImageLODDirectoryEntry& targetEntry) const
{
    // For mixed archives (BITMAPS.LOD): files stored sequentially in data section
    // First entry is metadata; actual files start after it.
    constexpr std::streamoff actualDataStart = 0xCAE0;

    auto it = std::find_if(entries.begin(), entries.end(), [&targetEntry](const auto& e)
                           { return std::strncmp(e.name, targetEntry.name, 8) == 0; });

    if (it == entries.end())
    {
        logger.error("Failed to find entry in directory");
        return actualDataStart;
    }

    size_t fileIndex = std::distance(entries.begin(), it);

    if (fileIndex == 0)
    {
        logger.warning("Attempted to extract first entry - this is likely metadata");
        return actualDataStart;
    }

    std::streamoff fileOffset = actualDataStart;
    for (size_t i = 1; i < fileIndex; i++)
    {
        fileOffset += entries[i].size;
    }

    return fileOffset;
}

std::optional<std::vector<uint8_t>>
ImageLODArchive::extractExternal(const ImageLODDirectoryEntry& entry, const std::string& filename)
{
    // External-only archive format (ICONS.LOD):
    // Actual position = entry.offset + offsetDelta
    // Data layout: [48-byte ImageFileHeader][zlib compressed pixel data]
    // The ImageFileHeader contains name, dimensions, decompressedSize at offset 40.

    std::streamoff actualOffset =
        static_cast<std::streamoff>(entry.offset) + static_cast<std::streamoff>(offsetDelta);

    file.clear();
    file.seekg(actualOffset, std::ios::beg);
    if (!file.good())
    {
        logger.error(std::format("Failed to seek to 0x{:X} for '{}'",
                                 static_cast<uint64_t>(actualOffset), filename));
        return std::nullopt;
    }

    // Read 48-byte image header
    ImageFileHeader imgHeader;
    file.read(reinterpret_cast<char*>(&imgHeader), sizeof(ImageFileHeader));

    if (!file.good())
    {
        logger.error(std::format("Failed to read image header for '{}'", filename));
        return std::nullopt;
    }

    if (entry.size <= sizeof(ImageFileHeader))
    {
        logger.error(std::format("Entry '{}' has invalid size {}", filename, entry.size));
        return std::nullopt;
    }

    uint32_t compressedSize = entry.size - sizeof(ImageFileHeader);
    std::vector<uint8_t> compressed(compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

    if (!file.good())
    {
        logger.error(
            std::format("Failed to read {} compressed bytes for '{}'", compressedSize, filename));
        return std::nullopt;
    }

    uint32_t decompressedSize = imgHeader.decompressedSize;

    // Check for zlib magic
    if (compressed.size() >= 2 && compressed[0] == 0x78 &&
        (compressed[1] == 0x9C || compressed[1] == 0x01 || compressed[1] == 0xDA))
    {
        // Decompress
        std::vector<uint8_t> decompressed(decompressedSize);
        unsigned long destLen = decompressedSize;
        int result =
            uncompress(decompressed.data(), &destLen, compressed.data(), compressed.size());

        if (result != Z_OK)
        {
            logger.error(
                std::format("zlib decompression failed for '{}': error {}", filename, result));
            return std::nullopt;
        }

        if (destLen != decompressedSize)
        {
            decompressed.resize(destLen);
        }

        logger.debug(std::format("Extracted '{}': {}x{}, {} -> {} bytes", filename,
                                 imgHeader.width, imgHeader.height, compressedSize, destLen));
        return decompressed;
    }

    // Not zlib — return raw compressed buffer
    logger.debug(std::format("Extracted '{}': {} bytes (raw)", filename, compressed.size()));
    return compressed;
}

std::optional<std::vector<uint8_t>>
ImageLODArchive::extractCustom(const ImageLODDirectoryEntry& entry, const std::string& filename)
{
    // Mixed archive Custom format (BITMAPS.LOD):
    // Data offset calculated by summing preceding entry sizes
    // Layout: [48-byte ImageFileHeader][zlib compressed data]

    std::streamoff dataOffset = calculateDataOffset(entry);
    logger.debug(std::format("Custom format offset: 0x{:X}", static_cast<uint64_t>(dataOffset)));

    file.clear();
    file.seekg(dataOffset, std::ios::beg);
    if (!file.good())
    {
        logger.error(std::format("Failed to seek to 0x{:X}", static_cast<uint64_t>(dataOffset)));
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

    std::string headerName(imgHeader.name, 16);
    headerName = headerName.substr(0, headerName.find('\0'));
    logger.debug(std::format("Image header: name='{}', size={}x{}, decompSize={}", headerName,
                             imgHeader.width, imgHeader.height, imgHeader.decompressedSize));

    uint32_t compressedSize = entry.size - sizeof(ImageFileHeader);
    std::vector<uint8_t> compressed(compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), compressedSize);

    if (!file.good())
    {
        logger.error(std::format("Failed to read {} bytes of compressed data", compressedSize));
        return std::nullopt;
    }

    // Check for zlib magic
    if (compressed.size() < 2 || compressed[0] != 0x78 || compressed[1] != 0x9C)
    {
        logger.warning(std::format("File '{}' doesn't have zlib header - returning raw", filename));
        return compressed;
    }

    std::vector<uint8_t> decompressed(imgHeader.decompressedSize);
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

    logger.debug(std::format("Decompressed '{}': {} bytes", filename, decompressed.size()));
    return decompressed;
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
    for (size_t i = 0; i < entries.size(); i++)
    {
        // Skip container entry for external-only archives
        if (externalOnly && i == 0)
        {
            continue;
        }

        std::string entryName = buildFilename(i);

        if (entryName.size() != filename.size())
        {
            continue;
        }

        bool match = true;
        for (size_t j = 0; j < entryName.size(); j++)
        {
            if (std::tolower(static_cast<unsigned char>(entryName[j])) !=
                std::tolower(static_cast<unsigned char>(filename[j])))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            targetEntry = &entries[i];
            break;
        }
    }

    if (!targetEntry)
    {
        return std::nullopt;
    }

    ImageEntryType entryType = detectEntryType(*targetEntry);
    logger.debug(std::format("Extracting '{}' from {} (type: {}, offset: {}, size: {})", filename,
                             archivePath.filename().string(),
                             entryType == ImageEntryType::CustomFormat ? "Custom" : "External",
                             targetEntry->offset, targetEntry->size));

    // External-only archives (ICONS.LOD): all entries use delta + 8-byte prefix + zlib
    if (externalOnly)
    {
        return extractExternal(*targetEntry, filename);
    }

    // Mixed archives (BITMAPS.LOD): Custom entries use 48-byte header + zlib
    if (entryType == ImageEntryType::CustomFormat)
    {
        return extractCustom(*targetEntry, filename);
    }

    // Mixed archive External format: use delta + 8-byte prefix + zlib
    // (same approach as external-only but for individual entries in mixed archives)
    return extractExternal(*targetEntry, filename);
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
    for (size_t i = 0; i < entries.size(); i++)
    {
        std::string entryName = buildFilename(i);

        if (entryName.size() != filename.size())
        {
            continue;
        }

        bool match = true;
        for (size_t j = 0; j < entryName.size(); j++)
        {
            if (std::tolower(static_cast<unsigned char>(entryName[j])) !=
                std::tolower(static_cast<unsigned char>(filename[j])))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            targetEntry = &entries[i];
            break;
        }
    }

    if (!targetEntry)
    {
        return std::nullopt;
    }

    ImageEntryType entryType = detectEntryType(*targetEntry);

    // For external-only archives, read the 48-byte ImageFileHeader directly.
    if (externalOnly)
    {
        std::streamoff actualOffset = static_cast<std::streamoff>(targetEntry->offset) +
                                      static_cast<std::streamoff>(offsetDelta);

        file.clear();
        file.seekg(actualOffset, std::ios::beg);
        if (!file.good())
        {
            return std::nullopt;
        }

        ImageFileHeader imgHeader;
        file.read(reinterpret_cast<char*>(&imgHeader), sizeof(ImageFileHeader));
        if (!file.good())
        {
            return std::nullopt;
        }

        return imgHeader;
    }

    // For mixed archives with Custom format, read the 48-byte header
    std::streamoff dataOffset;
    if (entryType == ImageEntryType::ExternalFormat)
    {
        dataOffset = static_cast<std::streamoff>(targetEntry->offset) +
                     static_cast<std::streamoff>(offsetDelta);
    }
    else
    {
        dataOffset = calculateDataOffset(*targetEntry);
    }

    file.clear();
    file.seekg(dataOffset, std::ios::beg);
    if (!file.good())
    {
        return std::nullopt;
    }

    ImageFileHeader imgHeader;
    file.read(reinterpret_cast<char*>(&imgHeader), sizeof(ImageFileHeader));
    if (!file.good())
    {
        return std::nullopt;
    }

    return imgHeader;
}

bool ImageLODArchive::resolveEntryNames()
{
    resolvedNames.clear();
    resolvedNames.reserve(entries.size());

    for (size_t i = 0; i < entries.size(); i++)
    {
        resolvedNames.push_back(buildFilename(i));
    }

    return true;
}

} // namespace runeharbor::formats