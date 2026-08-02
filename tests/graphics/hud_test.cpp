// SPDX-License-Identifier: MIT
//
// Unit tests for HUD hit-testing and texture lookup behavior.
#include <string>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/game/game_world.hpp"
#include "../../src/graphics/debug_text.hpp"
#include "../../src/graphics/irenderer.hpp"
#include "../../src/ui/hud.hpp"

using namespace runeharbor::ui;

namespace
{

class TestRenderer final : public runeharbor::graphics::IRenderer
{
  public:
    void clear(uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void present() override {}
    void* createTexture(const runeharbor::graphics::Image&) override { return nullptr; }
    void destroyTexture(void*) override {}
    void renderTexture(void*, int, int, int, int) override {}
    void drawRect(int, int, int, int, uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void drawFilledRect(int, int, int, int, uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void renderTexturedPolygon(const std::vector<SDL_Vertex>&, SDL_Texture*) override {}
    SDL_Renderer* getSDLRenderer() override { return nullptr; }
    int getViewportWidth() const override { return 640; }
    int getViewportHeight() const override { return 480; }
};

} // namespace

TEST_CASE("HUD portraitAt maps screen clicks to member indices", "[ui][hud]")
{
    HUD hud;

    // No scaling/offset: screen coords == game coords (640x480). The clickable
    // region is the original's character-select button, not the full portrait
    // art: 31x40 rects at x = 61/177/292/407, y = 424 (MM7-Rel.exe
    // 0x41B8FD-0x41B96E). Centers are therefore (76/192/307/422, 444).
    SECTION("1:1 scale hits each select button center")
    {
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 76, 444) == 0);
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 192, 444) == 1);
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 307, 444) == 2);
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 422, 444) == 3);
    }

    SECTION("clicks outside the select row miss")
    {
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 76, 100) == -1);  // too high (Y)
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 76, 423) == -1);  // just above the row
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 76, 464) == -1);  // just below the row
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 10, 444) == -1);  // left of the first button
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 130, 444) == -1); // gap between 0 and 1
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 500, 444) == -1); // right of the last button
    }

    SECTION("select rect edges are half-open")
    {
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 61, 424) == 0);  // inclusive top-left
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 91, 463) == 0);  // inclusive bottom-right
        REQUIRE(hud.portraitAt(1.0f, 0.0f, 0.0f, 92, 444) == -1); // exclusive right edge
    }

    SECTION("scaled + offset window maps correctly")
    {
        // 2x scale, offset (50, 30): member-0 game point (76,444) maps to
        // screen (50 + 76*2, 30 + 444*2) = (202, 918).
        REQUIRE(hud.portraitAt(2.0f, 50.0f, 30.0f, 202, 918) == 0);
        // member 3 game (422,444) -> screen (50+844, 30+888) = (894, 918)
        REQUIRE(hud.portraitAt(2.0f, 50.0f, 30.0f, 894, 918) == 3);
    }

    SECTION("non-positive scale is safe")
    {
        REQUIRE(hud.portraitAt(0.0f, 0.0f, 0.0f, 76, 444) == -1);
    }
}

TEST_CASE("HUD caches border texture lookups between frames", "[ui][hud]")
{
    HUD hud;
    runeharbor::game::GameWorld world;
    runeharbor::graphics::DebugText debugText;
    TestRenderer renderer;
    std::unordered_map<std::string, int> lookupCounts;
    int textureToken = 0;

    hud.setGameWorld(&world);
    hud.setTextureLookup(
        [&](const std::string& name, int& width, int& height) -> void*
        {
            ++lookupCounts[name];
            width = 1;
            height = 1;
            return &textureToken;
        });

    hud.render(renderer, debugText, 1.0f, 0.0f, 0.0f);
    hud.render(renderer, debugText, 1.0f, 0.0f, 0.0f);

    CHECK(lookupCounts["ib-r-A"] == 1);
    CHECK(lookupCounts["ib-b-A"] == 1);
    CHECK(lookupCounts["ib-t-A"] == 1);
    CHECK(lookupCounts["ib-l-A"] == 1);
}
