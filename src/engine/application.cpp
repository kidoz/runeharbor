// SPDX-License-Identifier: MIT
#include "application.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <vector>

#include <SDL3/SDL.h>

#include "../platform/iwindow.hpp"
#include "../util/ilogger.hpp"
#include "../graphics/sdl_renderer.hpp"
#include "virtual_filesystem.hpp"

namespace runeharbor::engine
{

Application::Application(util::ILogger& logger, platform::IWindow& window)
    : logger(logger), window(window), vfs(std::make_unique<VirtualFileSystem>(logger))
{
}

Application::~Application()
{
    if (initialized)
    {
        shutdown();
    }
}

bool Application::initialize(const platform::WindowConfig& windowConfig)
{
    logger.info("RuneHarbor Engine v0.1.0");
    logger.info("Initializing...");

    if (!window.initialize(windowConfig))
    {
        logger.error("Failed to initialize window");
        return false;
    }

    logger.info("Window created successfully");

    // Create renderer
    SDL_Window* sdlWindow = window.getSDLWindow();
    if (!sdlWindow)
    {
        logger.error("Failed to get SDL window for renderer creation");
        return false;
    }

    renderer = std::make_unique<graphics::SDLRenderer>(sdlWindow, logger);
    logger.info("Renderer created successfully");
    logger.info("Press ESC or close window to exit");

    initialized = true;
    return true;
}

bool Application::loadGameData(const std::filesystem::path& dataPath)
{
    logger.info(std::format("Loading game data from: {}", dataPath.string()));

    if (!std::filesystem::exists(dataPath))
    {
        logger.error(std::format("Game data path does not exist: {}", dataPath.string()));
        return false;
    }

    // Text/data archives
    const std::vector<std::string> textArchives = {
        "Events.lod",
        "GAMES.LOD",
    };

    // Image archives (use different format)
    const std::vector<std::string> imageArchives = {
        "BITMAPS.LOD",
        "ICONS.LOD",
        "SPRITES.LOD",
    };

    size_t mountedCount = 0;

    // Mount text archives
    for (const auto& archiveName : textArchives)
    {
        auto archivePath = dataPath / archiveName;

        // Try both exact case and lowercase
        if (!std::filesystem::exists(archivePath))
        {
            std::string lowerName = archiveName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            archivePath = dataPath / lowerName;
        }

        if (std::filesystem::exists(archivePath))
        {
            if (vfs->mountArchive(archivePath))
            {
                mountedCount++;
            }
        }
        else
        {
            logger.debug(std::format("Text archive not found (skipping): {}", archiveName));
        }
    }

    // Mount image archives
    for (const auto& archiveName : imageArchives)
    {
        auto archivePath = dataPath / archiveName;

        // Try both exact case and lowercase
        if (!std::filesystem::exists(archivePath))
        {
            std::string lowerName = archiveName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            archivePath = dataPath / lowerName;
        }

        if (std::filesystem::exists(archivePath))
        {
            if (vfs->mountImageArchive(archivePath))
            {
                mountedCount++;
            }
        }
        else
        {
            logger.debug(std::format("Image archive not found (skipping): {}", archiveName));
        }
    }

    if (mountedCount == 0)
    {
        logger.error("No LOD archives were mounted");
        return false;
    }

    logger.info(std::format("Successfully mounted {} LOD archive(s)", mountedCount));

    // Demo: list some files
    auto allFiles = vfs->listAllFiles();
    logger.info(std::format("Total files available: {}", allFiles.size()));

    // Demo: read a test file if available
    if (vfs->fileExists("Global.txt"))
    {
        auto data = vfs->readFile("Global.txt");
        if (data.has_value())
        {
            logger.info(std::format("Demo: Successfully read Global.txt ({} bytes)",
                                    data->size()));
        }
    }

    gameDataLoaded = true;
    return true;
}

void Application::run()
{
    if (!initialized)
    {
        logger.error("Cannot run application: not initialized");
        return;
    }

    if (!renderer)
    {
        logger.error("Cannot run application: renderer not initialized");
        return;
    }

    logger.info("Entering main loop...");

    // Main loop
    while (!window.shouldClose())
    {
        window.processEvents();

        // Clear screen (dark blue)
        renderer->clear(20, 30, 60, 255);

        // TODO: Render game content here

        // Present frame
        renderer->present();

        SDL_Delay(16); // ~60 FPS
    }

    logger.info("Exited main loop");
}

void Application::shutdown()
{
    if (!initialized)
    {
        return;
    }

    logger.info("Shutting down...");

    // Unmount all game data
    if (gameDataLoaded && vfs)
    {
        vfs->unmountAll();
        gameDataLoaded = false;
    }

    window.shutdown();
    logger.info("Shutdown complete");

    initialized = false;
}

} // namespace runeharbor::engine
