// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct SpellEntry
{
    int id = 0;
    int level = 0;
    std::string name;
    std::string resistance;
    std::string shortName;
    std::string description;
    std::string normalEffect;
    std::string expertEffect;
    std::string masterEffect;
    std::string grandMasterEffect;
    std::string stats; // Single char, e.g., "P", "PMEC"
};

class SpellsParser
{
  public:
    explicit SpellsParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<SpellEntry>& getSpells() const { return spells; }

  private:
    util::ILogger& logger;
    std::vector<SpellEntry> spells;
    // Will use a robust splitString utility function from string_utils.hpp
};

} // namespace runeharbor::formats
