// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct QuestEntry
{
    int qBit = 0;              // The unique ID for the quest or quest bit.
    std::string questNoteText; // The main text for the quest note.
    std::string notes;         // Additional notes or internal description.
    std::string owner;         // The owner or giver of the quest.
};

class QuestsParser
{
  public:
    explicit QuestsParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<QuestEntry>& getQuests() const { return quests; }

  private:
    util::ILogger& logger;
    std::vector<QuestEntry> quests;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
