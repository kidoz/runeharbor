// SPDX-License-Identifier: MIT
#pragma once

#include <memory>

namespace runeharbor::platform
{
class IWindow;
struct WindowConfig;
} // namespace runeharbor::platform

namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::engine
{

class Application
{
  public:
    Application(util::ILogger& logger, platform::IWindow& window);
    ~Application();

    bool initialize(const platform::WindowConfig& windowConfig);
    void run();
    void shutdown();

  private:
    util::ILogger& logger;
    platform::IWindow& window;
    bool initialized = false;
};

} // namespace runeharbor::engine
