// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "event_file_parser.hpp"

namespace runeharbor::formats
{

EventFileParser::EventFileParser(util::ILogger& logger) : logger(logger) {}

bool EventFileParser::parse(const std::vector<uint8_t>& data)
{
    events.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Event file data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Skip header lines (first two lines)
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read first header line.");
        return false;
    }
    logger.debug(std::format("Skipping header line 1: {}", line));

    if (!std::getline(iss, line))
    {
        logger.error("Failed to read second header line.");
        return false;
    }
    logger.debug(std::format("Skipping header line 2: {}", line));

    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty()) // Use util::trim for consistency
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        logger.debug(std::format("Processing line (raw): '{}'", line));

        EventEntry entry;
        std::vector<std::string> fields;
        // Split line by tab delimiter using the utility function
        fields = util::splitString(line, '\t');

        // Check if there are enough fields (minimum for basic data)
        // Based on preliminary analysis, we expect at least 8 fields for basic identification
        // # # Type Map Picture Name Proprietor Name Title
        if (fields.size() < 8)
        {
            logger.warning(std::format("Skipping malformed event line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. mainId
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.mainId = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. subId
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.subId = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 3. type
            if (fieldIndex < fields.size())
                entry.type = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 4. mapId
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.mapId = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 5. pictureId
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.pictureId = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 6. name
            if (fieldIndex < fields.size())
                entry.name = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 7. proprietorName
            if (fieldIndex < fields.size())
                entry.proprietorName = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 8. title
            if (fieldIndex < fields.size())
                entry.title = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Fields 9-16
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field9_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field10_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field11_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field12_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field13_float = std::stof(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field14_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size())
                entry.field15_str = util::trim(fields[fieldIndex]);
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field16_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // Fields 17-24
            if (fieldIndex < fields.size())
                entry.field17_str = util::trim(fields[fieldIndex]);
            fieldIndex++;

            if (fieldIndex < fields.size())
                entry.field18_str = util::trim(fields[fieldIndex]);
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field19_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field20_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field21_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field22_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size())
                entry.field23_str = util::trim(fields[fieldIndex]);
            fieldIndex++;

            if (fieldIndex < fields.size())
                entry.field24_str = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // New fields: field25_int to field30_int
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field25_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field26_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field27_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field28_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field29_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.field30_int = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            events.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing event line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} event entries", events.size()));
    return true;
}

} // namespace runeharbor::formats
