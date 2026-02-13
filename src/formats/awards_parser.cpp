#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "awards_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline


namespace runeharbor::formats
{

AwardsParser::AwardsParser(util::ILogger& logger) : logger(logger) {}

bool AwardsParser::parse(const std::vector<uint8_t>& data)
{
    awards.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Awards data is empty");
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
    const std::string expected_header = "A Bit	Awards	Sort	Notes";
    if (util::trim(line) != expected_header)
    {
        logger.error(std::format("Malformed header: Expected '{}', got '{}'", expected_header, util::trim(line)));
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

        AwardEntry entry;
        std::vector<std::string> fields = util::splitString(line, '\t', '"'); // Split by tab, handle quotes

        // We expect at least 3 fields, making the 'Notes' field optional.
        if (fields.size() < 3) // Use < 3 for robustness
        {
            logger.warning(std::format("Skipping malformed award line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. aBit
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.aBit = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. awardText
            if (fieldIndex < fields.size()) entry.awardText = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. sortOrder
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.sortOrder = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 4. notes
            if (fieldIndex < fields.size()) entry.notes = util::trim(fields[fieldIndex]);
            fieldIndex++;
            
            awards.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing award line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} award entries", awards.size()));
    return true;
}

} // namespace runeharbor::formats
