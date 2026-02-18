// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "classes_parser.hpp"

namespace runeharbor::formats
{

ClassesParser::ClassesParser(util::ILogger& logger) : logger(logger) {}

bool ClassesParser::parse(const std::vector<uint8_t>& data)
{
    classes.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Classes data is empty");
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
    const std::string expected_header = "Class	Descriptions	Notes";
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

        ClassEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect exactly 3 fields based on classes.md analysis
        if (fields.size() < 3) // Use < 3 for robustness
        {
            logger.warning(std::format("Skipping malformed class line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. className
            if (fieldIndex < fields.size())
                entry.className = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 2. description
            if (fieldIndex < fields.size())
                entry.description = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. notes
            if (fieldIndex < fields.size())
                entry.notes = util::trim(fields[fieldIndex]);
            fieldIndex++;

            classes.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing class line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} class entries", classes.size()));
    return true;
}

} // namespace runeharbor::formats
