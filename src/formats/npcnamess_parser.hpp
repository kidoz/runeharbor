// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct NPCNames
{
    std::vector<std::string> maleNames;
    std::vector<std::string> femaleNames;
};

class NPCNamesParser
{
public:
    explicit NPCNamesParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const NPCNames& getNPCNames() const { return names; }

private:
    util::ILogger& logger;
    NPCNames names;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
