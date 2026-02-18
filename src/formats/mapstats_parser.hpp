// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct MonsterEncounter
{
    std::string picture;
    std::string name;
    std::string countRange; // e.g., "1-5" or just a number
    std::string id;         // Changed from int to string, as it often contains ranges like "2-5"
};

struct MapStatsEntry
{
    int id = 0;
    std::string name;
    std::string fileName;
    int resetCount = 0; // Field labeled '#'
    int visitDay = 0;   // Field labeled 'Day'
    int per = 0;        // Field labeled '0-20'
    int refillDays = 0; // Field labeled 'Days'
    int alertDays = 0;  // Field labeled 'Days'
    int perm = 0;       // Field labeled 'Perm'
    int steal = 0;      // Field labeled '0-20'
    int lock = 0;       // Field labeled '0-10'
    int trap = 0;       // Field labeled '0-6'
    int tres = 0;       // Field labeled '%' (Treasure)
    int enc = 0;        // Field labeled '%' (Encounter)
    int m1 = 0;         // Field labeled '%' (Monster 1 rate?)
    int m2 = 0;         // Field labeled '%' (Monster 2 rate?)

    MonsterEncounter monster1;
    MonsterEncounter monster2;
    MonsterEncounter monster3;

    std::string track;           // Field labeled 'Track'
    std::string eaxEnvironments; // Field labeled 'EAX Environments'
    std::string mapDesigner;     // Field labeled 'Map Designer'
    std::string notes;           // Field labeled 'Notes'
    std::string notesExtraField; // Extra empty field found between notes and inArea
    std::string inArea;          // Field labeled 'in area' - can be 'x', '0', etc.
};

class MapStatsParser
{
  public:
    explicit MapStatsParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<MapStatsEntry>& getMapStats() const { return mapStats; }

  private:
    util::ILogger& logger;
    std::vector<MapStatsEntry> mapStats;
};

} // namespace runeharbor::formats
