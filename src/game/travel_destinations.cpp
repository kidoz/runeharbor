// SPDX-License-Identifier: MIT
#include "travel_destinations.hpp"

#include <algorithm>

namespace runeharbor::game
{

namespace
{

TravelDestination dest(std::string map, std::string name, int days)
{
    TravelDestination d;
    d.mapName = std::move(map);
    d.displayName = std::move(name);
    d.travelDays = days;
    // Arrival at a reasonable centred position; the map loader clamps to
    // terrain. Full per-destination coords can be lifted from the binary later.
    d.arrivalX = 1024.0f;
    d.arrivalY = 1024.0f;
    d.arrivalZ = 0.0f;
    d.arrivalFacing = 0.0f;
    return d;
}

} // namespace

std::vector<TravelDestination> destinationsForBuilding(int sourceMapId, BuildingType type)
{
    // Seed set: each outdoor map's stables/boats offer trips to its neighbours.
    // (const_cast-free: this is a pure lookup returning by value.) The map-id
    // values mirror the 2dEvents "Map" column.
    std::vector<TravelDestination> out;
    const bool isBoat = (type == BuildingType::Boat);

    switch (sourceMapId)
    {
    case 1: // Emeral Island
        out.push_back(dest("out02.odm", "Harmondale", 2));
        if (isBoat)
            out.push_back(dest("out03.odm", "Tularea", 3));
        break;
    case 2: // Harmondale
        out.push_back(dest("out01.odm", "Emerald Island", 2));
        out.push_back(dest("out03.odm", "Tularea", 1));
        break;
    case 3: // Tularea
        out.push_back(dest("out02.odm", "Harmondale", 1));
        out.push_back(dest("out04.odm", "Bracada Desert", 2));
        break;
    case 4: // Bracada
        out.push_back(dest("out03.odm", "Tularea", 2));
        break;
    default:
        // Fallback: a single hop back to the starting region so every
        // stables/boat is functional even without an authored table.
        if (sourceMapId != 1)
            out.push_back(dest("out01.odm", "Emerald Island", 3));
        break;
    }
    return out;
}

} // namespace runeharbor::game
