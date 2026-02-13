// SPDX-License-Identifier: MIT
#pragma once

#ifndef SDL_MAIN_HANDLED
#    define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>

#include "../iwindow.hpp"

namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::platform
{

class SdlWindow : public IWindow
{
  public:
    explicit SdlWindow(util::ILogger& logger);
    ~SdlWindow() override;

    bool initialize(const WindowConfig& config) override;
    void shutdown() override;
    void processEvents() override;
    bool shouldClose() const override;
    void swapBuffers() override;
    SDL_Window* getSDLWindow() override { return window; }
    MouseState getMouseState() const override;
    bool wasMouseClicked(MouseButton button = MouseButton::Left) const override;
    bool wasMousePressed(MouseButton button = MouseButton::Left) const override;
    void resetFrameState() override;

  private:
    static constexpr int kMouseButtonCount = 3;

    util::ILogger& logger;
    SDL_Window* window = nullptr;
    bool closeRequested = false;

    // Mouse state
    MouseState mouseState_;
    bool mouseButtonPressed_[kMouseButtonCount] = {false, false, false};
    bool mouseButtonClicked_[kMouseButtonCount] = {false, false, false};
};

} // namespace runeharbor::platform
