// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../../src/graphics/billboard.hpp"

using namespace runeharbor::graphics;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr float kEps = 0.001f;
}

TEST_CASE("computeBillboardQuad - faces a camera looking down -Z", "[billboard]")
{
    // Camera in front of the sprite along +Z, looking toward -Z at the sprite.
    const Vec3 base{0.0f, 0.0f, 0.0f};
    const Vec3 camera{0.0f, 64.0f, 500.0f};
    const float halfWidth = 24.0f;
    const float height = 56.0f;

    const BillboardQuad quad = computeBillboardQuad(base, camera, halfWidth, height);

    SECTION("anchored at the base, extends upward by height")
    {
        // Bottom edge sits on the base plane (y == base.y).
        REQUIRE_THAT(quad.bottomLeft.y, WithinAbs(0.0f, kEps));
        REQUIRE_THAT(quad.bottomRight.y, WithinAbs(0.0f, kEps));
        // Top edge is exactly `height` above the bottom.
        REQUIRE_THAT(quad.topLeft.y, WithinAbs(height, kEps));
        REQUIRE_THAT(quad.topRight.y, WithinAbs(height, kEps));
    }

    SECTION("width spans 2*halfWidth along the screen-right axis")
    {
        const Vec3 widthVec = quad.bottomRight - quad.bottomLeft;
        REQUIRE_THAT(widthVec.length(), WithinAbs(2.0f * halfWidth, kEps));
        // With the camera on +Z, screen-right lies on the X axis.
        REQUIRE_THAT(std::abs(widthVec.x), WithinAbs(2.0f * halfWidth, kEps));
        REQUIRE_THAT(widthVec.y, WithinAbs(0.0f, kEps));
        REQUIRE_THAT(widthVec.z, WithinAbs(0.0f, kEps));
    }

    SECTION("quad normal (right x up) points back toward the camera on the ground plane")
    {
        const Vec3 right = (quad.bottomRight - quad.bottomLeft).normalized();
        const Vec3 up = (quad.topLeft - quad.bottomLeft).normalized();
        const Vec3 normal = right.cross(up);
        const Vec3 toCamera = Vec3{camera.x - base.x, 0.0f, camera.z - base.z}.normalized();
        REQUIRE(normal.dot(toCamera) > 0.99f);
    }
}

TEST_CASE("computeBillboardQuad - yaws with camera azimuth", "[billboard]")
{
    const Vec3 base{0.0f, 0.0f, 0.0f};
    const float halfWidth = 30.0f;
    const float height = 60.0f;

    // Camera off to the +X side: the billboard should rotate so its width axis
    // runs along Z instead of X.
    const Vec3 camera{500.0f, 0.0f, 0.0f};
    const BillboardQuad quad = computeBillboardQuad(base, camera, halfWidth, height);

    const Vec3 widthVec = quad.bottomRight - quad.bottomLeft;
    REQUIRE_THAT(widthVec.length(), WithinAbs(2.0f * halfWidth, kEps));
    REQUIRE_THAT(std::abs(widthVec.z), WithinAbs(2.0f * halfWidth, kEps));
    REQUIRE_THAT(widthVec.x, WithinAbs(0.0f, kEps));
}

TEST_CASE("computeBillboardQuad - degenerate overhead camera stays finite", "[billboard]")
{
    const Vec3 base{10.0f, 0.0f, 20.0f};
    const Vec3 camera{10.0f, 1000.0f, 20.0f}; // directly above
    const BillboardQuad quad = computeBillboardQuad(base, camera, 24.0f, 56.0f);

    const Vec3 widthVec = quad.bottomRight - quad.bottomLeft;
    REQUIRE_THAT(widthVec.length(), WithinAbs(48.0f, kEps));
    REQUIRE(std::isfinite(quad.topLeft.y));
}

TEST_CASE("computeBillboardQuad - flipU mirrors horizontal texcoords", "[billboard]")
{
    const Vec3 base{0.0f, 0.0f, 0.0f};
    const Vec3 camera{0.0f, 0.0f, 100.0f};

    const BillboardQuad normal = computeBillboardQuad(base, camera, 24.0f, 56.0f, false);
    REQUIRE_THAT(normal.uvBottomLeft.u, WithinAbs(0.0f, kEps));
    REQUIRE_THAT(normal.uvBottomRight.u, WithinAbs(1.0f, kEps));

    const BillboardQuad flipped = computeBillboardQuad(base, camera, 24.0f, 56.0f, true);
    REQUIRE_THAT(flipped.uvBottomLeft.u, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(flipped.uvBottomRight.u, WithinAbs(0.0f, kEps));
    // Vertical coords are unchanged by a horizontal flip.
    REQUIRE_THAT(flipped.uvBottomLeft.v, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(flipped.uvTopLeft.v, WithinAbs(0.0f, kEps));
}
