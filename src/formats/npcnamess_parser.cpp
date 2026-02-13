#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "npcnamess_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline


namespace runeharbor::formats
{

NPCNamesParser::NPCNamesParser(util::ILogger& logger) : logger(logger) {}

bool NPCNamesParser::parse(const std::vector<uint8_t>& data)
{
    names.maleNames.clear(); // Clear any previous data
    names.femaleNames.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("NPCNames data is empty");
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
    const std::string expected_header = "Male	Female";
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

        std::vector<std::string> fields = util::splitString(line, '\t'); // Split by tab, no quotes

        // We expect exactly 2 fields
        if (fields.size() != 2)
        {
            logger.warning(std::format("Skipping malformed NPCNames line (expected 2 fields, got {}): {}", fields.size(), line));
            continue;
        }

        // Male Name (first field)
        std::string male_name = (fields.size() > 0) ? util::trim(fields[0]) : "";
        names.maleNames.push_back(male_name);

        // Female Name (second field, optional)
        std::string female_name = (fields.size() > 1) ? util::trim(fields[1]) : "";
        names.femaleNames.push_back(female_name);
    }

    logger.info(std::format("Successfully parsed {} male and {} female NPC names", names.maleNames.size(), names.femaleNames.size()));
    return true;
}

} // namespace runeharbor::formats
