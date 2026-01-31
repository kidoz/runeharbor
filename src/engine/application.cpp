// SPDX-License-Identifier: MIT
#include "application.hpp"

#include <SDL3/SDL.h>

#include "../platform/iwindow.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::engine
{

Application::Application(util::ILogger& logger, platform::IWindow& window)
    : logger(logger), window(window)
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
    logger.info("Press ESC or close window to exit");

    initialized = true;
    return true;
}

void Application::run()
{
    if (!initialized)
    {
        logger.error("Cannot run application: not initialized");
        return;
    }

    // Main loop
    while (!window.shouldClose())
    {
        window.processEvents();
        window.swapBuffers();
        SDL_Delay(16); // ~60 FPS
    }
}

void Application::shutdown()
{
    if (!initialized)
    {
        return;
    }

    logger.info("Shutting down...");
    window.shutdown();
    logger.info("Shutdown complete");

    initialized = false;
}

} // namespace runeharbor::engine
