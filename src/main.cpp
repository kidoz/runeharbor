// SPDX-License-Identifier: MIT
#include <filesystem>
#include <memory>
#include <string>

#include "engine/application.hpp"
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

    // Configure window
    platform::WindowConfig windowConfig;
    windowConfig.title = "RuneHarbor Engine - LOD Integration Demo";
    windowConfig.width = 800;
    windowConfig.height = 600;
    windowConfig.fullscreen = false;
    windowConfig.resizable = true;

    // Initialize window
    if (!app->initialize(windowConfig))
    {
        return 1;
    }

    std::string dataPathArg;
    std::string mapName;
    bool listMaps = false;
    bool listOutdoorMaps = false;
    bool noAutoMap = false;
    bool showHelp = false;
    bool preferOutdoor = true;
    bool forceIndoor = false;
    bool forceOutdoor = false;

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
        logger->info("  -h, --help         Show this help");
        return 0;
    }

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
    else
    {
        logger->info("No game data path provided");
        logger->info("Usage: runeharbor <path-to-MM7-DATA-folder> [map.blv]");
        logger->info("Example: runeharbor /path/to/MM7/Data d01.blv");
        logger->info("");
        logger->info("Continuing without game data (window demo only)...");
    }

    // Run main loop
    app->run();
    app->shutdown();

    return 0;
}
