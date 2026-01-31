// SPDX-License-Identifier: MIT
#include "console_logger.hpp"

#include <format>
#include <iostream>

namespace runeharbor::util
{

void ConsoleLogger::log(LogLevel level, std::string_view message)
{
    std::cout << std::format("{}{:<8}\033[0m {}\n", levelToColor(level), levelToString(level),
                             message);
}

const char* ConsoleLogger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:
        return "[DEBUG]";
    case LogLevel::Info:
        return "[INFO ]";
    case LogLevel::Warning:
        return "[WARN ]";
    case LogLevel::Error:
        return "[ERROR]";
    case LogLevel::Critical:
        return "[CRIT ]";
    }
    return "[UNKNOWN]";
}

const char* ConsoleLogger::levelToColor(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:
        return "\033[36m"; // Cyan
    case LogLevel::Info:
        return "\033[32m"; // Green
    case LogLevel::Warning:
        return "\033[33m"; // Yellow
    case LogLevel::Error:
        return "\033[31m"; // Red
    case LogLevel::Critical:
        return "\033[35m"; // Magenta
    }
    return "\033[0m"; // Reset
}

} // namespace runeharbor::util
