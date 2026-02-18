// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct AutonoteEntry
{
    int noteBit = 0;
    std::string autonoteText;
    std::string category;
};

class AutonoteParser
{
  public:
    explicit AutonoteParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<AutonoteEntry>& getAutonoteEntries() const { return entries; }

  private:
    util::ILogger& logger;
    std::vector<AutonoteEntry> entries;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
