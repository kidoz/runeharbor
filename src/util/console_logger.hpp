// SPDX-License-Identifier: MIT
#pragma once

#include "ilogger.hpp"

namespace runeharbor::util
{

class ConsoleLogger : public ILogger
{
  public:
    ConsoleLogger() = default;
    ~ConsoleLogger() override = default;

    void log(LogLevel level, std::string_view message) override;

  private:
    static const char* levelToString(LogLevel level);
    static const char* levelToColor(LogLevel level);
};

} // namespace runeharbor::util
