// SPDX-License-Identifier: MIT
#include <filesystem>

#include <catch2/catch_all.hpp>
#include <cstring>
#include <fstream>
#include <zlib.h>

#include "../../src/formats/lod_archive.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor;
using namespace runeharbor::formats;
using namespace runeharbor::util;

// Mock logger for testing
class TestLogger : public ILogger
{
  public:
    void log(LogLevel level, std::string_view message) override
    {
        lastLevel = level;
        lastMessage = std::string(message);
        logCount++;
    }

    LogLevel lastLevel = LogLevel::Info;
    std::string lastMessage;
    int logCount = 0;
};

// Helper to compress data with zlib
static std::vector<uint8_t> compressData(const std::string& data)
{
    std::vector<uint8_t> compressed(compressBound(data.size()));
    uLongf compressedSize = compressed.size();

    int result = compress(compressed.data(), &compressedSize,
                          reinterpret_cast<const Bytef*>(data.data()), data.size());

    if (result != Z_OK)
    {
        return {};
    }

    compressed.resize(compressedSize);
    return compressed;
}

// Helper to create a minimal test LOD file matching the real MM7 format
class TestLODFile
{
  public:
    TestLODFile(const std::filesystem::path& path) : filepath(path) { createTestFile(); }

    ~TestLODFile() { std::filesystem::remove(filepath); }

    std::filesystem::path path() const { return filepath; }

  private:
    void createTestFile()
    {
        std::ofstream file(filepath, std::ios::binary);

        // Compress test data
        std::string testData = "Hello World";
        auto compressed = compressData(testData);

        // Write header (256 bytes)
        LODHeader header = {};
        std::memcpy(header.magic, "LOD\0", 4);
        std::memcpy(header.gameId, "MMVI", 4);
        file.write(reinterpret_cast<const char*>(&header), sizeof(LODHeader));

        // Write one directory entry at offset 0x100
        // The 'size' field contains the compressed data size
        // The 'offset' field is used as a sort key (not actual position)
        file.seekp(0x100);
        LODDirectoryEntry entry = {};
        std::strncpy(entry.filename, "TEST.TXT", 16);
        entry.offset = 0; // Sort key (first file)
        entry.size = static_cast<uint32_t>(compressed.size());
        file.write(reinterpret_cast<const char*>(&entry), sizeof(LODDirectoryEntry));

        // Write null entry to terminate directory (only need 8 zero bytes)
        // The implementation reads 32 bytes but checks only filename[0]
        LODDirectoryEntry nullEntry = {};
        file.write(reinterpret_cast<const char*>(&nullEntry), sizeof(LODDirectoryEntry));

        // Data section starts at null entry position + 8 bytes
        // For file #0: [8-byte header][compressed data]
        // Current position after null entry: 0x100 + 32 + 32 = 0x140
        // Data section starts at: 0x120 + 8 = 0x128

        // Write data at the correct position (dataSectionStart = 0x128)
        file.seekp(0x128);

        // Write 8-byte header: [uncompressed size (4 bytes)][unknown (4 bytes)]
        uint32_t uncompressedSize = static_cast<uint32_t>(testData.size());
        uint32_t unknown = 0;
        file.write(reinterpret_cast<const char*>(&uncompressedSize), 4);
        file.write(reinterpret_cast<const char*>(&unknown), 4);

        // Write compressed data
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    }

    std::filesystem::path filepath;
};

TEST_CASE("LODArchive construction", "[lod]")
{
    TestLogger logger;
    LODArchive archive(logger);

    SECTION("Archive is not open after construction")
    {
        REQUIRE_FALSE(archive.isOpen());
    }
}

TEST_CASE("LODArchive opening", "[lod]")
{
    TestLogger logger;
    LODArchive archive(logger);
    TestLODFile testFile("test_archive.lod");

    SECTION("Can open valid LOD file")
    {
        REQUIRE(archive.open(testFile.path()));
        REQUIRE(archive.isOpen());
    }

    SECTION("Cannot open non-existent file")
    {
        REQUIRE_FALSE(archive.open("nonexistent.lod"));
        REQUIRE_FALSE(archive.isOpen());
    }

    SECTION("Logs error for non-existent file")
    {
        archive.open("nonexistent.lod");
        REQUIRE(logger.lastLevel == LogLevel::Error);
        REQUIRE(logger.lastMessage.find("Failed to open") != std::string::npos);
    }
}

TEST_CASE("LODArchive file listing", "[lod]")
{
    TestLogger logger;
    LODArchive archive(logger);
    TestLODFile testFile("test_archive.lod");

    REQUIRE(archive.open(testFile.path()));

    SECTION("Lists files in archive")
    {
        auto files = archive.listFiles();
        REQUIRE(files.size() == 1);
        REQUIRE(files[0] == "TEST.TXT");
    }
}

TEST_CASE("LODArchive file extraction", "[lod]")
{
    TestLogger logger;
    LODArchive archive(logger);
    TestLODFile testFile("test_archive.lod");

    REQUIRE(archive.open(testFile.path()));

    SECTION("Can extract existing file")
    {
        auto data = archive.extractFile("TEST.TXT");
        REQUIRE(data.has_value());
        REQUIRE(data->size() == 11);

        std::string content(data->begin(), data->end());
        REQUIRE(content == "Hello World");
    }

    SECTION("Extraction is case-insensitive")
    {
        auto data = archive.extractFile("test.txt");
        REQUIRE(data.has_value());
        REQUIRE(data->size() == 11);
    }

    SECTION("Returns nullopt for non-existent file")
    {
        auto data = archive.extractFile("MISSING.TXT");
        REQUIRE_FALSE(data.has_value());
    }

    SECTION("Logs error for non-existent file")
    {
        logger.logCount = 0;
        archive.extractFile("MISSING.TXT");
        REQUIRE(logger.lastLevel == LogLevel::Error);
        REQUIRE(logger.lastMessage.find("not found") != std::string::npos);
    }
}

TEST_CASE("LODArchive closing", "[lod]")
{
    TestLogger logger;
    LODArchive archive(logger);
    TestLODFile testFile("test_archive.lod");

    archive.open(testFile.path());
    REQUIRE(archive.isOpen());

    SECTION("Close makes archive not open")
    {
        archive.close();
        REQUIRE_FALSE(archive.isOpen());
    }

    SECTION("Can safely close multiple times")
    {
        archive.close();
        archive.close();
        REQUIRE_FALSE(archive.isOpen());
    }
}

TEST_CASE("LODArchive real file test", "[lod][integration]")
{
    TestLogger logger;
    LODArchive archive(logger);

    // Test with real Events.lod if it exists
    std::filesystem::path eventsLod = "tmp/DATA/Events.lod";

    if (std::filesystem::exists(eventsLod))
    {
        SECTION("Can open real Events.lod")
        {
            REQUIRE(archive.open(eventsLod));
            REQUIRE(archive.isOpen());

            auto files = archive.listFiles();
            REQUIRE(files.size() > 0);

            INFO("Events.lod contains " << files.size() << " files");
        }

        SECTION("Can extract 2DEvents.txt from Events.lod")
        {
            REQUIRE(archive.open(eventsLod));

            auto data = archive.extractFile("2DEvents.txt");
            REQUIRE(data.has_value());
            REQUIRE(data->size() > 0);

            INFO("Extracted 2DEvents.txt: " << data->size() << " bytes");
        }
    }
    else
    {
        WARN("Skipping real file test - tmp/DATA/Events.lod not found");
    }
}
