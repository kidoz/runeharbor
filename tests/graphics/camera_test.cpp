// SPDX-License-Identifier: MIT
//
// Regression tests for Camera::setPosition()/getPosition() round-tripping.
// The previous implementation used atan2(-dir.x, -dir.z), which placed the
// camera on the opposite side of its target and made the party appear to look
// 180 degrees backwards (most visible on the Emerald Island start pier).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../../src/graphics/camera.hpp"

using runeharbor::graphics::Camera;
using runeharbor::graphics::Vec3;

TEST_CASE("Camera setPosition/getPosition round-trips", "[graphics][camera]")
{
    Camera camera;
    camera.setTarget(Vec3{0.0f, 0.0f, 0.0f});

    SECTION("camera south of target looks north")
    {
        const Vec3 eye{0.0f, 0.0f, -100.0f};
        camera.setPosition(eye);
        REQUIRE(camera.getPosition().x == Catch::Approx(eye.x).margin(0.01f));
        REQUIRE(camera.getPosition().y == Catch::Approx(eye.y).margin(0.01f));
        REQUIRE(camera.getPosition().z == Catch::Approx(eye.z).margin(0.01f));
    }

    SECTION("camera east of target looks west")
    {
        const Vec3 eye{100.0f, 0.0f, 0.0f};
        camera.setPosition(eye);
        REQUIRE(camera.getPosition().x == Catch::Approx(eye.x).margin(0.01f));
        REQUIRE(camera.getPosition().z == Catch::Approx(eye.z).margin(0.01f));
    }

    SECTION("camera north of target looks south")
    {
        const Vec3 eye{0.0f, 0.0f, 100.0f};
        camera.setPosition(eye);
        REQUIRE(camera.getPosition().z == Catch::Approx(eye.z).margin(0.01f));
    }

    SECTION("elevated camera preserves pitch")
    {
        const Vec3 eye{0.0f, 50.0f, -100.0f};
        camera.setPosition(eye);
        REQUIRE(camera.getPosition().x == Catch::Approx(eye.x).margin(0.01f));
        REQUIRE(camera.getPosition().y == Catch::Approx(eye.y).margin(0.01f));
        REQUIRE(camera.getPosition().z == Catch::Approx(eye.z).margin(0.01f));
    }
}
