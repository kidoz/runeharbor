// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct NPCProfessionEntry
{
    int id = 0;
    std::string name;
    int cost = 0;
    std::string benefitText;
    std::string joinText;
    std::string actionText;
    std::string dismissText;
};

class NPCProfessionParser
{
  public:
    explicit NPCProfessionParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<NPCProfessionEntry>& getEntries() const { return entries_; }

  private:
    util::ILogger& logger_;
    std::vector<NPCProfessionEntry> entries_;
};

} // namespace runeharbor::formats
