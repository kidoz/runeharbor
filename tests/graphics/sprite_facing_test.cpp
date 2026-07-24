// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>

#include "../../src/graphics/sprite_facing.hpp"

using namespace runeharbor::graphics;

TEST_CASE("resolveSpriteFacing - 5-to-8 mirror mapping", "[sprite_facing]")
{
    // Octants 0..4 map directly to frames 1..5 with no flip.
    for (int o = 0; o <= 4; ++o)
    {
        const SpriteFacing f = resolveSpriteFacing(o);
        REQUIRE(f.frameDirection == o + 1);
        REQUIRE_FALSE(f.flipU);
    }

    // Octants 5,6,7 mirror frames 4,3,2.
    REQUIRE(resolveSpriteFacing(5).frameDirection == 4);
    REQUIRE(resolveSpriteFacing(6).frameDirection == 3);
    REQUIRE(resolveSpriteFacing(7).frameDirection == 2);
    REQUIRE(resolveSpriteFacing(5).flipU);
    REQUIRE(resolveSpriteFacing(6).flipU);
    REQUIRE(resolveSpriteFacing(7).flipU);
}

TEST_CASE("cameraRelativeOctant - facing toward and away from camera", "[sprite_facing]")
{
    const Vec3 sprite{0.0f, 0.0f, 0.0f};

    SECTION("camera directly in front (+Z), sprite heading 0 -> stable octant")
    {
        const Vec3 camera{0.0f, 0.0f, 100.0f};
        const int octant = cameraRelativeOctant(0, camera, sprite);
        REQUIRE(octant >= 0);
        REQUIRE(octant <= 7);
    }

    SECTION("opposite camera positions yield opposite octants (~4 apart)")
    {
        const Vec3 front{0.0f, 0.0f, 100.0f};
        const Vec3 back{0.0f, 0.0f, -100.0f};
        const int oFront = cameraRelativeOctant(0, front, sprite);
        const int oBack = cameraRelativeOctant(0, back, sprite);
        const int diff = ((oFront - oBack) & 7);
        REQUIRE(diff == 4);
    }

    SECTION("rotating the sprite shifts the octant in the opposite direction")
    {
        const Vec3 camera{0.0f, 0.0f, 100.0f};
        const int base = cameraRelativeOctant(0, camera, sprite);
        // Quarter turn of the sprite heading = 2 octants.
        const int quarter = cameraRelativeOctant(kMM7FullTurn / 4, camera, sprite);
        REQUIRE(((quarter - base) & 7) == 2);
    }
}

TEST_CASE("cameraRelativeOctant - result always normalized 0..7", "[sprite_facing]")
{
    const Vec3 sprite{10.0f, 5.0f, -20.0f};
    for (int facing = 0; facing < kMM7FullTurn; facing += 53)
    {
        for (float ang = 0.0f; ang < 6.28f; ang += 0.31f)
        {
            const Vec3 camera{sprite.x + std::cos(ang) * 300.0f, sprite.y,
                              sprite.z + std::sin(ang) * 300.0f};
            const int o = cameraRelativeOctant(facing, camera, sprite);
            REQUIRE(o >= 0);
            REQUIRE(o <= 7);
        }
    }
}
