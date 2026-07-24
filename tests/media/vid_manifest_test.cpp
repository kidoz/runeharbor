// SPDX-License-Identifier: MIT
#include <cstdint>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <fstream>

#include "../../src/media/vid_manifest.hpp"

namespace
{
void writeU32(std::ofstream& file, uint32_t value)
{
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeVidEntry(std::ofstream& file, const char* name, uint32_t offset)
{
    char nameBuffer[40] = {};
    for (size_t i = 0; name[i] != '\0' && i < sizeof(nameBuffer) - 1; i++)
    {
        nameBuffer[i] = name[i];
    }

    file.write(nameBuffer, sizeof(nameBuffer));
    writeU32(file, offset);
}
} // namespace

TEST_CASE("VidManifest reads clip names from VID directory", "[vid][manifest]")
{
    const auto path = std::filesystem::temp_directory_path() / "runeharbor_vid_manifest_test.vid";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        REQUIRE(file.is_open());

        writeU32(file, 5);
        writeVidEntry(file, "3DOLOGO.SMK", 224);
        writeVidEntry(file, "INTRO.BIK", 1024);
        writeVidEntry(file, "README.TXT", 2048);
        writeVidEntry(file, "intro.bik", 4096);
        writeVidEntry(file, "INTRO.BIK", 8192);
    }

    runeharbor::media::VidManifest manifest;
    REQUIRE(manifest.load(path));

    const auto& clips = manifest.clips();
    REQUIRE(clips.size() == 3);
    CHECK(clips[0].name == "3DOLOGO.SMK");
    CHECK(clips[1].name == "INTRO.BIK");
    CHECK(clips[2].name == "intro.bik");

    std::filesystem::remove(path);
}
