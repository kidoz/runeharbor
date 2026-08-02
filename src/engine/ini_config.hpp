// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::engine
{

struct StartupSettings
{
    // [settings]
    bool useCd = true;
    bool useRegistry = true;
    int resolution = 0;
    int mixerChannels = 16;
    bool noIntro = false;
    bool noLogo = false;
    bool noSound = false;
    bool noWalkSound = false;
    bool noAnim = false;
    int gammaPos = 4;

    // [debug]
    bool windowed = false;
    bool showFr = false;
    bool noMonster = false;
    bool noDamage = false;
    bool noDecoration = false;
    int walkSpeed = 384;
    bool noMist = false;
    int partyHeight = 192;
    int partyEyeLevel = 160;

    // [outdoor]
    std::string startMap;
    bool noSky = false;
    bool noWavyWater = false;
    std::array<int, 3> rgbDayTop = {81, 121, 236};
    std::array<int, 3> rgbDayBottom = {153, 193, 237};
    std::array<int, 3> rgbNightTop = {0, 0, 0};
    std::array<int, 3> rgbNightBottom = {11, 41, 129};
    int gridBand1 = 10;
    int gridBand2 = 15;
    int gridBand3 = 25;
    int terrainGamma = 0;
    int buildingGamma = 0;
    int distShade = 2048;
    int distShadeMist = 4096;
    int distMist = 8192;
    int terrainSubdivPow2 = 0;
    int terrainSubdivSize = 0;

    // [render]
    bool noDecorations = false;

    // [screen] — stored as an origin plus extent. The original INI expresses
    // this as inclusive edges vx1/vy1/vx2/vy2 = 8/8/468/351, so the extent is
    // 461x344 (see parseIniSettings).
    int viewportX = 8;
    int viewportY = 8;
    int viewportWidth = 461;
    int viewportHeight = 344;
    std::optional<int> windowX;
    std::optional<int> windowY;
};

/// Parse INI text and extract startup-related settings.
StartupSettings parseIniSettings(std::string_view iniText, util::ILogger& logger);

/// Load and parse INI settings from disk. Missing file returns defaults.
StartupSettings loadIniSettings(const std::filesystem::path& iniPath, util::ILogger& logger);

/// Build ordered INI path candidates, preferring mm7.ini then mm6.ini.
std::vector<std::filesystem::path>
buildIniSearchCandidates(const std::optional<std::filesystem::path>& dataPath,
                         const std::filesystem::path& currentDir);

/// Save startup-related settings back to disk.
bool saveIniSettings(const std::filesystem::path& iniPath, const StartupSettings& settings,
                     util::ILogger& logger);

/// Merge additive startup settings; booleans are OR-ed, map name prefers override when non-empty.
StartupSettings mergeStartupSettings(const StartupSettings& base, const StartupSettings& overrides);

} // namespace runeharbor::engine
