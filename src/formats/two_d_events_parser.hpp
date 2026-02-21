// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct TwoDEventEntry
{
    int id = 0;
    std::string category;
    std::string displayName;
    std::vector<std::string> columns;
};

class TwoDEventsParser
{
  public:
    explicit TwoDEventsParser(util::ILogger& logger);

    bool parse(const std::vector<uint8_t>& data);
    const std::vector<TwoDEventEntry>& getEntries() const { return entries_; }
    const TwoDEventEntry* findById(int id) const;

  private:
    static bool containsAlpha(std::string_view text);

    util::ILogger& logger_;
    std::vector<TwoDEventEntry> entries_;
};

} // namespace runeharbor::formats
