// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <memory>
#include <string>

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
class VirtualFileSystem;

class Application
{
  public:
    Application(util::ILogger& logger, platform::IWindow& window);
    ~Application();

    bool initialize(const platform::WindowConfig& windowConfig);

    /// Load game data from specified directory (mounts LOD archives)
    /// Returns true on success, false if no archives were mounted
    bool loadGameData(const std::filesystem::path& dataPath);

    void run();
    void shutdown();

  private:
    util::ILogger& logger;
    platform::IWindow& window;
    std::unique_ptr<VirtualFileSystem> vfs;
    bool initialized = false;
    bool gameDataLoaded = false;
};

} // namespace runeharbor::engine
