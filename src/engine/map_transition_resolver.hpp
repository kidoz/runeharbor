// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string_view>

namespace runeharbor::game
{
struct MapTransition;
class GameWorld;
} // namespace runeharbor::game

namespace runeharbor::engine
{

// Resolve the transition attached to the currently interacted event (if any) for the given
// normalized target map. Returns nullptr when no matching transition exists.
const game::MapTransition* resolveInteractionTransition(const game::GameWorld* world,
                                                        std::string_view normalizedTargetMap);

// Convert transition direction token into canonical spawn index:
// 0 = default, 1 = north, 2 = south, 3 = east, 4 = west.
int resolveSpawnIndexFromDirection(int direction);

// Resolve entry yaw override for edge transitions.
// Returns empty when no direction-based yaw override should be applied.
std::optional<float> directionToEntryYaw(int direction);

} // namespace runeharbor::engine
