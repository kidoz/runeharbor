// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::formats
{

struct PlacemonEntry
{
    std::string mapName;
    int minDifficulty = 1;
    int maxDifficulty = 10;
    int monsterId = 0;
    int weight = 0;
};

class PlacemonParser
{
  public:
    explicit PlacemonParser(util::ILogger& logger);

    bool parse(const std::vector<uint8_t>& data);
    const std::vector<PlacemonEntry>& getEntries() const { return entries_; }

  private:
    util::ILogger& logger_;
    std::vector<PlacemonEntry> entries_;
};

} // namespace runeharbor::formats
