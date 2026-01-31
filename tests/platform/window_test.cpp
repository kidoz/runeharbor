// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

#include "../../src/platform/iwindow.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::platform;
using namespace runeharbor::util;

// Mock window for testing without SDL
class MockWindow : public IWindow
{
  public:
    explicit MockWindow(ILogger& logger) : initCalled(false), shutdownCalled(false), logger(logger)
    {
    }

    bool initialize(const WindowConfig& config) override
    {
        initCalled = true;
        lastConfig = config;
        logger.debug("MockWindow initialized");
        return shouldSucceed;
    }

    void shutdown() override
    {
        shutdownCalled = true;
        logger.debug("MockWindow shutdown");
    }

    void processEvents() override { eventProcessCount++; }

    bool shouldClose() const override { return closeRequested; }

    void swapBuffers() override { swapCount++; }

    SDL_Window* getSDLWindow() override { return nullptr; }

    // Test control
    bool shouldSucceed = true;
    bool closeRequested = false;
    bool initCalled = false;
    bool shutdownCalled = false;
    int eventProcessCount = 0;
    int swapCount = 0;
    WindowConfig lastConfig;

  private:
    ILogger& logger;
};

// Mock logger that doesn't output
class SilentLogger : public ILogger
{
  public:
    void log(LogLevel, std::string_view) override {}
};

TEST_CASE("WindowConfig structure", "[window][config]")
{
    SECTION("WindowConfig has sensible defaults")
    {
        WindowConfig config;
        REQUIRE(config.title == "RuneHarbor Engine");
        REQUIRE(config.width == 800);
        REQUIRE(config.height == 600);
        REQUIRE(config.fullscreen == false);
        REQUIRE(config.resizable == true);
    }

    SECTION("WindowConfig can be customized")
    {
        WindowConfig config;
        config.title = "Custom Title";
        config.width = 1920;
        config.height = 1080;
        config.fullscreen = true;
        config.resizable = false;

        REQUIRE(config.title == "Custom Title");
        REQUIRE(config.width == 1920);
        REQUIRE(config.height == 1080);
        REQUIRE(config.fullscreen == true);
        REQUIRE(config.resizable == false);
    }
}

TEST_CASE("MockWindow basic functionality", "[window][mock]")
{
    SilentLogger logger;
    MockWindow window(logger);

    SECTION("Window starts uninitialized")
    {
        REQUIRE_FALSE(window.initCalled);
        REQUIRE_FALSE(window.shutdownCalled);
    }

    SECTION("Window can be initialized")
    {
        WindowConfig config;
        config.title = "Test Window";
        config.width = 640;
        config.height = 480;

        bool result = window.initialize(config);

        REQUIRE(result == true);
        REQUIRE(window.initCalled == true);
        REQUIRE(window.lastConfig.title == "Test Window");
        REQUIRE(window.lastConfig.width == 640);
        REQUIRE(window.lastConfig.height == 480);
    }

    SECTION("Window can fail initialization")
    {
        WindowConfig config;
        window.shouldSucceed = false;

        bool result = window.initialize(config);

        REQUIRE(result == false);
        REQUIRE(window.initCalled == true);
    }

    SECTION("Window can be shutdown")
    {
        window.shutdown();
        REQUIRE(window.shutdownCalled == true);
    }

    SECTION("Window processes events")
    {
        REQUIRE(window.eventProcessCount == 0);
        window.processEvents();
        REQUIRE(window.eventProcessCount == 1);
        window.processEvents();
        window.processEvents();
        REQUIRE(window.eventProcessCount == 3);
    }

    SECTION("Window shouldClose returns false by default")
    {
        REQUIRE_FALSE(window.shouldClose());
    }

    SECTION("Window shouldClose can be triggered")
    {
        window.closeRequested = true;
        REQUIRE(window.shouldClose());
    }

    SECTION("Window swapBuffers increments counter")
    {
        REQUIRE(window.swapCount == 0);
        window.swapBuffers();
        REQUIRE(window.swapCount == 1);
        window.swapBuffers();
        window.swapBuffers();
        REQUIRE(window.swapCount == 3);
    }
}

TEST_CASE("IWindow polymorphism", "[window][interface]")
{
    SilentLogger logger;

    SECTION("MockWindow works through IWindow pointer")
    {
        std::unique_ptr<IWindow> window = std::make_unique<MockWindow>(logger);
        WindowConfig config;
        REQUIRE(window->initialize(config));
        REQUIRE_NOTHROW(window->processEvents());
        REQUIRE_NOTHROW(window->shutdown());
    }

    SECTION("MockWindow works through IWindow reference")
    {
        MockWindow mockWindow(logger);
        IWindow& window = mockWindow;
        WindowConfig config;
        REQUIRE(window.initialize(config));
        REQUIRE_FALSE(window.shouldClose());
    }
}
