// SPDX-License-Identifier: MIT
#include "npcgreet_parser.hpp"

#include <algorithm>
#include <charconv>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{
namespace
{
int parseIntOr(std::string_view raw, int fallback)
{
    const std::string trimmed = util::trim(std::string(raw));
    if (trimmed.empty())
    {
        return fallback;
    }

    int value = 0;
    const char* begin = trimmed.data();
    const char* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
    {
        return fallback;
    }

    return value;
}

bool startsWithDigit(std::string_view raw)
{
    return !raw.empty() && raw.front() >= '0' && raw.front() <= '9';
}
} // namespace

NPCGreetingParser::NPCGreetingParser(util::ILogger& logger) : logger_(logger) {}

bool NPCGreetingParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();
    if (data.empty())
    {
        logger_.error("NPCGreet data is empty");
        return false;
    }

    const std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    size_t lineStart = 0;
    while (lineStart < content.size())
    {
        size_t lineEnd = content.find('\n', lineStart);
        if (lineEnd == std::string::npos)
        {
            lineEnd = content.size();
        }

        const std::string line = util::trim(content.substr(lineStart, lineEnd - lineStart));
        lineStart = lineEnd + 1;
        if (line.empty() || !startsWithDigit(line))
        {
            continue;
        }

        const auto fields = util::splitString(line, '\t', '"');
        if (fields.size() < 2)
        {
            continue;
        }

        const int id = parseIntOr(fields[0], -1);
        if (id < 0)
        {
            continue;
        }

        NPCGreetingEntry entry;
        entry.id = id;
        entry.greeting1 = util::trim(fields[1]);
        entry.greeting2 = (fields.size() >= 3) ? util::trim(fields[2]) : entry.greeting1;

        entries_.push_back(std::move(entry));
    }

    logger_.info("Parsed " + std::to_string(entries_.size()) + " npc greeting rows");
    return true;
}

} // namespace runeharbor::formats
