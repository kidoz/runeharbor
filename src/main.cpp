// SPDX-License-Identifier: MIT
#include <memory>

#include "engine/application.hpp"
#include "platform/iwindow.hpp"
#include "platform/sdl3/sdl_window.hpp"
#include "util/console_logger.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    using namespace runeharbor;

    // Composition root - wire up all dependencies
    auto logger = std::make_unique<util::ConsoleLogger>();
    auto window = std::make_unique<platform::SdlWindow>(*logger);
    auto app = std::make_unique<engine::Application>(*logger, *window);

    // Configure window
    platform::WindowConfig windowConfig;
    windowConfig.title = "RuneHarbor Engine - Bootstrap";
    windowConfig.width = 800;
    windowConfig.height = 600;
    windowConfig.fullscreen = false;
    windowConfig.resizable = true;

    // Initialize and run
    if (!app->initialize(windowConfig))
    {
        return 1;
    }

    app->run();
    app->shutdown();

    return 0;
}
