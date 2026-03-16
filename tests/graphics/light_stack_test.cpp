// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

#include "../../src/graphics/light_stack.hpp"

using namespace runeharbor::graphics;

namespace
{

LightSource makeLight(float x, float y, float z, float radius, float brightness = 1.0f)
{
    LightSource ls;
    ls.position = Vec3(x, y, z);
    ls.radius = radius;
    ls.brightness = brightness;
    ls.color = {1.0f, 1.0f, 1.0f, 1.0f};
    ls.active = true;
    return ls;
}

} // namespace

TEST_CASE("LightStack capacity and overflow", "[graphics][lighting]")
{
    LightStack stack(3);

    SECTION("push within capacity succeeds")
    {
        REQUIRE(stack.pushLight(makeLight(0, 0, 0, 100)));
        REQUIRE(stack.pushLight(makeLight(1, 0, 0, 100)));
        REQUIRE(stack.pushLight(makeLight(2, 0, 0, 100)));
        REQUIRE(stack.count() == 3);
        REQUIRE(stack.isFull());
    }

    SECTION("push beyond capacity fails and counts overflow")
    {
        stack.pushLight(makeLight(0, 0, 0, 100));
        stack.pushLight(makeLight(1, 0, 0, 100));
        stack.pushLight(makeLight(2, 0, 0, 100));
        REQUIRE_FALSE(stack.pushLight(makeLight(3, 0, 0, 100)));
        REQUIRE(stack.count() == 3);
        REQUIRE(stack.overflowCount() == 1);
    }

    SECTION("clear resets count and overflow")
    {
        stack.pushLight(makeLight(0, 0, 0, 100));
        stack.pushLight(makeLight(1, 0, 0, 100));
        stack.pushLight(makeLight(2, 0, 0, 100));
        stack.pushLight(makeLight(3, 0, 0, 100)); // overflow
        stack.clear();
        REQUIRE(stack.count() == 0);
        REQUIRE(stack.overflowCount() == 0);
        REQUIRE_FALSE(stack.isFull());
    }
}

TEST_CASE("LightStack default capacity", "[graphics][lighting]")
{
    LightStack stack;
    REQUIRE(stack.maxLights() == LightStack::kDefaultMaxLights);
    REQUIRE(stack.count() == 0);
}

TEST_CASE("LightStack attenuation", "[graphics][lighting]")
{
    SECTION("point at light center gets full brightness")
    {
        std::vector<LightSource> lights = {makeLight(0, 0, 0, 100, 1.0f)};
        auto color = LightStack::computeAttenuation(lights, 0, 0, 0);
        REQUIRE(color.r == 1.0f);
        REQUIRE(color.g == 1.0f);
        REQUIRE(color.b == 1.0f);
    }

    SECTION("point at radius edge gets zero")
    {
        std::vector<LightSource> lights = {makeLight(0, 0, 0, 100, 1.0f)};
        auto color = LightStack::computeAttenuation(lights, 100, 0, 0);
        REQUIRE(color.r < 0.01f);
    }

    SECTION("point outside radius gets zero")
    {
        std::vector<LightSource> lights = {makeLight(0, 0, 0, 100, 1.0f)};
        auto color = LightStack::computeAttenuation(lights, 200, 0, 0);
        REQUIRE(color.r == 0.0f);
    }

    SECTION("halfway distance gets ~50% brightness")
    {
        std::vector<LightSource> lights = {makeLight(0, 0, 0, 100, 1.0f)};
        auto color = LightStack::computeAttenuation(lights, 50, 0, 0);
        REQUIRE(color.r > 0.45f);
        REQUIRE(color.r < 0.55f);
    }

    SECTION("inactive light contributes nothing")
    {
        LightSource inactive = makeLight(0, 0, 0, 100, 1.0f);
        inactive.active = false;
        std::vector<LightSource> lights = {inactive};
        auto color = LightStack::computeAttenuation(lights, 0, 0, 0);
        REQUIRE(color.r == 0.0f);
    }

    SECTION("multiple lights accumulate clamped to 1.0")
    {
        std::vector<LightSource> lights = {
            makeLight(0, 0, 0, 100, 0.8f),
            makeLight(0, 0, 0, 100, 0.8f),
        };
        auto color = LightStack::computeAttenuation(lights, 0, 0, 0);
        REQUIRE(color.r == 1.0f); // clamped
    }

    SECTION("colored light contributes per-channel")
    {
        LightSource red = makeLight(0, 0, 0, 100, 1.0f);
        red.color = {1.0f, 0.0f, 0.0f, 1.0f};
        std::vector<LightSource> lights = {red};
        auto color = LightStack::computeAttenuation(lights, 0, 0, 0);
        REQUIRE(color.r == 1.0f);
        REQUIRE(color.g == 0.0f);
        REQUIRE(color.b == 0.0f);
    }

    SECTION("attenuateAt uses stack lights")
    {
        LightStack stack;
        stack.pushLight(makeLight(0, 0, 0, 100, 1.0f));
        auto color = stack.attenuateAt(0, 0, 0);
        REQUIRE(color.r == 1.0f);
    }
}

TEST_CASE("AmbientLight", "[graphics][lighting]")
{
    SECTION("fromByte(0) gives minimum ambient")
    {
        auto ambient = AmbientLight::fromByte(0);
        REQUIRE(ambient.r > 0.29f);
        REQUIRE(ambient.r < 0.31f);
    }

    SECTION("fromByte(255) gives full ambient")
    {
        auto ambient = AmbientLight::fromByte(255);
        REQUIRE(ambient.r > 0.99f);
    }

    SECTION("toColor returns SDL_FColor")
    {
        AmbientLight a{0.5f, 0.6f, 0.7f};
        auto c = a.toColor();
        REQUIRE(c.r == 0.5f);
        REQUIRE(c.g == 0.6f);
        REQUIRE(c.b == 0.7f);
        REQUIRE(c.a == 1.0f);
    }
}
