// SPDX-License-Identifier: MIT
#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/application.hpp"
#include "engine/ini_config.hpp"
#include "platform/iwindow.hpp"
#include "platform/sdl3/sdl_window.hpp"
#include "util/console_logger.hpp"

int main(int argc, char* argv[])
{
    using namespace runeharbor;

    // Composition root - wire up all dependencies
    auto logger = std::make_unique<util::ConsoleLogger>();
    auto window = std::make_unique<platform::SdlWindow>(*logger);
    auto app = std::make_unique<engine::Application>(*logger, *window);

    std::string dataPathArg;
    std::string mapName;
    bool listMaps = false;
    bool listOutdoorMaps = false;
    bool noAutoMap = false;
    bool showHelp = false;
    bool useDefs = false;
    bool preferOutdoor = true;
    bool forceIndoor = false;
    bool forceOutdoor = false;
    engine::StartupSettings cliSettings;
    bool bootCharCreate = false;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            showHelp = true;
        }
        else if (arg == "--data" && i + 1 < argc)
        {
            dataPathArg = argv[++i];
        }
        else if (arg.rfind("--data=", 0) == 0)
        {
            dataPathArg = arg.substr(std::string("--data=").size());
        }
        else if (arg == "--map" && i + 1 < argc)
        {
            mapName = argv[++i];
        }
        else if (arg == "--list-maps")
        {
            listMaps = true;
        }
        else if (arg == "--list-outdoor")
        {
            listOutdoorMaps = true;
        }
        else if (arg == "--no-auto-map")
        {
            noAutoMap = true;
        }
        else if (arg == "--indoor")
        {
            forceIndoor = true;
        }
        else if (arg == "--outdoor")
        {
            forceOutdoor = true;
        }
        else if (arg == "-nointro" || arg == "--nointro")
        {
            cliSettings.noIntro = true;
        }
        else if (arg == "-nologo" || arg == "--nologo")
        {
            cliSettings.noLogo = true;
        }
        else if (arg == "-nosound" || arg == "--nosound")
        {
            cliSettings.noSound = true;
        }
        else if (arg == "-nowalksound" || arg == "--nowalksound")
        {
            cliSettings.noWalkSound = true;
        }
        else if (arg == "-noanim" || arg == "--noanim")
        {
            cliSettings.noAnim = true;
        }
        else if (arg == "-window" || arg == "--window")
        {
            cliSettings.windowed = true;
        }
        else if (arg == "-usedefs" || arg == "--usedefs")
        {
            useDefs = true;
        }
        else if (arg == "--charcreate")
        {
            bootCharCreate = true;
        }
        else if (dataPathArg.empty())
        {
            dataPathArg = arg;
        }
        else if (mapName.empty())
        {
            mapName = arg;
        }
    }

    if (showHelp)
    {
        logger->info("Usage: runeharbor <path-to-MM7-DATA-folder> [map.blv]");
        logger->info("   or: runeharbor --data <path-to-MM7-DATA-folder> [options]");
        logger->info("Options:");
        logger->info("  --data <path>      Path to MM7 DATA folder");
        logger->info("  --map <name>       Load a specific BLV map");
        logger->info("  --list-maps        List available BLV maps and exit");
        logger->info("  --list-outdoor     List available ODM maps and exit");
        logger->info("  --indoor           Prefer indoor (BLV) auto-load");
        logger->info("  --outdoor          Prefer outdoor (ODM) auto-load");
        logger->info("  --no-auto-map      Skip auto-loading the first BLV map");
        logger->info("  -nointro           Skip intro videos");
        logger->info("  -nologo            Skip logo videos");
        logger->info("  -nosound           Disable sound");
        logger->info("  -nowalksound       Disable walking/step sounds");
        logger->info("  -noanim            Disable animations");
        logger->info("  -window            Run in windowed mode");
        logger->info("  -usedefs           Prefer text definition tables (dev mode)");
        logger->info("  --charcreate       Boot directly to the Create Party screen (dev)");
        logger->info("  -h, --help         Show this help");
        return 0;
    }

    engine::StartupSettings iniSettings;
    std::optional<std::filesystem::path> iniPathLoaded;
    const std::optional<std::filesystem::path> dataPathForIni =
        dataPathArg.empty() ? std::nullopt : std::optional<std::filesystem::path>(dataPathArg);
    std::vector<std::filesystem::path> iniCandidates =
        engine::buildIniSearchCandidates(dataPathForIni, std::filesystem::current_path());

    for (const auto& candidate : iniCandidates)
    {
        if (std::filesystem::exists(candidate))
        {
            iniSettings = engine::loadIniSettings(candidate, *logger);
            iniPathLoaded = candidate;
            break;
        }
    }

    engine::StartupSettings startupSettings =
        engine::mergeStartupSettings(iniSettings, cliSettings);

    if (!iniSettings.startMap.empty())
    {
        logger->info("Applying [outdoor] startmap from startup INI");
        app->setDefaultStartMap(iniSettings.startMap);
    }

    // Configure window
    platform::WindowConfig windowConfig;
    windowConfig.title = "RuneHarbor Engine - LOD Integration Demo";
    windowConfig.width = 800;
    windowConfig.height = 600;
    windowConfig.fullscreen = !startupSettings.windowed;
    windowConfig.resizable = true;
    if (startupSettings.windowX.has_value() && startupSettings.windowY.has_value())
    {
        windowConfig.windowX = startupSettings.windowX;
        windowConfig.windowY = startupSettings.windowY;
    }

    // Initialize window
    if (!app->initialize(windowConfig))
    {
        return 1;
    }

    app->setUseDefsMode(useDefs);
    app->setPreferLowResSprites(startupSettings.resolution == 1);

    engine::BootConfig bootConfig;
    bootConfig.noIntro = startupSettings.noIntro;
    bootConfig.noLogo = startupSettings.noLogo;
    bootConfig.noSound = startupSettings.noSound;
    bootConfig.noWalkSound = startupSettings.noWalkSound;
    bootConfig.noAnim = startupSettings.noAnim;
    bootConfig.mixerChannels = startupSettings.mixerChannels;
    bootConfig.windowed = startupSettings.windowed;
    bootConfig.bootCharCreate = bootCharCreate;
    bootConfig.showFr = startupSettings.showFr;
    bootConfig.noMonster = startupSettings.noMonster;
    bootConfig.noDamage = startupSettings.noDamage;
    bootConfig.noDecoration = startupSettings.noDecoration || startupSettings.noDecorations;
    bootConfig.noSky = startupSettings.noSky;
    bootConfig.noWavyWater = startupSettings.noWavyWater;
    bootConfig.noMist = startupSettings.noMist;
    bootConfig.walkSpeed = startupSettings.walkSpeed;
    bootConfig.partyHeight = startupSettings.partyHeight;
    bootConfig.partyEyeLevel = startupSettings.partyEyeLevel;
    bootConfig.gridBand1 = startupSettings.gridBand1;
    bootConfig.gridBand2 = startupSettings.gridBand2;
    bootConfig.gridBand3 = startupSettings.gridBand3;
    bootConfig.terrainGamma = startupSettings.terrainGamma;
    bootConfig.buildingGamma = startupSettings.buildingGamma;
    bootConfig.distShade = startupSettings.distShade;
    bootConfig.distShadeMist = startupSettings.distShadeMist;
    bootConfig.distMist = startupSettings.distMist;
    bootConfig.skyDayTop = {
        static_cast<uint8_t>(startupSettings.rgbDayTop[0]),
        static_cast<uint8_t>(startupSettings.rgbDayTop[1]),
        static_cast<uint8_t>(startupSettings.rgbDayTop[2]),
    };
    bootConfig.skyDayBottom = {
        static_cast<uint8_t>(startupSettings.rgbDayBottom[0]),
        static_cast<uint8_t>(startupSettings.rgbDayBottom[1]),
        static_cast<uint8_t>(startupSettings.rgbDayBottom[2]),
    };
    bootConfig.skyNightTop = {
        static_cast<uint8_t>(startupSettings.rgbNightTop[0]),
        static_cast<uint8_t>(startupSettings.rgbNightTop[1]),
        static_cast<uint8_t>(startupSettings.rgbNightTop[2]),
    };
    bootConfig.skyNightBottom = {
        static_cast<uint8_t>(startupSettings.rgbNightBottom[0]),
        static_cast<uint8_t>(startupSettings.rgbNightBottom[1]),
        static_cast<uint8_t>(startupSettings.rgbNightBottom[2]),
    };
    bootConfig.viewportX = std::max(0, startupSettings.viewportX);
    bootConfig.viewportY = std::max(0, startupSettings.viewportY);
    bootConfig.viewportWidth = std::max(1, startupSettings.viewportWidth);
    bootConfig.viewportHeight = std::max(1, startupSettings.viewportHeight);

    if (forceIndoor && forceOutdoor)
    {
        logger->warning("--indoor and --outdoor both specified; defaulting to outdoor");
        forceIndoor = false;
    }
    if (forceIndoor)
    {
        preferOutdoor = false;
    }
    if (forceOutdoor)
    {
        preferOutdoor = true;
    }

    // Load game data if path provided
    if (!dataPathArg.empty())
    {
        std::filesystem::path dataPath(dataPathArg);
        logger->info("Game data path provided via command line");

        if (!app->loadGameData(dataPath))
        {
            logger->warning("Failed to load game data - continuing without it");
        }
        else
        {
            if (listMaps)
            {
                auto maps = app->listMaps();
                logger->info("Available BLV maps:");
                for (const auto& name : maps)
                {
                    logger->info("  " + name);
                }
                return 0;
            }

            if (listOutdoorMaps)
            {
                auto maps = app->listOutdoorMaps();
                logger->info("Available ODM maps:");
                for (const auto& name : maps)
                {
                    logger->info("  " + name);
                }
                return 0;
            }

            app->configureBootFlow(mapName, preferOutdoor, !noAutoMap);
        }
    }

    // Apply boot config (must be after loadGameData so intro playlist is built)
    app->setBootConfig(bootConfig);

    if (dataPathArg.empty())
    {
        logger->info("No game data path provided");
        logger->info("Usage: runeharbor <path-to-MM7-DATA-folder> [map.blv]");
        logger->info("Example: runeharbor /path/to/MM7/Data d01.blv");
        logger->info("");
        logger->info("Continuing without game data (window demo only)...");
    }

    // Run main loop
    app->run();

    if (auto pos = window->getWindowPosition(); pos.has_value())
    {
        startupSettings.windowX = pos->x;
        startupSettings.windowY = pos->y;
    }

    std::filesystem::path saveIniPath;
    if (iniPathLoaded.has_value())
    {
        saveIniPath = *iniPathLoaded;
    }
    else if (!iniCandidates.empty())
    {
        saveIniPath = iniCandidates.front();
    }
    else
    {
        saveIniPath = std::filesystem::current_path() / "mm7.ini";
    }

    startupSettings.noIntro = bootConfig.noIntro;
    startupSettings.noLogo = bootConfig.noLogo;
    startupSettings.noSound = bootConfig.noSound;
    startupSettings.noWalkSound = bootConfig.noWalkSound;
    startupSettings.noAnim = bootConfig.noAnim;
    startupSettings.mixerChannels = bootConfig.mixerChannels;
    startupSettings.windowed = bootConfig.windowed;
    startupSettings.showFr = bootConfig.showFr;
    startupSettings.noMonster = bootConfig.noMonster;
    startupSettings.noDamage = bootConfig.noDamage;
    startupSettings.noDecoration = bootConfig.noDecoration;
    startupSettings.noDecorations = bootConfig.noDecoration;
    startupSettings.noSky = bootConfig.noSky;
    startupSettings.noWavyWater = bootConfig.noWavyWater;
    startupSettings.noMist = bootConfig.noMist;
    startupSettings.walkSpeed = bootConfig.walkSpeed;
    startupSettings.partyHeight = bootConfig.partyHeight;
    startupSettings.partyEyeLevel = bootConfig.partyEyeLevel;
    startupSettings.gridBand1 = bootConfig.gridBand1;
    startupSettings.gridBand2 = bootConfig.gridBand2;
    startupSettings.gridBand3 = bootConfig.gridBand3;
    startupSettings.terrainGamma = bootConfig.terrainGamma;
    startupSettings.buildingGamma = bootConfig.buildingGamma;
    startupSettings.distShade = bootConfig.distShade;
    startupSettings.distShadeMist = bootConfig.distShadeMist;
    startupSettings.distMist = bootConfig.distMist;
    startupSettings.rgbDayTop = {
        static_cast<int>(bootConfig.skyDayTop[0]),
        static_cast<int>(bootConfig.skyDayTop[1]),
        static_cast<int>(bootConfig.skyDayTop[2]),
    };
    startupSettings.rgbDayBottom = {
        static_cast<int>(bootConfig.skyDayBottom[0]),
        static_cast<int>(bootConfig.skyDayBottom[1]),
        static_cast<int>(bootConfig.skyDayBottom[2]),
    };
    startupSettings.rgbNightTop = {
        static_cast<int>(bootConfig.skyNightTop[0]),
        static_cast<int>(bootConfig.skyNightTop[1]),
        static_cast<int>(bootConfig.skyNightTop[2]),
    };
    startupSettings.rgbNightBottom = {
        static_cast<int>(bootConfig.skyNightBottom[0]),
        static_cast<int>(bootConfig.skyNightBottom[1]),
        static_cast<int>(bootConfig.skyNightBottom[2]),
    };
    startupSettings.viewportX = std::max(0, bootConfig.viewportX);
    startupSettings.viewportY = std::max(0, bootConfig.viewportY);
    startupSettings.viewportWidth = std::max(1, bootConfig.viewportWidth);
    startupSettings.viewportHeight = std::max(1, bootConfig.viewportHeight);
    if (iniSettings.startMap.empty())
    {
        startupSettings.startMap.clear();
    }
    engine::saveIniSettings(saveIniPath, startupSettings, *logger);

    app->shutdown();

    return 0;
}
