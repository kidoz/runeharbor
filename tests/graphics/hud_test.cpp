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
