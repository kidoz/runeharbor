// SPDX-License-Identifier: MIT
#include "two_d_events_parser.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <sstream>
#include <string>

#include <cctype>

#include "../game/building_type.hpp"
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

std::optional<float> parseFloatStrict(std::string_view text)
{
    // std::from_chars for float is not consistently available across the
    // supported toolchains; parse via strtod on a trimmed copy.
    const std::string trimmed = util::trim(std::string(text));
    if (trimmed.empty())
    {
        return std::nullopt;
    }
    try
    {
        size_t pos = 0;
        const float value = std::stof(trimmed, &pos);
        if (pos != trimmed.size())
        {
            return std::nullopt;
        }
        return value;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

// Safe column accessor: returns the trimmed value or empty if out of range.
// `columns` is the fields[1..] vector (column 0 == fields[1]).
std::string columnAt(const std::vector<std::string>& columns, size_t index)
{
    if (index < columns.size())
    {
        return columns[index];
    }
    return {};
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

        // Decode typed fields from the columns vector. Column indices are
        // RE-derived from `2dEvents.txt` (see docs/re/29-shops-and-economy.md
        // section 2.1). columns[N] == fields[N+1].
        entry.buildingType = game::buildingTypeFromName(columnAt(entry.columns, 1));
        if (auto v = parseIntStrict(columnAt(entry.columns, 0)))
            entry.perTypeIndex = *v;
        if (auto v = parseIntStrict(columnAt(entry.columns, 2)))
            entry.mapId = *v;
        if (auto v = parseIntStrict(columnAt(entry.columns, 3)))
            entry.pictureId = *v;
        entry.name = columnAt(entry.columns, 4);
        entry.proprietorName = columnAt(entry.columns, 5);
        entry.title = columnAt(entry.columns, 6);
        if (auto v = parseFloatStrict(columnAt(entry.columns, 11)))
            entry.buyMultiplier = *v;
        if (auto v = parseFloatStrict(columnAt(entry.columns, 12)))
            entry.secondaryMultiplier = *v;
        if (auto v = parseIntStrict(columnAt(entry.columns, 14)))
            entry.serviceSeed = *v;
        if (auto v = parseIntStrict(columnAt(entry.columns, 17)))
            entry.openHour = *v;
        if (auto v = parseIntStrict(columnAt(entry.columns, 18)))
            entry.closedHour = *v;

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
