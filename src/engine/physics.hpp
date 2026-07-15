// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

#include "../formats/blv_map.hpp"
#include "../formats/odm_map.hpp"
#include "../game/party.hpp"

namespace runeharbor::engine
{

struct PhysicsConfig
{
    float playerRadius = 32.0f;
    float playerHeight = 160.0f;
    float gravity = -1024.0f; // units per second squared
    float maxFallSpeed = -2048.0f;
    float stepHeight = 48.0f;
};

// Applies gravity and resolves collisions for the party using continuous collision detection.
void updatePartyPhysics(game::Party& party, const formats::BLVMapData* blv,
                        const formats::ODMMapData* odm, float deltaMs, const PhysicsConfig& config);

} // namespace runeharbor::engine
