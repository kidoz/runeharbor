// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace runeharbor::util
{

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class ILogger
{
  public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, std::string_view message) = 0;

    void debug(std::string_view message) { log(LogLevel::Debug, message); }
    void info(std::string_view message) { log(LogLevel::Info, message); }
    void warning(std::string_view message) { log(LogLevel::Warning, message); }
    void error(std::string_view message) { log(LogLevel::Error, message); }
    void critical(std::string_view message) { log(LogLevel::Critical, message); }
};

} // namespace runeharbor::util
