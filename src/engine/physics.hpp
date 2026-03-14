// SPDX-License-Identifier: MIT
#pragma once

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

// Returns true if position was adjusted (collision occurred)
bool resolveIndoorCollision(const formats::BLVMapData& blv, float& px, float& py, float& pz,
                            float radius, float height);

bool resolveOutdoorCollision(const formats::ODMMapData& odm, float& px, float& py, float& pz,
                             float radius, float height);

// Applies gravity and resolves collisions for the party.
// Returns the new Z velocity.
void updatePartyPhysics(game::Party& party, const formats::BLVMapData* blv,
                        const formats::ODMMapData* odm, float deltaMs, const PhysicsConfig& config);

} // namespace runeharbor::engine
