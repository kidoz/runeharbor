// SPDX-License-Identifier: MIT
#pragma once

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

  private:
    util::ILogger& logger;
    SDL_Window* window = nullptr;
    bool closeRequested = false;
};

} // namespace runeharbor::platform
