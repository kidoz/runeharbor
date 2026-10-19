// SPDX-License-Identifier: MIT
#include "placemon_parser.hpp"

#include <charconv>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../util/ilogger.hpp"
#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

namespace
{
bool parseInt(std::string_view raw, int& value)
{
    std::string token = util::trim(std::string(raw));
    if (token.empty())
    {
        return false;
    }

    if (token.front() == '"' && token.back() == '"' && token.size() >= 2)
    {
        token = token.substr(1, token.size() - 2);
    }

    token = util::cleanNumericString(token);
    if (token.empty())
    {
        return false;
    }

    const char* begin = token.data();
    const char* end = token.data() + token.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    return ec == std::errc{} && ptr == end;
}

std::vector<std::string> splitColumns(const std::string& line)
{
    std::vector<std::string> cols = util::splitString(line, '\t', '"');
    if (cols.size() < 2)
    {
        cols = util::splitString(line, ',', '"');
    }
    if (cols.size() < 2)
    {
        std::istringstream stream(line);
        cols.clear();
        std::string word;
        while (stream >> word)
        {
            cols.push_back(word);
        }
    }
    return cols;
}
} // namespace

PlacemonParser::PlacemonParser(util::ILogger& logger) : logger_(logger) {}

bool PlacemonParser::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();

    if (data.empty())
    {
        logger_.warning("Placemon parser: input is empty");
        return false;
    }

    const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
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
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
        {
            continue;
        }
        if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/')
        {
            continue;
        }

        std::vector<std::string> cols = splitColumns(trimmed);
        if (cols.size() < 2)
        {
            continue;
        }

        int id = 0;
        if (!parseInt(cols[0], id) || id <= 0)
        {
            // Header row ("\tPlaced Monster Names") and blank/invalid ids.
            continue;
        }

        std::string name = util::trim(cols[1]);
        if (cols.size() > 2)
        {
            // Whitespace fallback splits the name; rejoin the remaining columns.
            for (size_t i = 2; i < cols.size(); ++i)
            {
                const std::string part = util::trim(cols[i]);
                if (!part.empty())
                {
                    name += ' ';
                    name += part;
                }
            }
        }

        if (name.empty() || name == "0")
        {
            continue;
        }

        entries_.push_back({id, std::move(name)});
    }

    logger_.info("Placemon parser: loaded " + std::to_string(entries_.size()) + " entries");
    return !entries_.empty();
}

} // namespace runeharbor::formats
