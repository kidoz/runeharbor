// SPDX-License-Identifier: MIT
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct GlobalTextEntry
{
    int id = 0;
    std::string text;
};

class GlobalTextParser
{
  public:
    explicit GlobalTextParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::map<int, std::string>& getTextEntries() const { return textEntries; }
    std::optional<std::string> getText(int id) const;

  private:
    util::ILogger& logger;
    std::map<int, std::string> textEntries;
};

} // namespace runeharbor::formats
