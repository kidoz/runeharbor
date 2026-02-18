// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct CreditsSection
{
    std::string title;
    std::vector<std::string> content;
};

class CreditsParser
{
  public:
    explicit CreditsParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<CreditsSection>& getCreditsSections() const { return sections; }

  private:
    util::ILogger& logger;
    std::vector<CreditsSection> sections;
    // Will use string manipulation
};

} // namespace runeharbor::formats
