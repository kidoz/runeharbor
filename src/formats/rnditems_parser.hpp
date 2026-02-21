// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::formats
{

struct RndItemEntry
{
    int itemId = 0;
    int baseLevel = 0;
    std::array<int, 6> levelWeights = {};
};

class RndItemsParser
{
  public:
    explicit RndItemsParser(util::ILogger& logger);

    bool parse(const std::vector<uint8_t>& data);
    const std::vector<RndItemEntry>& getEntries() const { return entries_; }

  private:
    util::ILogger& logger_;
    std::vector<RndItemEntry> entries_;
};

} // namespace runeharbor::formats
