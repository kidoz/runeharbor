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

    // Load game data if path provided
    if (argc >= 2)
    {
        std::filesystem::path dataPath(argv[1]);
        logger->info("Game data path provided via command line");

        if (!app->loadGameData(dataPath))
        {
            logger->warning("Failed to load game data - continuing without it");
        }
    }
    else
    {
        logger->info("No game data path provided");
        logger->info("Usage: runeharbor <path-to-MM7-DATA-folder>");
        logger->info("Example: runeharbor /path/to/MM7/Data");
        logger->info("");
        logger->info("Continuing without game data (window demo only)...");
    }

    // Run main loop
    app->run();
    app->shutdown();

    return 0;
}
