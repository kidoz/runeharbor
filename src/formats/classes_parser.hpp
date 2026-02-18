// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct ClassEntry
{
    std::string className;   // The name of the class (e.g., "Knight", "Cavalier").
    std::string description; // Detailed description of the class.
    std::string notes;       // Category or base class (e.g., "Knight", "Thief").
};

class ClassesParser
{
  public:
    explicit ClassesParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<ClassEntry>& getClasses() const { return classes; }

  private:
    util::ILogger& logger;
    std::vector<ClassEntry> classes;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
