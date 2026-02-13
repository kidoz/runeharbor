#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "global_text_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline


namespace runeharbor::formats
{

GlobalTextParser::GlobalTextParser(util::ILogger& logger) : logger(logger) {}

bool GlobalTextParser::parse(const std::vector<uint8_t>& data)
{
    textEntries.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Global text data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Skip header line "Global Text"
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read header line.");
        return false;
    }
    if (line != "Global Text") {
        logger.warning(std::format("Expected 'Global Text' header, got '{}'. Attempting to parse anyway.", line));
        // Reset stream and re-add the line to be parsed as data if it's not the header.
        iss.seekg(0, std::ios::beg);
        iss.clear(); // Clear EOF flags
        iss.str(content); // Reset content
        std::getline(iss, line); // Read the first line again as a data line
    }


    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty()) // Use util::trim for consistency
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        std::vector<std::string> fields = util::splitString(line, '\t');

        if (fields.size() < 2)
        {
            logger.warning(std::format("Skipping malformed global text line (too few fields): {}", line));
            continue;
        }

        try
        {
            int id = std::stoi(util::trim(fields[0]));
            std::string text = util::trim(fields[1]);
            textEntries[id] = text;
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing global text line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} global text entries", textEntries.size()));
    return true;
}

std::optional<std::string> GlobalTextParser::getText(int id) const
{
    auto it = textEntries.find(id);
    if (it != textEntries.end())
    {
        return it->second;
    }
    return std::nullopt;
}

} // namespace runeharbor::formats