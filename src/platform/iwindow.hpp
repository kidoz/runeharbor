// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>

namespace runeharbor::platform
{

struct WindowConfig
{
    std::string title = "RuneHarbor Engine";
    uint32_t width = 800;
    uint32_t height = 600;
    bool fullscreen = false;
    bool resizable = true;
};

class IWindow
{
  public:
    virtual ~IWindow() = default;

    virtual bool initialize(const WindowConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual void processEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual void swapBuffers() = 0;
};

} // namespace runeharbor::platform
