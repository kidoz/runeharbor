// SPDX-License-Identifier: MIT

#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "../../src/formats/odm_map.hpp"
#include "../../src/graphics/outdoor_renderer.hpp"

using namespace runeharbor;
using namespace runeharbor::graphics;

TEST_CASE("OutdoorRendererMath - calcHorizonY", "[outdoor_renderer]")
{
    float viewportHeight = 1080.0f;
    float fovY = std::numbers::pi_v<float> / 3.0f; // 60 degrees

    SECTION("Pitch 0 - horizon is exactly in the middle")
    {
        float horizonY = detail::calcHorizonY(viewportHeight, fovY, 0.0f);
        REQUIRE_THAT(horizonY, Catch::Matchers::WithinAbs(540.0f, 0.1f));
    }

    SECTION("Pitch positive (looking up) - horizon goes down, revealing more sky")
    {
        float pitch = std::numbers::pi_v<float> / 6.0f; // 30 degrees up
        float horizonY = detail::calcHorizonY(viewportHeight, fovY, pitch);
        // tan(30) = 0.577, viewPlaneDist = 540 / tan(30) = 935.3
        // 540 + 935.3 * 0.577 = 1080
        REQUIRE_THAT(horizonY, Catch::Matchers::WithinAbs(1080.0f, 1.0f));
    }

    SECTION("Pitch negative (looking down) - horizon goes up, revealing less sky")
    {
        float pitch = -std::numbers::pi_v<float> / 6.0f; // 30 degrees down
        float horizonY = detail::calcHorizonY(viewportHeight, fovY, pitch);
        REQUIRE_THAT(horizonY, Catch::Matchers::WithinAbs(0.0f, 1.0f));
    }
}

TEST_CASE("OutdoorRendererMath - applyOutdoorLighting", "[outdoor_renderer]")
{
    detail::OutdoorLightingParams params;
    params.shadeStart = 100.0f;
    params.shadeMistStart = 200.0f;
    params.mistFull = 300.0f;
    params.mistColor = {0.5f, 0.5f, 0.5f, 1.0f}; // Grey mist
    params.gammaScale = 1.0f;
    params.ambientScale = 1.0f;

    SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};

    SECTION("Distance within shadeStart - no dimming, no mist")
    {
        SDL_FColor c = detail::applyOutdoorLighting(white, 50.0f, params);
        REQUIRE(c.r == 1.0f);
        REQUIRE(c.g == 1.0f);
        REQUIRE(c.b == 1.0f);
    }

    SECTION("Distance between shadeStart and shadeMistStart - dims slightly, no mist")
    {
        SDL_FColor c = detail::applyOutdoorLighting(white, 150.0f, params);
        // Midpoint -> uDim approx 15
        // shadeFactor = 1.0 - (15/31) * 0.65 = 0.685
        REQUIRE(c.r < 1.0f);
        REQUIRE(c.r > 0.5f);
        REQUIRE(c.r == c.g); // Uniform dimming
    }

    SECTION("Distance past mistFull - max dimming, full mist color")
    {
        SDL_FColor c = detail::applyOutdoorLighting(white, 400.0f, params);
        // Fully misted, should return exactly the mist color
        REQUIRE_THAT(c.r, Catch::Matchers::WithinAbs(0.5f, 0.01f));
        REQUIRE_THAT(c.g, Catch::Matchers::WithinAbs(0.5f, 0.01f));
        REQUIRE_THAT(c.b, Catch::Matchers::WithinAbs(0.5f, 0.01f));
    }
}

TEST_CASE("OutdoorRendererMath - makeOutdoorSpawnBillboard octant", "[outdoor_renderer]")
{
    formats::ODMSpawnPoint spawn = {};
    spawn.x = 0;
    spawn.y = 0;
    spawn.z = 100;
    spawn.objectType = 1;
    spawn.objectIndex = 0; // heading 0 in MM7 turn units

    // gameplayToRenderPosition maps x->x, y->z, z->y, so the sprite's render
    // position is (0, 100, 0).
    OutdoorRenderer::MonsterSpriteLookup lookup = [](uint16_t) { return "Goblin"; };

    SECTION("Camera directly south of sprite uses the first frame")
    {
        const Vec3 camPos = {0.0f, 0.0f, -100.0f};

        detail::SpawnBillboard b =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr);
        CHECK(b.textureName == "Goblin01");
        CHECK_FALSE(b.flipU);
    }

    SECTION("Camera directly east of sprite rotates two octants around")
    {
        const Vec3 camPos = {100.0f, 0.0f, 0.0f};

        detail::SpawnBillboard b =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr);
        CHECK(b.textureName == "Goblin03");
        CHECK_FALSE(b.flipU);
    }

    SECTION("Camera on the mirrored side reuses a flipped frame")
    {
        const Vec3 camPos = {-100.0f, 0.0f, 0.0f};

        detail::SpawnBillboard b =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr);
        // MM7 only stores five of the eight directions; the far side mirrors one.
        CHECK(b.textureName == "Goblin03");
        CHECK(b.flipU);
    }

    SECTION("Facing does not drift with the frame clock")
    {
        const Vec3 camPos = {0.0f, 0.0f, -100.0f};

        const detail::SpawnBillboard early =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr, 0);
        const detail::SpawnBillboard late =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr, 900000);
        CHECK(early.textureName == late.textureName);
    }
}

TEST_CASE("Outdoor terrain grid mirrors the Y axis against world space", "[outdoor_renderer]")
{
    // Grid Y grows south while gameplay Y grows north, so the two run opposite.
    CHECK(formats::outdoorGridToWorldX(64.0f) == Catch::Approx(0.0f));
    CHECK(formats::outdoorGridToWorldX(65.0f) == Catch::Approx(512.0f));
    CHECK(formats::outdoorGridToWorldY(64.0f) == Catch::Approx(0.0f));
    CHECK(formats::outdoorGridToWorldY(65.0f) == Catch::Approx(-512.0f));
    CHECK(formats::outdoorGridToWorldY(63.0f) == Catch::Approx(512.0f));

    // World -> grid is the exact inverse.
    for (float grid : {0.0f, 17.5f, 64.0f, 127.0f})
    {
        CHECK(formats::outdoorWorldToGridX(formats::outdoorGridToWorldX(grid)) ==
              Catch::Approx(grid));
        CHECK(formats::outdoorWorldToGridY(formats::outdoorGridToWorldY(grid)) ==
              Catch::Approx(grid));
    }
}
