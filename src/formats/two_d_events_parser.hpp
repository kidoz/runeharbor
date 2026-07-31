// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../game/building_type.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

using game::BuildingType;

// One row of `2dEvents.txt` (the building/shop registry). Field meanings are
// RE-derived from the loader FUN_00443824 and the in-memory struct at
// 0x005912B8 (stride 0x34). See docs/re/29-shops-and-economy.md section 2.
//
// The legacy `category`/`displayName`/`columns` members are preserved for
// back-compat with existing callers; new code should prefer the typed fields.
struct TwoDEventEntry
{
    int id = 0; // global building id (column 1)

    // Typed fields decoded from the tab-delimited columns.
    BuildingType buildingType = BuildingType::None; // "Type" column -> code
    int perTypeIndex = 0;                           // column 2 (the second "#")
    int mapId = 0;                                  // "Map"
    int pictureId = 0;                              // "Picture" (shopkeeper portrait)
    std::string name;                               // "Name" (shop title)
    std::string proprietorName;                     // "Proprieter Name"
    std::string title;                              // "Title" (Blacksmith/Healer/...)
    float buyMultiplier = 1.0f;                     // "Val" -> struct +0x20 float
    float secondaryMultiplier = 1.0f;               // "A"   -> struct +0x24 float
    int serviceSeed = 0;                            // "C"   -> struct +0x1C
    int openHour = 0;                               // "Open"
    int closedHour = 24;                            // "Closed"

    // Legacy / forward-compat.
    std::string category;             // first non-numeric text column (the Type label)
    std::string displayName;          // last non-numeric text column (the shop Name)
    std::vector<std::string> columns; // every column verbatim (fields[1..])
};

class TwoDEventsParser
{
  public:
    explicit TwoDEventsParser(util::ILogger& logger);

    bool parse(const std::vector<uint8_t>& data);
    const std::vector<TwoDEventEntry>& getEntries() const { return entries_; }
    const TwoDEventEntry* findById(int id) const;

  private:
    static bool containsAlpha(std::string_view text);

    util::ILogger& logger_;
    std::vector<TwoDEventEntry> entries_;
};

} // namespace runeharbor::formats
