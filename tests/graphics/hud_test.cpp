// SPDX-License-Identifier: MIT
//
// Unit tests for HUD portrait hit-testing (the game<->screen coordinate
// inversion used to select the active party member by clicking a portrait).
// See docs/re/33-active-member-and-targeting.md section 1.4.
#include <catch2/catch_test_macros.hpp>

#include "../../src/ui/hud.hpp"

using namespace runeharbor::ui;

TEST_CASE("HUD portraitAt maps screen clicks to member indices", "[ui][hud]")
{
    HUD hud;

    // No scaling/offset: screen coords == game coords (640x480).
    // Portraits are at game-X [124+72*i, 124+72*i+59], Y [373,452].
    SECTION("1:1 scale hits each portrait center")
    {
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 150, 410) == 0); // member 0 center
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 225, 410) == 1); // member 1 center
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 297, 410) == 2); // member 2 center
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 369, 410) == 3); // member 3 center
    }

    SECTION("clicks outside the portrait row miss")
    {
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 150, 100) == -1); // too high (Y)
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 10, 410) == -1);  // left of bar
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 190, 410) == -1); // gap between 0 and 1
    }

    SECTION("scaled + offset window maps correctly")
    {
        // 2x scale, offset (50, 30): a portrait-0 game point (150,410) maps to
        // screen (50 + 150*2, 30 + 410*2) = (350, 850).
        REQUIRE(hud.portraitAt(2.0f, 50.0f, 30.0f, 350, 850) == 0);
        // member 3 game (369,410) -> screen (50+738, 30+820) = (788, 850)
        REQUIRE(hud.portraitAt(2.0f, 50.0f, 30.0f, 788, 850) == 3);
    }

    SECTION("non-positive scale is safe")
    {
        REQUIRE(hud.portraitAt(0.0f, 0.0f, 0.0f, 150, 410) == -1);
    }
}
