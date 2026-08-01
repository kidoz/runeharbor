// SPDX-License-Identifier: MIT
//
// Hand-authored travel-destination table for stables/boats. The original MM7
// hardcodes destinations in the executable (records at 0x4F0830 + per-building
// menus at 0x4F0B4F/0x4F0BB8) — there is no external data file (trans.txt is
// area-description flavor text, NOT destinations; see docs/training-and-travel.md §3.2).
//
// This seed set connects the outdoor maps so travel is functional and
// expandable. Arrival coordinates are map-centre-ish defaults; the full
// per-building table can be enumerated from the binary later.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "building_type.hpp"

namespace runeharbor::game
{

struct TravelDestination
{
    std::string mapName;     // target map filename (e.g. "out02.odm")
    std::string displayName; // label shown in the travel UI
    float arrivalX = 0.0f;
    float arrivalY = 0.0f;
    float arrivalZ = 0.0f;
    float arrivalFacing = 0.0f; // yaw in MM7 turn-units (0..2047), matching Party::yaw
    int travelDays = 1;
};

// Returns the destinations offered by a stables/boat building, keyed by the
// building's source map id (the 2dEvents "Map" column). Empty if the building
// has no authored destinations (the UI will show "no destinations").
std::vector<TravelDestination> destinationsForBuilding(int sourceMapId, BuildingType type);

} // namespace runeharbor::game
