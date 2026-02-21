// SPDX-License-Identifier: MIT
#include "rnditems_parser.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <string>
#include <string_view>

#include "../util/ilogger.hpp"
#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

namespace
{
bool parseInt(std::string_view raw, int& value)
{
    const std::string cleaned = util::cleanNumericString(util::trim(std::string(raw)));
    if (cleaned.empty())
    {
        return false;
    }

    const char* begin = cleaned.data();
    const char* end = cleaned.data() + cleaned.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    return ec == std::errc{} && ptr == end;
}

bool looksLikeCommentOrHeader(std::string_view line)
{
    if (line.empty())
    {
        return true;
    }
    if (line[0] == ';' || line[0] == '#')
    {
        return true;
    }

    if (line.size() >= 2 && line[0] == '/' && line[1] == '/')
    {
        return true;
    }

    // Most rnditems headers are textual; skip non-numeric first token.
    int firstValue = 0;
    const size_t split = line.find_first_of("\t,");
    const std::string token = util::trim(std::string(line.substr(0, split)));
    return !parseInt(token, firstValue);
}
} // namespace

RndItemsParser::RndItemsParser(util::ILogger& logger) : logger_(logger) {}

bool RndItemsParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();

    if (data.empty())
    {
        logger_.warning("RndItems parser: input is empty");
        return false;
    }

    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(text);
    std::string line;
    bool firstLine = true;

    while (std::getline(stream, line))
    {
        if (firstLine && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
        {
            line = line.substr(3);
        }
        firstLine = false;

        const std::string trimmed = util::trim(line);
        if (looksLikeCommentOrHeader(trimmed))
        {
            continue;
        }

        std::vector<std::string> columns = util::splitString(trimmed, '\t', '"');
        if (columns.size() < 4)
        {
            columns = util::splitString(trimmed, ',', '"');
        }
        if (columns.size() < 4)
        {
            continue;
        }

        RndItemEntry entry;
        if (!parseInt(columns[0], entry.itemId))
        {
            continue;
        }

        int baseLevel = 0;
        if (columns.size() > 2 && parseInt(columns[2], baseLevel))
        {
            entry.baseLevel = baseLevel;
        }

        bool hasWeight = false;
        for (int level = 0; level < 6; level++)
        {
            const size_t col = static_cast<size_t>(3 + level);
            if (col >= columns.size())
            {
                break;
            }

            int weight = 0;
            if (!parseInt(columns[col], weight))
            {
                continue;
            }

            entry.levelWeights[static_cast<size_t>(level)] = std::max(0, weight);
            hasWeight = hasWeight || weight > 0;
        }

        if (!hasWeight)
        {
            continue;
        }

        entries_.push_back(entry);
    }

    logger_.info("RndItems parser: loaded " + std::to_string(entries_.size()) + " weighted items");
    return !entries_.empty();
}

} // namespace runeharbor::formats
