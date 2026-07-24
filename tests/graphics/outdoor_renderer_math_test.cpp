// SPDX-License-Identifier: MIT

#include <numbers>

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
    formats::ODMSpawnPoint spawn;
    spawn.x = 0;
    spawn.y = 0;
    spawn.z = 100;
    spawn.objectType = 1;

    // Sprite is at (0, 0, 100) (using x, y, z in MM7 coords)
    // Actually our gameplayToRenderPosition maps x->x, y->z, z->y.
    // So sprite render pos = (0, 100, 0)

    // Create a mock lookup that returns "Goblin"
    OutdoorRenderer::MonsterSpriteLookup lookup = [](uint16_t) { return "Goblin"; };

    SECTION("Camera directly south of sprite")
    {
        // Sprite is at Z=0. Camera at Z=-100.
        // Camera looks at sprite.
        Vec3 camPos = {0.0f, 0.0f, -100.0f};

        detail::SpawnBillboard b =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr);
        // dz = spriteZ - camZ = 0 - (-100) = 100
        // dx = 0
        // angleToCam = atan2(0, 100) = 0
        // octant = ((pi + pi/8 + 0 - 0) / (pi/4)) = 4.5 -> int(4) & 7 = 4
        // Index 4 -> +1 -> 5
        REQUIRE(b.textureName == "Goblinw05");
    }

    SECTION("Camera directly east of sprite")
    {
        // Sprite at X=0, Z=0. Camera at X=100, Z=0.
        Vec3 camPos = {100.0f, 0.0f, 0.0f};

        detail::SpawnBillboard b =
            detail::makeOutdoorSpawnBillboard(spawn, camPos, nullptr, lookup, nullptr);
        // dz = 0, dx = -100
        // angle = atan2(-100, 0) = -pi/2
        // octant = ((pi + pi/8 + pi/2) / (pi/4)) = (1.625 pi) / 0.25 pi = 6.5 -> 6
        // Index 6 -> 7
        REQUIRE(b.textureName == "Goblinw07");
    }
}
