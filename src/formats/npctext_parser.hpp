// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct NPCTextEntry
{
    int id = 0;
    std::string text;
    std::string notes;
    std::string owner;
};

class NPCTextParser
{
  public:
    explicit NPCTextParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<NPCTextEntry>& getNPCTextEntries() const { return entries; }

  private:
    util::ILogger& logger;
    std::vector<NPCTextEntry> entries;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
