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

/// A row from PlaceMon.txt: an id-to-name table for placed monsters.
///
/// The shipped file is a two-column table ("id<TAB>name"), not a random
/// placement rule table. The header row starts with an empty first column and
/// is ignored by the parser.
struct PlacemonEntry
{
    int id = 0;
    std::string name;
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
