// SPDX-License-Identifier: MIT
#include "npcprof_parser.hpp"

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

NPCProfessionParser::NPCProfessionParser(util::ILogger& logger) : logger_(logger) {}

bool NPCProfessionParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();
    if (data.empty())
    {
        logger_.error("NPCProf data is empty");
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
        if (id < 0) // Professions start from 0 or 1, let's accept 0
        {
            continue;
        }

        NPCProfessionEntry entry;
        entry.id = id;
        entry.name = util::trim(fields[1]);
        entry.cost = (fields.size() >= 3) ? parseIntOr(fields[2], 0) : 0;
        entry.benefitText = (fields.size() >= 4) ? util::trim(fields[3]) : "";
        entry.joinText = (fields.size() >= 5) ? util::trim(fields[4]) : "";
        entry.actionText = (fields.size() >= 6) ? util::trim(fields[5]) : "";
        entry.dismissText = (fields.size() >= 7) ? util::trim(fields[6]) : "";

        entries_.push_back(std::move(entry));
    }

    logger_.info("Parsed " + std::to_string(entries_.size()) + " npc profession rows");
    return true;
}

} // namespace runeharbor::formats
