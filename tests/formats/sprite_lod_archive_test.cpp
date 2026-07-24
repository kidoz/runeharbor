// SPDX-License-Identifier: MIT
#include <array>
#include <filesystem>

#include <catch2/catch_all.hpp>
#include <cstring>
#include <fstream>

#include "../../src/formats/sprite_lod_archive.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor;
using namespace runeharbor::formats;
using namespace runeharbor::util;

namespace
{

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

class TestSpriteLODFile
{
  public:
    TestSpriteLODFile()
        : filepath(std::filesystem::temp_directory_path() / "runeharbor_test_sprites.lod")
    {
        createTestFile();
    }

    ~TestSpriteLODFile() { std::filesystem::remove(filepath); }

    std::filesystem::path path() const { return filepath; }

  private:
    void createTestFile()
    {
        constexpr uint32_t kOffsetDelta = 0x1000;
        constexpr uint32_t kSpriteRelativeOffset = 0x40;
        constexpr uint32_t kSpriteAbsoluteOffset = kOffsetDelta + kSpriteRelativeOffset;

        std::ofstream file(filepath, std::ios::binary | std::ios::trunc);

        SpriteLODHeader header = {};
        std::memcpy(header.magic, "LOD\0", 4);
        std::memcpy(header.gameId, "MMVI", 4);
        file.write(reinterpret_cast<const char*>(&header), sizeof(SpriteLODHeader));

        file.seekp(0x100);

        SpriteLODDirectoryEntry metadata = {};
        std::memcpy(metadata.name, "sprites08", 9);
        metadata.offset = kOffsetDelta;
        file.write(reinterpret_cast<const char*>(&metadata), sizeof(SpriteLODDirectoryEntry));

        SpriteLODDirectoryEntry spriteEntry = {};
        std::memcpy(spriteEntry.name, "test_sprite_long", 16);
        spriteEntry.offset = kSpriteRelativeOffset;
        spriteEntry.size = sizeof(SpriteFileHeader) + 8 + 4;
        file.write(reinterpret_cast<const char*>(&spriteEntry), sizeof(SpriteLODDirectoryEntry));

        SpriteLODDirectoryEntry terminator = {};
        file.write(reinterpret_cast<const char*>(&terminator), sizeof(SpriteLODDirectoryEntry));

        file.seekp(kSpriteAbsoluteOffset);
        SpriteFileHeader sprite = {};
        std::memcpy(sprite.name, "test", 4);
        sprite.dataOffset = 0;
        sprite.compressedSize = 4;
        sprite.width = 2;
        sprite.height = 1;
        sprite.paletteId = 7;
        sprite.centerX = 1;
        sprite.centerY = 2;
        sprite.decompressedSize = 4;
        file.write(reinterpret_cast<const char*>(&sprite), sizeof(SpriteFileHeader));

        const std::array<uint8_t, 8> lineInfo = {0, 0, 1, 0, 0, 0, 0, 0};
        file.write(reinterpret_cast<const char*>(lineInfo.data()), lineInfo.size());

        const std::array<uint8_t, 4> pixels = {1, 2, 3, 4};
        file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    }

    std::filesystem::path filepath;
};

} // namespace

TEST_CASE("SpriteLODArchive reads MM7 sprite directory entries", "[sprite_lod]")
{
    TestLogger logger;
    SpriteLODArchive archive(logger);
    TestSpriteLODFile testFile;

    REQUIRE(archive.open(testFile.path()));

    SECTION("Lists 16-byte sprite names after metadata entry")
    {
        auto files = archive.listFiles();
        REQUIRE(files.size() == 2);
        REQUIRE(files[0] == "sprites08");
        REQUIRE(files[1] == "test_sprite_long");
    }

    SECTION("Reads sprite headers using metadata offset delta")
    {
        auto info = archive.getFileInfo("TEST_SPRITE_LONG");
        REQUIRE(info.has_value());
        REQUIRE(info->width == 2);
        REQUIRE(info->height == 1);
        REQUIRE(info->paletteId == 7);
        REQUIRE(info->centerX == 1);
        REQUIRE(info->centerY == 2);
    }

    SECTION("Extracts sprite pixels using metadata offset delta")
    {
        auto data = archive.extractFile("test_sprite_long");
        REQUIRE(data.has_value());
        REQUIRE(data->size() == 22);
        REQUIRE((*data)[0] == 2);
        REQUIRE((*data)[2] == 1);
        REQUIRE((*data)[8] == 7);
        REQUIRE((*data)[18] == 1);
        REQUIRE((*data)[19] == 2);
        REQUIRE((*data)[20] == 3);
        REQUIRE((*data)[21] == 4);
    }
}
