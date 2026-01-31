// SPDX-License-Identifier: MIT
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/engine/application.hpp"
#include "../../src/platform/iwindow.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::engine;
using namespace runeharbor::platform;
using namespace runeharbor::util;

// Test mocks
class TestLogger : public ILogger
{
  public:
    void log(LogLevel, std::string_view) override { logCount++; }

    int logCount = 0;
};

class TestWindow : public IWindow
{
  public:
    bool initialize(const WindowConfig&) override
    {
        initialized = true;
        return shouldInitSucceed;
    }

    void shutdown() override { shutdownCalled = true; }

    void processEvents() override { eventCount++; }

    bool shouldClose() const override { return shouldCloseFlag; }

    void swapBuffers() override { swapCount++; }

    bool shouldInitSucceed = true;
    bool shouldCloseFlag = false;
    bool initialized = false;
    bool shutdownCalled = false;
    int eventCount = 0;
    int swapCount = 0;
};

TEST_CASE("Application basic functionality", "[application]")
{
    TestLogger logger;
    TestWindow window;
    Application app(logger, window);

    SECTION("Application initializes successfully")
    {
        WindowConfig config;
        config.title = "Test App";

        bool result = app.initialize(config);

        REQUIRE(result == true);
        REQUIRE(window.initialized == true);
        REQUIRE(logger.logCount > 0);
    }

    SECTION("Application handles window initialization failure")
    {
        WindowConfig config;
        window.shouldInitSucceed = false;

        bool result = app.initialize(config);

        REQUIRE(result == false);
        REQUIRE(window.initialized == true); // init was called, but returned false
        REQUIRE(logger.logCount > 0);        // Should have logged error
    }

    SECTION("Application shutdown cleans up properly")
    {
        WindowConfig config;
        app.initialize(config);

        app.shutdown();

        REQUIRE(window.shutdownCalled == true);
    }
}

TEST_CASE("Application dependency injection", "[application][di]")
{
    TestLogger logger;
    TestWindow window;
    Application app(logger, window);

    SECTION("Application uses injected logger")
    {
        WindowConfig config;
        app.initialize(config);

        REQUIRE(logger.logCount > 0);
    }

    SECTION("Application uses injected window")
    {
        WindowConfig config;
        app.initialize(config);

        REQUIRE(window.initialized);
    }
}

TEST_CASE("Application RAII behavior", "[application][raii]")
{
    SECTION("Application destructor calls shutdown if initialized")
    {
        TestLogger logger;
        TestWindow window;

        {
            Application app(logger, window);
            WindowConfig config;
            app.initialize(config);
            // Destructor should call shutdown
        }

        REQUIRE(window.shutdownCalled);
    }

    SECTION("Application destructor safe if not initialized")
    {
        TestLogger logger;
        TestWindow window;

        {
            Application app(logger, window);
            // Destructor should not crash
        }

        REQUIRE_FALSE(window.initialized);
    }
}
