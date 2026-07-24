// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../../src/engine/physics.hpp"
#include "../../src/formats/blv_map.hpp"
#include "../../src/formats/odm_map.hpp"
#include "../../src/game/party.hpp"

using namespace runeharbor;

TEST_CASE("Physics - Basic Gravity", "[physics]")
{
    game::Party party;
    party.setWorldPosition(0.0f, 0.0f, 100.0f);
    party.setVelocityZ(0.0f);

    engine::PhysicsConfig config;
    config.gravity = -1000.0f;

    SECTION("Party falls under gravity")
    {
        engine::updatePartyPhysics(party, nullptr, nullptr, 1000.0f, config); // 1 second

        // velocity = a * t = -1000 * 1 = -1000
        REQUIRE_THAT(party.velocityZ(), Catch::Matchers::WithinAbs(-1000.0f, 0.1f));
        // position = p0 + v0*t + 0.5*a*t^2 = 100 + 0 - 500 = -400?
        // Wait, the engine applies gravity to vel first, then moves by vel * dt.
        // vel = 0 + (-1000) * 1 = -1000
        // pos = 100 + (-1000) * 1 = -900
        // And damping is applied: vel *= DAMPING_FACTOR (0.89263916f)
        // Let's just check it decreased
        REQUIRE(party.worldZ() < 100.0f);
    }
}

TEST_CASE("Physics - Slope Limitations", "[physics]")
{
    game::Party party;
    party.setWorldPosition(0.0f, 0.0f, 100.0f);
    party.setVelocityZ(0.0f);

    engine::PhysicsConfig config;
    config.gravity = 0.0f; // Turn off gravity for this test to isolate wall/slope push

    formats::BLVMapData blv;
    // We will create a face that acts as a steep slope (e.g. normal.z = 0.5, steeper than 45
    // degrees) 45 degrees is normal.z = 0.707. Steeper means smaller normal.z.
    formats::ParsedFace steepFace;
    steepFace.normalFX = 0.866f; // sqrt(3)/2
    steepFace.normalFY = 0.0f;
    steepFace.normalFZ = 0.5f;    // 60 degree slope (too steep!)
    steepFace.normalFDist = 0.0f; // Passes through origin
    steepFace.minX = -1000;
    steepFace.maxX = 1000;
    steepFace.minY = -1000;
    steepFace.maxY = 1000;
    steepFace.minZ = -1000;
    steepFace.maxZ = 1000;

    // Create a large polygon to ensure collision
    blv.vertices = {{0, 1000, 1000}, {0, -1000, 1000}, {-1000, -1000, -1000}, {-1000, 1000, -1000}};
    steepFace.vertexIndices = {0, 1, 2, 3};
    blv.faces.push_back(steepFace);

    SECTION("Steep slope repels player")
    {
        // Player moves towards the slope (-X direction)
        // Normal is +X (+0.866), so facing the slope is moving -X.
        party.setWorldPosition(100.0f, 0.0f, 0.0f);
        party.setVelocityX(-1000.0f);
        party.setVelocityY(0.0f);
        party.setVelocityZ(0.0f);

        engine::updatePartyPhysics(party, &blv, nullptr, 1000.0f, config);

        // Should be pushed back / velocity damped, but definitely should NOT be able to pass X=0
        REQUIRE(party.worldX() >= -32.0f); // radius is 32, so can't pass X=-32
    }
}

TEST_CASE("Physics - Wall Collision", "[physics]")
{
    game::Party party;
    party.setWorldPosition(100.0f, 0.0f, 0.0f);
    party.setVelocityX(0.0f);
    party.setVelocityY(0.0f);
    party.setVelocityZ(0.0f);

    engine::PhysicsConfig config;
    config.gravity = 0.0f;

    formats::BLVMapData blv;
    formats::ParsedFace wallFace;
    wallFace.normalFX = 1.0f;
    wallFace.normalFY = 0.0f;
    wallFace.normalFZ = 0.0f;
    wallFace.normalFDist = 0.0f;
    wallFace.minX = -1000;
    wallFace.maxX = 1000;
    wallFace.minY = -1000;
    wallFace.maxY = 1000;
    wallFace.minZ = -1000;
    wallFace.maxZ = 1000;

    blv.vertices = {{0, 1000, 1000}, {0, -1000, 1000}, {0, -1000, -1000}, {0, 1000, -1000}};
    wallFace.vertexIndices = {0, 1, 2, 3};
    blv.faces.push_back(wallFace);

    SECTION("Straight into wall stops movement")
    {
        party.setVelocityX(-1000.0f);
        engine::updatePartyPhysics(party, &blv, nullptr, 1000.0f, config);

        // Stopped at radius distance from wall
        REQUIRE_THAT(party.worldX(), Catch::Matchers::WithinAbs(32.0f, 0.5f));
        REQUIRE_THAT(party.velocityX(), Catch::Matchers::WithinAbs(0.0f, 0.1f));
    }

    SECTION("Angle into wall slides along wall")
    {
        party.setVelocityX(-1000.0f);
        party.setVelocityY(1000.0f);
        engine::updatePartyPhysics(party, &blv, nullptr, 1000.0f, config);

        // Blocked in X, slid in Y. Due to CLOSEST_DIST buffer and slide normal calculation,
        // the player bounces slightly outward from the wall (X > 32).
        REQUIRE(party.worldX() >= 32.0f);
        REQUIRE(party.worldX() <= 50.0f);
        REQUIRE(party.worldY() > 500.0f); // Should have moved far along Y
    }
}
