// SPDX-License-Identifier: MIT
#include <algorithm>
#include <filesystem>
#include <vector>

#include <catch2/catch_all.hpp>
#include <cstring>
#include <fstream>
#include <zlib.h>

#include "../../src/formats/image_lod_archive.hpp"
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
    }

    LogLevel lastLevel = LogLevel::Info;
    std::string lastMessage;
};

std::vector<uint8_t> compressBytes(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> compressed(compressBound(data.size()));
    uLongf compressedSize = compressed.size();
    const int result = compress(compressed.data(), &compressedSize, data.data(), data.size());
    REQUIRE(result == Z_OK);
    compressed.resize(compressedSize);
    return compressed;
}

void writeFixedName(char* out, size_t size, std::string_view name)
{
    std::fill(out, out + size, '\0');
    const size_t copySize = std::min(size, name.size());
    std::copy_n(name.data(), copySize, out);
}

class TestImageLODFile
{
  public:
    explicit TestImageLODFile(const std::filesystem::path& path) : path_(path)
    {
        std::vector<uint8_t> pixels = {1, 2, 3, 4};
        std::vector<uint8_t> compressed = compressBytes(pixels);
        std::vector<uint8_t> palette(768);
        palette[3] = 10;
        palette[4] = 20;
        palette[5] = 30;
        std::vector<uint8_t> rawPalette(768);
        rawPalette[0] = 1;
        rawPalette[1] = 2;
        rawPalette[2] = 3;
        rawPalette[765] = 253;
        rawPalette[766] = 254;
        rawPalette[767] = 255;

        ImageLODHeader header{};
        std::memcpy(header.magic, "LOD\0", 4);
        std::memcpy(header.gameId, "MMVI", 4);

        ImageLODDirectoryEntry customEntry{};
        writeFixedName(customEntry.name, sizeof(customEntry.name), "dummy");
        const uint8_t libMarker[8] = {0x4C, 0x00, 0x49, 0x00, 0x42, 0x00, 0x2E, 0x00};
        std::memcpy(customEntry.extensionMarker, libMarker, sizeof(libMarker));
        customEntry.offset = 0x80;
        customEntry.size = 0;

        ImageLODDirectoryEntry externalEntry{};
        writeFixedName(externalEntry.name, sizeof(externalEntry.name), "Hhm3ch");
        externalEntry.offset = 0xE0;
        externalEntry.size =
            static_cast<uint32_t>(sizeof(ImageFileHeader) + compressed.size() + palette.size());

        ImageLODDirectoryEntry paletteEntry{};
        writeFixedName(paletteEntry.name, sizeof(paletteEntry.name), "PAL002");
        paletteEntry.offset = 0x500;
        paletteEntry.size = static_cast<uint32_t>(sizeof(ImageFileHeader) + rawPalette.size());

        ImageFileHeader imageHeader{};
        writeFixedName(imageHeader.name, sizeof(imageHeader.name), "Hhm3ch");
        imageHeader.dataSize = static_cast<uint32_t>(pixels.size());
        imageHeader.compressedSize = static_cast<uint32_t>(compressed.size());
        imageHeader.width = 2;
        imageHeader.height = 2;
        imageHeader.widthLn2 = 1;
        imageHeader.heightLn2 = 1;
        imageHeader.widthMinus1 = 1;
        imageHeader.heightMinus1 = 1;
        imageHeader.paletteId = 1;
        imageHeader.decompressedSize = static_cast<uint32_t>(pixels.size());

        ImageFileHeader paletteHeader{};
        writeFixedName(paletteHeader.name, sizeof(paletteHeader.name), "pal002");

        const size_t imageEnd = 0x120 + externalEntry.offset + externalEntry.size;
        const size_t paletteEnd = 0x120 + paletteEntry.offset + paletteEntry.size;
        std::vector<uint8_t> fileData(std::max(imageEnd, paletteEnd), 0);
        std::memcpy(fileData.data(), &header, sizeof(header));
        std::memcpy(fileData.data() + 0x100, &customEntry, sizeof(customEntry));
        std::memcpy(fileData.data() + 0x120, &externalEntry, sizeof(externalEntry));
        std::memcpy(fileData.data() + 0x140, &paletteEntry, sizeof(paletteEntry));

        const size_t dataOffset = 0x120 + externalEntry.offset;
        std::memcpy(fileData.data() + dataOffset, &imageHeader, sizeof(imageHeader));
        std::memcpy(fileData.data() + dataOffset + sizeof(imageHeader), compressed.data(),
                    compressed.size());
        std::memcpy(fileData.data() + dataOffset + sizeof(imageHeader) + compressed.size(),
                    palette.data(), palette.size());

        const size_t paletteOffset = 0x120 + paletteEntry.offset;
        std::memcpy(fileData.data() + paletteOffset, &paletteHeader, sizeof(paletteHeader));
        std::memcpy(fileData.data() + paletteOffset + sizeof(paletteHeader), rawPalette.data(),
                    rawPalette.size());

        std::ofstream file(path_, std::ios::binary);
        file.write(reinterpret_cast<const char*>(fileData.data()),
                   static_cast<std::streamsize>(fileData.size()));
    }

    ~TestImageLODFile() { std::filesystem::remove(path_); }

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("ImageLODArchive extracts mixed external entries from table-relative offsets",
          "[image_lod]")
{
    const auto path = std::filesystem::temp_directory_path() / "runeharbor_image_lod_test.lod";
    TestImageLODFile testFile(path);
    TestLogger logger;

    ImageLODArchive archive(logger);
    REQUIRE(archive.open(testFile.path()));

    auto info = archive.getFileInfo("Hhm3ch");
    REQUIRE(info);
    CHECK(info->width == 2);
    CHECK(info->height == 2);

    auto data = archive.extractFile("Hhm3ch");
    REQUIRE(data);
    CHECK(*data == std::vector<uint8_t>{1, 2, 3, 4});

    auto palette = archive.extractPalette("Hhm3ch");
    REQUIRE(palette);
    REQUIRE(palette->size() == 768);
    CHECK((*palette)[3] == 10);
    CHECK((*palette)[4] == 20);
    CHECK((*palette)[5] == 30);
}

TEST_CASE("ImageLODArchive extracts raw PAL entries from mixed archives", "[image_lod]")
{
    const auto path =
        std::filesystem::temp_directory_path() / "runeharbor_image_lod_palette_test.lod";
    TestImageLODFile testFile(path);
    TestLogger logger;

    ImageLODArchive archive(logger);
    REQUIRE(archive.open(testFile.path()));

    auto palette = archive.extractFile("PAL002");
    REQUIRE(palette);
    REQUIRE(palette->size() == 768);
    CHECK((*palette)[0] == 1);
    CHECK((*palette)[1] == 2);
    CHECK((*palette)[2] == 3);
    CHECK((*palette)[765] == 253);
    CHECK((*palette)[766] == 254);
    CHECK((*palette)[767] == 255);
}
