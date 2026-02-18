// SPDX-License-Identifier: MIT
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../../src/util/console_logger.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::util;

namespace
{

// Mock logger for testing that captures output
class TestLogger : public ILogger
{
  public:
    void log(LogLevel level, std::string_view message) override
    {
        lastLevel = level;
        lastMessage = std::string(message);
        logCount++;
    }

    LogLevel lastLevel = LogLevel::Info;
    std::string lastMessage;
    int logCount = 0;
};

} // namespace

TEST_CASE("ILogger interface methods", "[logger]")
{
    TestLogger logger;

    SECTION("debug() calls log with Debug level")
    {
        logger.debug("test message");
        REQUIRE(logger.lastLevel == LogLevel::Debug);
        REQUIRE(logger.lastMessage == "test message");
        REQUIRE(logger.logCount == 1);
    }

    SECTION("info() calls log with Info level")
    {
        logger.info("info message");
        REQUIRE(logger.lastLevel == LogLevel::Info);
        REQUIRE(logger.lastMessage == "info message");
        REQUIRE(logger.logCount == 1);
    }

    SECTION("warning() calls log with Warning level")
    {
        logger.warning("warning message");
        REQUIRE(logger.lastLevel == LogLevel::Warning);
        REQUIRE(logger.lastMessage == "warning message");
        REQUIRE(logger.logCount == 1);
    }

    SECTION("error() calls log with Error level")
    {
        logger.error("error message");
        REQUIRE(logger.lastLevel == LogLevel::Error);
        REQUIRE(logger.lastMessage == "error message");
        REQUIRE(logger.logCount == 1);
    }

    SECTION("critical() calls log with Critical level")
    {
        logger.critical("critical message");
        REQUIRE(logger.lastLevel == LogLevel::Critical);
        REQUIRE(logger.lastMessage == "critical message");
        REQUIRE(logger.logCount == 1);
    }

    SECTION("multiple log calls increment count")
    {
        logger.info("first");
        logger.info("second");
        logger.info("third");
        REQUIRE(logger.logCount == 3);
        REQUIRE(logger.lastMessage == "third");
    }
}

TEST_CASE("ConsoleLogger instantiation", "[logger][console]")
{
    SECTION("ConsoleLogger can be created and used")
    {
        ConsoleLogger logger;
        // This will output to console, but shouldn't crash
        REQUIRE_NOTHROW(logger.info("Test message"));
        REQUIRE_NOTHROW(logger.debug("Debug message"));
        REQUIRE_NOTHROW(logger.warning("Warning message"));
        REQUIRE_NOTHROW(logger.error("Error message"));
        REQUIRE_NOTHROW(logger.critical("Critical message"));
    }
}

TEST_CASE("ConsoleLogger polymorphism", "[logger][console]")
{
    SECTION("ConsoleLogger works through ILogger pointer")
    {
        std::unique_ptr<ILogger> logger = std::make_unique<ConsoleLogger>();
        REQUIRE_NOTHROW(logger->info("Polymorphic call"));
    }

    SECTION("ConsoleLogger works through ILogger reference")
    {
        ConsoleLogger concreteLogger;
        ILogger& logger = concreteLogger;
        REQUIRE_NOTHROW(logger.warning("Reference call"));
    }
}
