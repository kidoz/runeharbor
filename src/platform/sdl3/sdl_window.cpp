// SPDX-License-Identifier: MIT
#include "sdl_window.hpp"

#include "../../util/ilogger.hpp"

namespace runeharbor::platform
{

SdlWindow::SdlWindow(util::ILogger& logger) : logger(logger) {}

SdlWindow::~SdlWindow()
{
    shutdown();
}

bool SdlWindow::initialize(const WindowConfig& config)
{
    logger.debug("Initializing SDL3 window subsystem");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        logger.error("SDL_Init failed");
        const char* sdlError = SDL_GetError();
        if (sdlError && *sdlError)
        {
            logger.error(sdlError);
        }
        return false;
    }

    Uint32 flags = 0;
    if (config.fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (config.resizable)
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    logger.debug("Creating SDL3 window");
    window = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
    if (!window)
    {
        logger.error("SDL_CreateWindow failed");
        const char* sdlError = SDL_GetError();
        if (sdlError && *sdlError)
        {
            logger.error(sdlError);
        }
        SDL_Quit();
        return false;
    }

    closeRequested = false;
    logger.debug("SDL3 window initialized successfully");
    return true;
}

void SdlWindow::shutdown()
{
    if (window)
    {
        logger.debug("Destroying SDL3 window");
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    logger.debug("Shutting down SDL3 subsystem");
    SDL_Quit();
}

void SdlWindow::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            closeRequested = true;
        }
    }
}

bool SdlWindow::shouldClose() const
{
    return closeRequested;
}

void SdlWindow::swapBuffers()
{
    // No-op for now since we're not rendering yet
    // Will use SDL_GL_SwapWindow or similar when we add rendering
}

} // namespace runeharbor::platform
