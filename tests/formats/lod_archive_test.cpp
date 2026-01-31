// SPDX-License-Identifier: MIT
#include "../../src/formats/lod_archive.hpp"
#include "../../src/util/ilogger.hpp"
#include <catch2/catch_all.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>

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

// Helper to create a minimal test LOD file
class TestLODFile
{
public:
    TestLODFile(const std::filesystem::path& path) : filepath(path)
    {
        createTestFile();
    }

    ~TestLODFile()
    {
        std::filesystem::remove(filepath);
    }

    std::filesystem::path path() const
    {
        return filepath;
    }

private:
    void createTestFile()
    {
        std::ofstream file(filepath, std::ios::binary);

        // Write header
        LODHeader header = {};
        std::memcpy(header.magic, "LOD\0", 4);
        std::memcpy(header.gameId, "MMVI", 4);
        file.write(reinterpret_cast<const char*>(&header), sizeof(LODHeader));

        // Write one directory entry at offset 0x100
        file.seekp(0x100);
        LODDirectoryEntry entry = {};
        std::strncpy(entry.filename, "TEST.TXT", 16);
        entry.offset = 0x120; // Right after directory entry
        entry.size = 11;       // "Hello World"
        file.write(reinterpret_cast<const char*>(&entry), sizeof(LODDirectoryEntry));

        // Write null entry to terminate directory
        LODDirectoryEntry nullEntry = {};
        file.write(reinterpret_cast<const char*>(&nullEntry), sizeof(LODDirectoryEntry));

        // Write file data
        file.seekp(entry.offset);
        file.write("Hello World", 11);
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
