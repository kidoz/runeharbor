// SPDX-License-Identifier: MIT
//
// Unit tests for INI parsing of the [screen] viewport, which the original
// expresses as inclusive edges (vx1/vy1/vx2/vy2) rather than an extent.

#include <catch2/catch_test_macros.hpp>

#include "../../src/engine/ini_config.hpp"
#include "../../src/util/console_logger.hpp"

using runeharbor::engine::parseIniSettings;

namespace
{
runeharbor::util::ConsoleLogger& testLogger()
{
    static runeharbor::util::ConsoleLogger logger;
    return logger;
}
} // namespace

TEST_CASE("Viewport defaults match the original's 8/8/468/351 edges", "[engine][ini]")
{
    const auto settings = parseIniSettings("", testLogger());

    CHECK(settings.viewportX == 8);
    CHECK(settings.viewportY == 8);
    // Extent is (vx2 - vx1 + 1) x (vy2 - vy1 + 1), not the raw edge values.
    CHECK(settings.viewportWidth == 461);
    CHECK(settings.viewportHeight == 344);
}

TEST_CASE("vx1/vy1/vx2/vy2 are read as inclusive edges", "[engine][ini]")
{
    SECTION("explicit edges matching the shipped defaults")
    {
        const auto settings =
            parseIniSettings("[screen]\nvx1=8\nvy1=8\nvx2=468\nvy2=351\n", testLogger());
        CHECK(settings.viewportX == 8);
        CHECK(settings.viewportY == 8);
        CHECK(settings.viewportWidth == 461);
        CHECK(settings.viewportHeight == 344);
    }

    SECTION("custom edges convert to the matching extent")
    {
        const auto settings =
            parseIniSettings("[screen]\nvx1=0\nvy1=0\nvx2=639\nvy2=479\n", testLogger());
        CHECK(settings.viewportX == 0);
        CHECK(settings.viewportY == 0);
        CHECK(settings.viewportWidth == 640);
        CHECK(settings.viewportHeight == 480);
    }

    SECTION("origin is applied before the edge is converted")
    {
        const auto settings =
            parseIniSettings("[screen]\nvx1=100\nvy1=50\nvx2=299\nvy2=249\n", testLogger());
        CHECK(settings.viewportX == 100);
        CHECK(settings.viewportY == 50);
        CHECK(settings.viewportWidth == 200);
        CHECK(settings.viewportHeight == 200);
    }
}

TEST_CASE("Width/height aliases still work when no edge keys are present", "[engine][ini]")
{
    const auto settings =
        parseIniSettings("[screen]\nviewport width=320\nviewport height=200\n", testLogger());
    CHECK(settings.viewportWidth == 320);
    CHECK(settings.viewportHeight == 200);
}

TEST_CASE("Degenerate viewports fall back to the defaults", "[engine][ini]")
{
    // vx2 < vx1 would yield a non-positive width; the clamp floors it at 1,
    // which then trips the min-safe check and restores the defaults.
    const auto settings =
        parseIniSettings("[screen]\nvx1=400\nvy1=400\nvx2=10\nvy2=10\n", testLogger());
    CHECK(settings.viewportX == 8);
    CHECK(settings.viewportY == 8);
    CHECK(settings.viewportWidth == 461);
    CHECK(settings.viewportHeight == 344);
}
