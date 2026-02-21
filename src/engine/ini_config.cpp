// SPDX-License-Identifier: MIT
#include "ini_config.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fstream>

#include "../util/ilogger.hpp"
#include "../util/string_utils.hpp"

namespace runeharbor::engine
{

namespace
{
using KeyValueMap = std::unordered_map<std::string, std::string>;
using SectionMap = std::unordered_map<std::string, KeyValueMap>;

std::optional<int> parseInt(std::string_view raw)
{
    const std::string trimmed = util::trim(std::string(raw));
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    bool negative = false;
    size_t offset = 0;
    if (trimmed[offset] == '+' || trimmed[offset] == '-')
    {
        negative = (trimmed[offset] == '-');
        offset++;
    }
    if (offset >= trimmed.size())
    {
        return std::nullopt;
    }

    const bool isHex = (trimmed.size() >= offset + 2) && trimmed[offset] == '0' &&
                       (trimmed[offset + 1] == 'x' || trimmed[offset + 1] == 'X');
    if (!isHex)
    {
        int value = 0;
        const char* begin = trimmed.data();
        const char* end = trimmed.data() + trimmed.size();
        auto [ptr, ec] = std::from_chars(begin, end, value);
        if (ec != std::errc{} || ptr != end)
        {
            return std::nullopt;
        }
        return value;
    }

    offset += 2;
    if (offset >= trimmed.size())
    {
        return std::nullopt;
    }

    uint32_t magnitude = 0;
    const char* begin = trimmed.data() + static_cast<std::ptrdiff_t>(offset);
    const char* end = trimmed.data() + static_cast<std::ptrdiff_t>(trimmed.size());
    auto [ptr, ec] = std::from_chars(begin, end, magnitude, 16);
    if (ec != std::errc{} || ptr != end)
    {
        return std::nullopt;
    }

    if (!negative)
    {
        if (magnitude > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }
        return static_cast<int>(magnitude);
    }

    const uint32_t maxNegative =
        static_cast<uint32_t>(std::numeric_limits<int>::max()) + static_cast<uint32_t>(1);
    if (magnitude > maxNegative)
    {
        return std::nullopt;
    }
    if (magnitude == maxNegative)
    {
        return std::numeric_limits<int>::min();
    }
    return -static_cast<int>(magnitude);
}

std::string toLowerCopy(std::string_view raw)
{
    std::string lowered;
    lowered.reserve(raw.size());
    for (char c : raw)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered;
}

bool readBoolSetting(const SectionMap& sections, const std::string& sectionName,
                     const std::string& keyName, bool defaultValue, util::ILogger& logger)
{
    auto sectionIt = sections.find(sectionName);
    if (sectionIt == sections.end())
    {
        return defaultValue;
    }

    auto keyIt = sectionIt->second.find(keyName);
    if (keyIt == sectionIt->second.end())
    {
        return defaultValue;
    }

    auto parsed = parseInt(keyIt->second);
    if (!parsed.has_value())
    {
        logger.warning(std::format("Invalid INI value [{}] {}='{}'; using default {}", sectionName,
                                   keyName, keyIt->second, defaultValue ? 1 : 0));
        return defaultValue;
    }

    return *parsed != 0;
}

std::optional<int> readIntSetting(const SectionMap& sections, const std::string& sectionName,
                                  const std::string& keyName, util::ILogger& logger)
{
    auto sectionIt = sections.find(sectionName);
    if (sectionIt == sections.end())
    {
        return std::nullopt;
    }

    auto keyIt = sectionIt->second.find(keyName);
    if (keyIt == sectionIt->second.end())
    {
        return std::nullopt;
    }

    auto parsed = parseInt(keyIt->second);
    if (!parsed.has_value())
    {
        logger.warning(std::format("Invalid INI value [{}] {}='{}'; ignoring", sectionName, keyName,
                                   keyIt->second));
        return std::nullopt;
    }

    return parsed;
}

std::optional<int> readIntSettingAny(const SectionMap& sections, const std::string& sectionName,
                                     std::initializer_list<std::string_view> keyNames,
                                     util::ILogger& logger)
{
    for (const auto keyName : keyNames)
    {
        if (auto value = readIntSetting(sections, sectionName, std::string(keyName), logger);
            value.has_value())
        {
            return value;
        }
    }
    return std::nullopt;
}

std::string readStringSetting(const SectionMap& sections, const std::string& sectionName,
                              const std::string& keyName, std::string_view defaultValue)
{
    auto sectionIt = sections.find(sectionName);
    if (sectionIt == sections.end())
    {
        return std::string(defaultValue);
    }

    auto keyIt = sectionIt->second.find(keyName);
    if (keyIt == sectionIt->second.end())
    {
        return std::string(defaultValue);
    }

    return keyIt->second;
}

SectionMap parseIniSections(std::string_view iniText)
{
    SectionMap sections;
    std::istringstream stream{std::string(iniText)};
    std::string line;
    std::string currentSection;
    bool firstLine = true;

    while (std::getline(stream, line))
    {
        if (firstLine && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
        {
            line = line.substr(3);
        }
        firstLine = false;

        const std::string trimmed = util::trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
        {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']')
        {
            std::string sectionName = util::trim(trimmed.substr(1, trimmed.size() - 2));
            currentSection = util::toLower(sectionName);
            continue;
        }

        const size_t equalPos = trimmed.find('=');
        if (equalPos == std::string::npos)
        {
            continue;
        }

        std::string key = util::toLower(util::trim(trimmed.substr(0, equalPos)));
        std::string value = util::trim(trimmed.substr(equalPos + 1));

        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                  (value.front() == '\'' && value.back() == '\'')))
        {
            value = value.substr(1, value.size() - 2);
        }

        sections[currentSection][key] = value;
    }

    return sections;
}

void writeBoolSetting(SectionMap& sections, const std::string& sectionName, const std::string& key,
                      bool value)
{
    sections[sectionName][key] = value ? "1" : "0";
}

void writeStringSetting(SectionMap& sections, const std::string& sectionName,
                        const std::string& key, const std::string& value)
{
    sections[sectionName][key] = value;
}

void writeIntSetting(SectionMap& sections, const std::string& sectionName, const std::string& key,
                     int value)
{
    sections[sectionName][key] = std::to_string(value);
}

bool saveIniSections(const std::filesystem::path& iniPath, const SectionMap& sections,
                     util::ILogger& logger)
{
    std::ofstream file(iniPath, std::ios::trunc);
    if (!file.is_open())
    {
        logger.warning(std::format("Failed to write INI file: {}", iniPath.string()));
        return false;
    }

    std::vector<std::string> sectionNames;
    sectionNames.reserve(sections.size());
    for (const auto& [name, _] : sections)
    {
        sectionNames.push_back(name);
    }
    std::sort(sectionNames.begin(), sectionNames.end());

    for (size_t i = 0; i < sectionNames.size(); i++)
    {
        const auto& sectionName = sectionNames[i];
        file << "[" << sectionName << "]\n";

        const auto& kv = sections.at(sectionName);
        std::vector<std::string> keys;
        keys.reserve(kv.size());
        for (const auto& [key, _] : kv)
        {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());

        for (const auto& key : keys)
        {
            file << key << "=" << kv.at(key) << "\n";
        }

        if (i + 1 < sectionNames.size())
        {
            file << "\n";
        }
    }

    if (!file.good())
    {
        logger.warning(std::format("Failed while writing INI file: {}", iniPath.string()));
        return false;
    }
    return true;
}
} // namespace

StartupSettings parseIniSettings(std::string_view iniText, util::ILogger& logger)
{
    StartupSettings settings;
    SectionMap sections = parseIniSections(iniText);

    settings.useCd = readBoolSetting(sections, "settings", "use_cd", settings.useCd, logger);
    settings.useRegistry =
        readBoolSetting(sections, "settings", "registry", settings.useRegistry, logger);
    settings.noIntro = readBoolSetting(sections, "settings", "nointro", settings.noIntro, logger);
    settings.noLogo = readBoolSetting(sections, "settings", "nologo", settings.noLogo, logger);
    settings.noSound = readBoolSetting(sections, "settings", "nosound", settings.noSound, logger);
    settings.noWalkSound =
        readBoolSetting(sections, "settings", "nowalksound", settings.noWalkSound, logger);
    settings.noAnim = readBoolSetting(sections, "settings", "noanim", settings.noAnim, logger);

    if (auto value = readIntSetting(sections, "settings", "resolution", logger); value.has_value())
    {
        settings.resolution = std::max(0, *value);
    }
    if (auto value = readIntSetting(sections, "settings", "mixerchannels", logger);
        value.has_value())
    {
        settings.mixerChannels = std::clamp(*value, 0, 16);
    }
    if (auto value = readIntSetting(sections, "settings", "gammapos", logger); value.has_value())
    {
        settings.gammaPos = std::clamp(*value, 0, 10);
    }

    if (auto value = readIntSetting(sections, "debug", "debug flags", logger); value.has_value())
    {
        const uint32_t flags = static_cast<uint32_t>(std::max(0, *value));
        settings.windowed = (flags & 0x01u) != 0;
        settings.showFr = (flags & 0x02u) != 0;
        settings.noMonster = (flags & 0x04u) != 0;
        settings.noDecoration = (flags & 0x08u) != 0;
        settings.noDamage = (flags & 0x10u) != 0;
    }

    settings.windowed =
        readBoolSetting(sections, "debug", "startinwindow", settings.windowed, logger);
    settings.showFr = readBoolSetting(sections, "debug", "showfr", settings.showFr, logger);
    settings.noMonster =
        readBoolSetting(sections, "debug", "nomonster", settings.noMonster, logger);
    settings.noDamage = readBoolSetting(sections, "debug", "nodamage", settings.noDamage, logger);
    settings.noDecoration =
        readBoolSetting(sections, "debug", "nodecoration", settings.noDecoration, logger);
    settings.noMist = readBoolSetting(sections, "debug", "nomist", settings.noMist, logger);
    if (auto value = readIntSetting(sections, "debug", "walkspeed", logger); value.has_value())
    {
        settings.walkSpeed = std::max(1, *value);
    }
    if (auto value = readIntSetting(sections, "party", "walkspeed", logger); value.has_value())
    {
        settings.walkSpeed = std::max(1, *value);
    }
    if (auto value = readIntSetting(sections, "party", "height", logger); value.has_value())
    {
        settings.partyHeight = std::max(1, *value);
    }
    if (auto value = readIntSetting(sections, "party", "eyelevel", logger); value.has_value())
    {
        settings.partyEyeLevel = std::max(0, *value);
    }

    settings.startMap = readStringSetting(sections, "outdoor", "startmap", "");
    settings.noSky = readBoolSetting(sections, "outdoor", "nosky", settings.noSky, logger);
    settings.noWavyWater =
        readBoolSetting(sections, "outdoor", "nowavywater", settings.noWavyWater, logger);
    auto readColor = [&](std::initializer_list<std::string_view> aliases, int defaultValue) -> int
    {
        if (auto value = readIntSettingAny(sections, "outdoor", aliases, logger); value.has_value())
        {
            return std::clamp(*value, 0, 255);
        }
        return defaultValue;
    };
    settings.rgbDayTop[0] = readColor({"rgbdaytop.r", "rgbdaytop_r"}, settings.rgbDayTop[0]);
    settings.rgbDayTop[1] = readColor({"rgbdaytop.g", "rgbdaytop_g"}, settings.rgbDayTop[1]);
    settings.rgbDayTop[2] = readColor({"rgbdaytop.b", "rgbdaytop_b"}, settings.rgbDayTop[2]);
    settings.rgbDayBottom[0] =
        readColor({"rgbdaybottom.r", "rgbdaybottom_r"}, settings.rgbDayBottom[0]);
    settings.rgbDayBottom[1] =
        readColor({"rgbdaybottom.g", "rgbdaybottom_g"}, settings.rgbDayBottom[1]);
    settings.rgbDayBottom[2] =
        readColor({"rgbdaybottom.b", "rgbdaybottom_b"}, settings.rgbDayBottom[2]);
    settings.rgbNightTop[0] =
        readColor({"rgbnighttop.r", "rgbnighttop_r"}, settings.rgbNightTop[0]);
    settings.rgbNightTop[1] =
        readColor({"rgbnighttop.g", "rgbnighttop_g"}, settings.rgbNightTop[1]);
    settings.rgbNightTop[2] =
        readColor({"rgbnighttop.b", "rgbnighttop_b"}, settings.rgbNightTop[2]);
    settings.rgbNightBottom[0] =
        readColor({"rgbnightbottom.r", "rgbnightbottom_r"}, settings.rgbNightBottom[0]);
    settings.rgbNightBottom[1] =
        readColor({"rgbnightbottom.g", "rgbnightbottom_g"}, settings.rgbNightBottom[1]);
    settings.rgbNightBottom[2] =
        readColor({"rgbnightbottom.b", "rgbnightbottom_b"}, settings.rgbNightBottom[2]);
    if (auto value = readIntSetting(sections, "outdoor", "gridband1", logger); value.has_value())
    {
        settings.gridBand1 = std::max(1, *value);
    }
    if (auto value = readIntSetting(sections, "outdoor", "gridband2", logger); value.has_value())
    {
        settings.gridBand2 = std::max(settings.gridBand1, *value);
    }
    if (auto value = readIntSetting(sections, "outdoor", "gridband3", logger); value.has_value())
    {
        settings.gridBand3 = std::max(settings.gridBand2, *value);
    }
    if (auto value = readIntSetting(sections, "outdoor", "ter_gamma", logger); value.has_value())
    {
        settings.terrainGamma = *value;
    }
    if (auto value = readIntSetting(sections, "outdoor", "bld_gamma", logger); value.has_value())
    {
        settings.buildingGamma = *value;
    }
    if (auto value = readIntSetting(sections, "outdoor", "terrain_subdivpow2", logger);
        value.has_value())
    {
        settings.terrainSubdivPow2 = std::max(0, *value);
    }
    if (auto value = readIntSetting(sections, "outdoor", "terrain_subdivsize", logger);
        value.has_value())
    {
        settings.terrainSubdivSize = std::max(0, *value);
    }
    if (auto value = readIntSetting(sections, "shading", "dist_shade", logger); value.has_value())
    {
        settings.distShade = std::max(0, *value);
    }
    if (auto value = readIntSetting(sections, "shading", "dist_shademist", logger);
        value.has_value())
    {
        settings.distShadeMist = std::max(settings.distShade, *value);
    }
    if (auto value = readIntSetting(sections, "shading", "dist_mist", logger); value.has_value())
    {
        settings.distMist = std::max(settings.distShadeMist, *value);
    }

    settings.noDecorations =
        readBoolSetting(sections, "render", "nodecorations", settings.noDecorations, logger);

    if (auto value = readIntSettingAny(
            sections, "screen", {"viewport x", "view x", "x", "param1", "1", "screen x", "screenx"},
            logger);
        value.has_value())
    {
        settings.viewportX = *value;
    }
    if (auto value = readIntSettingAny(
            sections, "screen", {"viewport y", "view y", "y", "param2", "2", "screen y", "screeny"},
            logger);
        value.has_value())
    {
        settings.viewportY = *value;
    }
    if (auto value = readIntSettingAny(sections, "screen",
                                       {"viewport width", "view width", "width", "w", "param3", "3",
                                        "screen width", "screenw"},
                                       logger);
        value.has_value())
    {
        settings.viewportWidth = std::max(1, *value);
    }
    if (auto value = readIntSettingAny(sections, "screen",
                                       {"viewport height", "view height", "height", "h", "param4",
                                        "4", "screen height", "screenh"},
                                       logger);
        value.has_value())
    {
        settings.viewportHeight = std::max(1, *value);
    }

    settings.windowX = readIntSetting(sections, "screen", "window x", logger);
    settings.windowY = readIntSetting(sections, "screen", "window y", logger);

    return settings;
}

StartupSettings loadIniSettings(const std::filesystem::path& iniPath, util::ILogger& logger)
{
    StartupSettings defaults;

    if (!std::filesystem::exists(iniPath))
    {
        return defaults;
    }

    std::ifstream file(iniPath);
    if (!file.is_open())
    {
        logger.warning(std::format("Failed to open INI file: {}", iniPath.string()));
        return defaults;
    }

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    StartupSettings settings = parseIniSettings(text, logger);

    logger.info(std::format("Loaded startup settings from {}", iniPath.string()));
    return settings;
}

std::vector<std::filesystem::path>
buildIniSearchCandidates(const std::optional<std::filesystem::path>& dataPath,
                         const std::filesystem::path& currentDir)
{
    static constexpr std::array<std::string_view, 2> kIniNames = {"mm7.ini", "mm6.ini"};

    std::vector<std::filesystem::path> candidates;
    std::unordered_set<std::string> seen;

    auto pushCandidate = [&](const std::filesystem::path& path)
    {
        const std::string normalized = path.lexically_normal().string();
        if (normalized.empty() || seen.contains(normalized))
        {
            return;
        }
        seen.insert(normalized);
        candidates.push_back(path);
    };

    auto appendRoot = [&](const std::filesystem::path& root)
    {
        if (root.empty())
        {
            return;
        }
        for (const auto name : kIniNames)
        {
            pushCandidate(root / std::string(name));
        }
    };

    if (dataPath.has_value())
    {
        const std::string leaf = toLowerCopy(dataPath->filename().string());
        if (leaf == "data" && !dataPath->parent_path().empty())
        {
            appendRoot(dataPath->parent_path());
        }
        appendRoot(*dataPath);
    }
    appendRoot(currentDir);

    return candidates;
}

bool saveIniSettings(const std::filesystem::path& iniPath, const StartupSettings& settings,
                     util::ILogger& logger)
{
    SectionMap sections;

    if (std::filesystem::exists(iniPath))
    {
        std::ifstream file(iniPath);
        if (file.is_open())
        {
            std::string text((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
            sections = parseIniSections(text);
        }
    }

    writeBoolSetting(sections, "settings", "use_cd", settings.useCd);
    writeBoolSetting(sections, "settings", "registry", settings.useRegistry);
    writeIntSetting(sections, "settings", "resolution", settings.resolution);
    writeIntSetting(sections, "settings", "mixerchannels", settings.mixerChannels);
    writeBoolSetting(sections, "settings", "nointro", settings.noIntro);
    writeBoolSetting(sections, "settings", "nologo", settings.noLogo);
    writeBoolSetting(sections, "settings", "nosound", settings.noSound);
    writeBoolSetting(sections, "settings", "nowalksound", settings.noWalkSound);
    writeBoolSetting(sections, "settings", "noanim", settings.noAnim);
    writeIntSetting(sections, "settings", "gammapos", settings.gammaPos);

    writeBoolSetting(sections, "debug", "startinwindow", settings.windowed);
    writeBoolSetting(sections, "debug", "showfr", settings.showFr);
    writeBoolSetting(sections, "debug", "nomonster", settings.noMonster);
    writeBoolSetting(sections, "debug", "nodamage", settings.noDamage);
    writeBoolSetting(sections, "debug", "nodecoration", settings.noDecoration);
    int debugFlags = 0;
    if (settings.windowed)
        debugFlags |= 0x01;
    if (settings.showFr)
        debugFlags |= 0x02;
    if (settings.noMonster)
        debugFlags |= 0x04;
    if (settings.noDecoration)
        debugFlags |= 0x08;
    if (settings.noDamage)
        debugFlags |= 0x10;
    writeIntSetting(sections, "debug", "debug flags", debugFlags);
    writeIntSetting(sections, "debug", "walkspeed", settings.walkSpeed);
    writeBoolSetting(sections, "debug", "nomist", settings.noMist);
    writeIntSetting(sections, "party", "walkspeed", settings.walkSpeed);
    writeIntSetting(sections, "party", "height", settings.partyHeight);
    writeIntSetting(sections, "party", "eyelevel", settings.partyEyeLevel);

    if (!settings.startMap.empty())
    {
        writeStringSetting(sections, "outdoor", "startmap", settings.startMap);
    }
    writeBoolSetting(sections, "outdoor", "nosky", settings.noSky);
    writeBoolSetting(sections, "outdoor", "nowavywater", settings.noWavyWater);
    writeIntSetting(sections, "outdoor", "rgbdaytop.r", settings.rgbDayTop[0]);
    writeIntSetting(sections, "outdoor", "rgbdaytop.g", settings.rgbDayTop[1]);
    writeIntSetting(sections, "outdoor", "rgbdaytop.b", settings.rgbDayTop[2]);
    writeIntSetting(sections, "outdoor", "rgbdaybottom.r", settings.rgbDayBottom[0]);
    writeIntSetting(sections, "outdoor", "rgbdaybottom.g", settings.rgbDayBottom[1]);
    writeIntSetting(sections, "outdoor", "rgbdaybottom.b", settings.rgbDayBottom[2]);
    writeIntSetting(sections, "outdoor", "rgbnighttop.r", settings.rgbNightTop[0]);
    writeIntSetting(sections, "outdoor", "rgbnighttop.g", settings.rgbNightTop[1]);
    writeIntSetting(sections, "outdoor", "rgbnighttop.b", settings.rgbNightTop[2]);
    writeIntSetting(sections, "outdoor", "rgbnightbottom.r", settings.rgbNightBottom[0]);
    writeIntSetting(sections, "outdoor", "rgbnightbottom.g", settings.rgbNightBottom[1]);
    writeIntSetting(sections, "outdoor", "rgbnightbottom.b", settings.rgbNightBottom[2]);
    writeIntSetting(sections, "outdoor", "gridband1", settings.gridBand1);
    writeIntSetting(sections, "outdoor", "gridband2", settings.gridBand2);
    writeIntSetting(sections, "outdoor", "gridband3", settings.gridBand3);
    writeIntSetting(sections, "outdoor", "ter_gamma", settings.terrainGamma);
    writeIntSetting(sections, "outdoor", "bld_gamma", settings.buildingGamma);
    writeIntSetting(sections, "outdoor", "terrain_subdivpow2", settings.terrainSubdivPow2);
    writeIntSetting(sections, "outdoor", "terrain_subdivsize", settings.terrainSubdivSize);
    writeIntSetting(sections, "shading", "dist_shade", std::max(0, settings.distShade));
    writeIntSetting(sections, "shading", "dist_shademist",
                    std::max(std::max(0, settings.distShade), settings.distShadeMist));
    writeIntSetting(sections, "shading", "dist_mist",
                    std::max(std::max(std::max(0, settings.distShade), settings.distShadeMist),
                             settings.distMist));

    writeBoolSetting(sections, "render", "nodecorations", settings.noDecorations);

    writeIntSetting(sections, "screen", "viewport x", settings.viewportX);
    writeIntSetting(sections, "screen", "viewport y", settings.viewportY);
    writeIntSetting(sections, "screen", "viewport width", std::max(1, settings.viewportWidth));
    writeIntSetting(sections, "screen", "viewport height", std::max(1, settings.viewportHeight));

    if (settings.windowX.has_value() && settings.windowY.has_value())
    {
        writeStringSetting(sections, "screen", "window x", std::to_string(*settings.windowX));
        writeStringSetting(sections, "screen", "window y", std::to_string(*settings.windowY));
    }

    if (saveIniSections(iniPath, sections, logger))
    {
        logger.info(std::format("Saved startup settings to {}", iniPath.string()));
        return true;
    }

    return false;
}

StartupSettings mergeStartupSettings(const StartupSettings& base, const StartupSettings& overrides)
{
    StartupSettings merged = base;
    merged.noIntro = base.noIntro || overrides.noIntro;
    merged.noLogo = base.noLogo || overrides.noLogo;
    merged.noSound = base.noSound || overrides.noSound;
    merged.noWalkSound = base.noWalkSound || overrides.noWalkSound;
    merged.noAnim = base.noAnim || overrides.noAnim;
    merged.windowed = base.windowed || overrides.windowed;
    if (!overrides.startMap.empty())
    {
        merged.startMap = overrides.startMap;
    }
    if (overrides.windowX.has_value() && overrides.windowY.has_value())
    {
        merged.windowX = overrides.windowX;
        merged.windowY = overrides.windowY;
    }
    return merged;
}

} // namespace runeharbor::engine
