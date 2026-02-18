// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct AwardEntry
{
    int aBit = 0;          // The unique ID for the award bit.
    std::string awardText; // The display text for the award, can include format specifiers.
    int sortOrder = 0;     // A numerical value for sorting or categorization.
    std::string notes;     // Additional notes or descriptions.
};

class AwardsParser
{
  public:
    explicit AwardsParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<AwardEntry>& getAwards() const { return awards; }

  private:
    util::ILogger& logger;
    std::vector<AwardEntry> awards;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
