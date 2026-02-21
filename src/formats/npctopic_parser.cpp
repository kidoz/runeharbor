// SPDX-License-Identifier: MIT
#include "npctopic_parser.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <unordered_set>

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
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
    {
        return fallback;
    }
    return value;
}

bool isNumeric(std::string_view raw)
{
    if (raw.empty())
    {
        return false;
    }
    return std::all_of(raw.begin(), raw.end(),
                       [](unsigned char c) { return c >= '0' && c <= '9'; });
}

int pow10i(size_t exponent)
{
    int value = 1;
    for (size_t i = 0; i < exponent; i++)
    {
        value *= 10;
    }
    return value;
}
} // namespace

NPCTopicParser::NPCTopicParser(util::ILogger& logger) : logger_(logger) {}

bool NPCTopicParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();

    if (data.empty())
    {
        logger_.error("NPCTopic data is empty");
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
        if (line.empty())
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

        NPCTopicEntry entry;
        entry.id = id;
        entry.topic = util::trim(fields[1]);
        if (fields.size() >= 5)
        {
            entry.textIds = parseTextIdRefs(fields[4]);
        }
        if (fields.size() >= 6)
        {
            entry.owner = util::trim(fields[5]);
        }

        entries_.push_back(std::move(entry));
    }

    logger_.info("Parsed " + std::to_string(entries_.size()) + " npc topic rows");
    return true;
}

std::vector<int> NPCTopicParser::parseTextIdRefs(std::string_view raw) const
{
    std::string cleaned = util::trim(std::string(raw));
    if (cleaned.empty())
    {
        return {};
    }

    if (cleaned.size() >= 2 && ((cleaned.front() == '"' && cleaned.back() == '"') ||
                                (cleaned.front() == '\'' && cleaned.back() == '\'')))
    {
        cleaned = cleaned.substr(1, cleaned.size() - 2);
    }

    const auto tokens = util::splitString(cleaned, ',');
    std::vector<int> ids;
    std::unordered_set<int> seen;

    for (const auto& tokenRaw : tokens)
    {
        const std::string token = util::trim(tokenRaw);
        if (token.empty())
        {
            continue;
        }

        const size_t dash = token.find('-');
        if (dash == std::string::npos)
        {
            const int value = parseIntOr(token, -1);
            if (value > 0 && !seen.contains(value))
            {
                seen.insert(value);
                ids.push_back(value);
            }
            continue;
        }

        const std::string lhsRaw = util::trim(token.substr(0, dash));
        const std::string rhsRaw = util::trim(token.substr(dash + 1));
        if (!isNumeric(lhsRaw) || !isNumeric(rhsRaw))
        {
            continue;
        }

        int lhs = parseIntOr(lhsRaw, -1);
        if (lhs <= 0)
        {
            continue;
        }

        int rhs = parseIntOr(rhsRaw, -1);
        if (rhs <= 0)
        {
            continue;
        }

        if (rhsRaw.size() < lhsRaw.size())
        {
            const int base = pow10i(rhsRaw.size());
            int candidate = (lhs / base) * base + rhs;
            if (candidate < lhs)
            {
                candidate += base;
            }
            rhs = candidate;
        }

        if (rhs < lhs)
        {
            std::swap(lhs, rhs);
        }

        for (int value = lhs; value <= rhs; value++)
        {
            if (value > 0 && !seen.contains(value))
            {
                seen.insert(value);
                ids.push_back(value);
            }
        }
    }

    return ids;
}

} // namespace runeharbor::formats
