// SPDX-License-Identifier: MIT
#include "autonote_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

AutonoteParser::AutonoteParser(util::ILogger& logger) : logger(logger) {}

bool AutonoteParser::parse(const std::vector<uint8_t>& data)
{
    entries.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Autonote data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Skip header line
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read header line.");
        return false;
    }
    const std::string expected_header = "Note bit	Autonote Text	Category";
    if (util::trim(line) != expected_header)
    {
        logger.error(std::format("Malformed header: Expected '{}', got '{}'", expected_header,
                                 util::trim(line)));
        return false;
    }
    logger.debug(std::format("Skipping header: {}", line));

    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty())
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        AutonoteEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect at least 3 fields. More fields will be ignored.
        if (fields.size() < 3)
        {
            logger.warning(
                std::format("Skipping malformed Autonote line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. Note bit
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.noteBit = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. Autonote Text
            if (fieldIndex < fields.size())
                entry.autonoteText = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. Category
            if (fieldIndex < fields.size())
                entry.category = util::trim(fields[fieldIndex]);
            fieldIndex++;

            entries.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing Autonote line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} Autonote entries", entries.size()));
    return true;
}

} // namespace runeharbor::formats
