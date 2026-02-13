// SPDX-License-Identifier: MIT
#include "sdl_window.hpp"

#include <SDL3/SDL_main.h>

#include <string>

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

    int driverCount = SDL_GetNumVideoDrivers();
    std::string driverList;
    for (int i = 0; i < driverCount; ++i)
    {
        const char* driver = SDL_GetVideoDriver(i);
        if (!driver || !*driver)
        {
            continue;
        }
        if (!driverList.empty())
        {
            driverList += ", ";
        }
        driverList += driver;
    }
    if (driverList.empty())
    {
        driverList = "(none)";
    }
    logger.debug(std::string("Available SDL video drivers: ") + driverList);

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        logger.error("SDL_Init failed");
        const char* sdlError = SDL_GetError();
        std::string errorMessage = (sdlError && *sdlError) ? sdlError : "(empty SDL_GetError)";
        logger.error(std::string("SDL error: ") + errorMessage);
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
        else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
        {
            closeRequested = true;
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            int buttonIndex = -1;
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                buttonIndex = static_cast<int>(MouseButton::Left);
                mouseState_.leftButton = true;
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE)
            {
                buttonIndex = static_cast<int>(MouseButton::Middle);
                mouseState_.middleButton = true;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                buttonIndex = static_cast<int>(MouseButton::Right);
                mouseState_.rightButton = true;
            }

            if (buttonIndex >= 0 && buttonIndex < kMouseButtonCount)
            {
                mouseButtonPressed_[buttonIndex] = true;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            int buttonIndex = -1;
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                buttonIndex = static_cast<int>(MouseButton::Left);
                mouseState_.leftButton = false;
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE)
            {
                buttonIndex = static_cast<int>(MouseButton::Middle);
                mouseState_.middleButton = false;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                buttonIndex = static_cast<int>(MouseButton::Right);
                mouseState_.rightButton = false;
            }

            if (buttonIndex >= 0 && buttonIndex < kMouseButtonCount)
            {
                mouseButtonClicked_[buttonIndex] = true;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            mouseState_.x = static_cast<int>(event.motion.x);
            mouseState_.y = static_cast<int>(event.motion.y);
        }
    }
}

MouseState SdlWindow::getMouseState() const
{
    return mouseState_;
}

bool SdlWindow::wasMouseClicked(MouseButton button) const
{
    int index = static_cast<int>(button);
    if (index >= 0 && index < kMouseButtonCount)
    {
        return mouseButtonClicked_[index];
    }
    return false;
}

bool SdlWindow::wasMousePressed(MouseButton button) const
{
    int index = static_cast<int>(button);
    if (index >= 0 && index < kMouseButtonCount)
    {
        return mouseButtonPressed_[index];
    }
    return false;
}

void SdlWindow::resetFrameState()
{
    for (int i = 0; i < kMouseButtonCount; i++)
    {
        mouseButtonPressed_[i] = false;
        mouseButtonClicked_[i] = false;
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
