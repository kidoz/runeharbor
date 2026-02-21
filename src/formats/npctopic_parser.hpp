// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct NPCTopicEntry
{
    int id = 0;
    std::string topic;
    std::vector<int> textIds;
    std::string owner;
};

class NPCTopicParser
{
  public:
    explicit NPCTopicParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<NPCTopicEntry>& getEntries() const { return entries_; }

  private:
    std::vector<int> parseTextIdRefs(std::string_view raw) const;
    util::ILogger& logger_;
    std::vector<NPCTopicEntry> entries_;
};

} // namespace runeharbor::formats
