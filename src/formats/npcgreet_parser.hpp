// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct NPCGreetingEntry
{
    int id = 0;
    std::string greeting1;
    std::string greeting2;
};

class NPCGreetingParser
{
  public:
    explicit NPCGreetingParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<NPCGreetingEntry>& getEntries() const { return entries_; }

  private:
    util::ILogger& logger_;
    std::vector<NPCGreetingEntry> entries_;
};

} // namespace runeharbor::formats
