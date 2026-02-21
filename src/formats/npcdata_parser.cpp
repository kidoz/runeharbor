// SPDX-License-Identifier: MIT
#include "npcdata_parser.hpp"

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

bool parseBool(std::string_view raw)
{
    const std::string lowered = util::toLower(util::trim(std::string(raw)));
    return lowered == "1" || lowered == "y" || lowered == "yes" || lowered == "true";
}

bool startsWithDigit(std::string_view raw)
{
    return !raw.empty() && raw.front() >= '0' && raw.front() <= '9';
}
} // namespace

NPCDataParser::NPCDataParser(util::ILogger& logger) : logger_(logger) {}

bool NPCDataParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();
    if (data.empty())
    {
        logger_.error("NPCData data is empty");
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
        if (id <= 0)
        {
            continue;
        }

        NPCDataEntry entry;
        entry.id = id;
        entry.name = util::trim(fields[1]);
        entry.pictureId = (fields.size() >= 3) ? parseIntOr(fields[2], 0) : 0;
        entry.professionId = (fields.size() >= 9) ? parseIntOr(fields[8], 0) : 0;
        entry.joinsParty = (fields.size() >= 10) ? parseBool(fields[9]) : false;
        entry.greetingId = (fields.size() >= 8) ? parseIntOr(fields[7], 0) : 0;

        for (int i = 0; i < 6; i++)
        {
            const size_t idx = static_cast<size_t>(10 + i);
            if (idx >= fields.size())
            {
                break;
            }
            entry.actionEventIds[static_cast<size_t>(i)] = parseIntOr(fields[idx], 0);
        }

        entries_.push_back(std::move(entry));
    }

    logger_.info("Parsed " + std::to_string(entries_.size()) + " npc data rows");
    return true;
}

} // namespace runeharbor::formats
