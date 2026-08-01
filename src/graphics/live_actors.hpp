// SPDX-License-Identifier: MIT
//
// Live actor (monster) snapshot consumed by the world renderers. The renderers
// don't depend on game::CombatSystem directly; instead the host pushes a list
// of these each frame via OutdoorRenderer::setLiveActorProvider. This mirrors
// how MM7's renderers iterate the live actor table at 0x5FF06A (stride 0x34)
// rather than the static map spawn points.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace runeharbor::graphics
{

struct LiveActor
{
    float x = 0.0f, y = 0.0f, z = 0.0f; // gameplay-space (Z-up)
    uint16_t monsterId = 0;             // -> MonsterEntry (sprite name source)
    uint16_t facingAngle = 0;           // 0..2047 (MM7 turn units; 0=East, 512=North)
    bool dead = false;                  // render laid-out (corpses stay visible)
    bool flying = false;                // altitude offset
};

using LiveActorProvider = std::function<std::vector<LiveActor>()>;

} // namespace runeharbor::graphics
