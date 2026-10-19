// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>

namespace runeharbor::game
{

/// Serialized runtime state for one live monster. Per-map lists of these are
/// stored on GameWorld so leaving and re-entering (or saving and loading) a map
/// preserves monster health, positions, AI, and event overrides instead of
/// respawning the generated roster at full health.
struct SavedMonsterState
{
    int monsterId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    uint16_t facingAngle = 0;
    int currentHP = 0;
    int maxHP = 0;
    int group = 0;
    uint16_t topic = 0;
    uint8_t aiState = 0;
    int hostilityLevel = 0;
    int hostilityOverride = 0;
    uint32_t flags = 0;
    uint32_t flagsExtended = 0;
    int targetCharacter = -1;
    uint32_t targetId = 0;
    std::array<int, 8> scriptFields = {};
};

} // namespace runeharbor::game
