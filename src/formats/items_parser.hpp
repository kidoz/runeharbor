// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct ItemEntry
{
    int id = 0;
    std::string picFile;
    std::string name;
    int value = 0;
    std::string equipStat;
    std::string skillGroup;
    std::string mod1; // e.g., "3d3", "4"
    int mod2 = 0;
    int material = 0;
    int idRepSt = 0; // ID/Rep/St
    std::string notIdentifiedName;
    int spriteIndex = 0;
    int varA = 0;
    int varB = 0;
    int equipX = 0;
    int equipY = 0;
    std::string notes;
};

class ItemsParser
{
public:
    explicit ItemsParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<ItemEntry>& getItems() const { return items; }

private:
    util::ILogger& logger;
    std::vector<ItemEntry> items;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
