// SPDX-License-Identifier: MIT
#include "two_d_events_parser.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <sstream>
#include <string>

#include <cctype>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

namespace
{

std::optional<int> parseIntStrict(std::string_view text)
{
    const std::string trimmed = util::trim(std::string(text));
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    int value = 0;
    const char* begin = trimmed.data();
    const char* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
    {
        return std::nullopt;
    }
    return value;
}

} // namespace

TwoDEventsParser::TwoDEventsParser(util::ILogger& logger) : logger_(logger) {}

bool TwoDEventsParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();

    if (data.empty())
    {
        logger_.error("2dEvents data is empty");
        return false;
    }

    const std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(content);
    std::string line;
    int lineNo = 0;

    while (std::getline(stream, line))
    {
        lineNo++;
        const std::string trimmed = util::trim(line);
        if (trimmed.empty())
        {
            continue;
        }
        if (trimmed[0] == ';' || trimmed[0] == '#')
        {
            continue;
        }

        std::vector<std::string> fields = util::splitString(line, '\t', '"');
        if (fields.empty())
        {
            continue;
        }

        auto idOpt = parseIntStrict(fields[0]);
        if (!idOpt.has_value())
        {
            // Most files include one or more non-numeric header rows.
            continue;
        }

        TwoDEventEntry entry;
        entry.id = *idOpt;

        std::string firstText;
        std::string lastText;
        entry.columns.reserve(fields.size());

        for (size_t i = 1; i < fields.size(); i++)
        {
            const std::string value = util::trim(fields[i]);
            entry.columns.push_back(value);

            if (value.empty())
            {
                continue;
            }

            if (firstText.empty() && containsAlpha(value))
            {
                firstText = value;
            }

            if (containsAlpha(value))
            {
                lastText = value;
            }
        }

        if (firstText.empty())
        {
            for (const auto& column : entry.columns)
            {
                if (!column.empty())
                {
                    firstText = column;
                    break;
                }
            }
        }

        entry.category = firstText;
        entry.displayName = !lastText.empty() ? lastText : firstText;

        if (entry.displayName.empty())
        {
            logger_.warning(std::format("Skipping 2dEvents line {}: no display fields", lineNo));
            continue;
        }

        entries_.push_back(std::move(entry));
    }

    logger_.info(std::format("Parsed {} 2dEvents entries", entries_.size()));
    return true;
}

const TwoDEventEntry* TwoDEventsParser::findById(int id) const
{
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [id](const TwoDEventEntry& entry) { return entry.id == id; });
    return it != entries_.end() ? &(*it) : nullptr;
}

bool TwoDEventsParser::containsAlpha(std::string_view text)
{
    return std::any_of(text.begin(), text.end(),
                       [](unsigned char c) { return std::isalpha(c) != 0; });
}

} // namespace runeharbor::formats
