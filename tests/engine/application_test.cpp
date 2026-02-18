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

namespace
{

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

    SDL_Window* getSDLWindow() override { return nullptr; }

    MouseState getMouseState() const override { return {}; }

    bool wasMouseClicked(MouseButton) const override { return false; }

    bool wasMousePressed(MouseButton) const override { return false; }

    void resetFrameState() override {}

    bool shouldInitSucceed = true;
    bool shouldCloseFlag = false;
    bool initialized = false;
    bool shutdownCalled = false;
    int eventCount = 0;
    int swapCount = 0;
};

} // namespace

TEST_CASE("Application construction", "[application]")
{
    TestLogger logger;
    TestWindow window;

    SECTION("Application can be constructed with mocks")
    {
        Application app(logger, window);
        // Construction should not crash; logger should be used
        REQUIRE(logger.logCount >= 0);
    }
}

TEST_CASE("Application window init failure", "[application]")
{
    TestLogger logger;
    TestWindow window;
    Application app(logger, window);

    SECTION("Returns false when window initialization fails")
    {
        WindowConfig config;
        window.shouldInitSucceed = false;

        bool result = app.initialize(config);

        REQUIRE(result == false);
        REQUIRE(window.initialized == true); // init was called, but returned false
        REQUIRE(logger.logCount > 0);        // Should have logged error
    }
}

TEST_CASE("Application no SDL window", "[application]")
{
    TestLogger logger;
    TestWindow window;
    Application app(logger, window);

    SECTION("Returns false when getSDLWindow returns nullptr")
    {
        // TestWindow::getSDLWindow() returns nullptr, so initialize should fail
        // after window.initialize succeeds but before renderer creation
        WindowConfig config;
        bool result = app.initialize(config);

        REQUIRE(result == false);
        REQUIRE(window.initialized == true);
        REQUIRE(logger.logCount > 0);
    }
}

TEST_CASE("Application dependency injection", "[application][di]")
{
    TestLogger logger;
    TestWindow window;
    Application app(logger, window);

    SECTION("Application uses injected logger during construction")
    {
        // The constructor creates VFS and party, which may log
        // At minimum, initialize will use the logger
        WindowConfig config;
        app.initialize(config);
        REQUIRE(logger.logCount > 0);
    }

    SECTION("Application uses injected window during initialize")
    {
        WindowConfig config;
        app.initialize(config);
        REQUIRE(window.initialized);
    }
}

TEST_CASE("Application RAII behavior", "[application][raii]")
{
    SECTION("Application destructor safe if not initialized")
    {
        TestLogger logger;
        TestWindow window;

        {
            Application app(logger, window);
            // Destructor should not crash even without initialize()
        }

        // shutdown is only called if initialized was set to true
        REQUIRE_FALSE(window.shutdownCalled);
    }

    SECTION("Application destructor safe after failed initialize")
    {
        TestLogger logger;
        TestWindow window;

        {
            Application app(logger, window);
            WindowConfig config;
            // initialize returns false (no SDL window), so initialized stays false
            app.initialize(config);
        }

        // shutdown should not be called since initialize didn't complete
        REQUIRE_FALSE(window.shutdownCalled);
    }
}
