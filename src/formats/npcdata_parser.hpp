// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct NPCDataEntry
{
    int id = 0;
    std::string name;
    int pictureId = 0;
    int professionId = 0;
    bool joinsParty = false;
    int greetingId = 0;
    std::array<int, 6> actionEventIds = {};
};

class NPCDataParser
{
  public:
    explicit NPCDataParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<NPCDataEntry>& getEntries() const { return entries_; }

  private:
    util::ILogger& logger_;
    std::vector<NPCDataEntry> entries_;
};

} // namespace runeharbor::formats
