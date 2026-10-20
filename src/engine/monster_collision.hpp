// SPDX-License-Identifier: MIT
#pragma once

namespace runeharbor::formats
{
struct BLVMapData;
struct ODMMapData;
} // namespace runeharbor::formats

namespace runeharbor::engine
{

struct MonsterCollisionConfig
{
    float radius = 28.0f;      // Horizontal padding applied to face bounds.
    float stepHeight = 48.0f;  // Maximum terrain rise a ground monster can climb.
    float probeHeight = 32.0f; // Segment height above the monster's feet.
};

/// True when a monster may move from one position to another.
///
/// Indoor maps: rejects segments crossing non-portal, non-water, vertical
/// geometry (walls/closed doors). Outdoor maps: rejects entry into water cells,
/// terrain steps taller than `stepHeight`, and building walls. Floors, ceilings,
/// portals, and invisible/fluid faces are ignored.
bool canMonsterMove(const formats::BLVMapData* blv, const formats::ODMMapData* odm, float fromX,
                    float fromY, float fromZ, float toX, float toY, float toZ,
                    const MonsterCollisionConfig& config = {});

} // namespace runeharbor::engine
