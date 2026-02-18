// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "credits_parser.hpp"

namespace runeharbor::formats
{

CreditsParser::CreditsParser(util::ILogger& logger) : logger(logger) {}

bool CreditsParser::parse(const std::vector<uint8_t>& data)
{
    sections.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Credits data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    CreditsSection current_section;

    while (std::getline(iss, line))
    {
        std::string trimmed_line = util::trim(line);

        if (trimmed_line.empty())
        {
            // Ignore blank lines
            continue;
        }

        if (trimmed_line.length() > 0 && trimmed_line[0] == '_')
        {
            // This is a new heading
            if (!current_section.title.empty() || !current_section.content.empty())
            {
                // Save the previous section if it's not empty
                sections.push_back(current_section);
            }
            // Start a new section
            current_section = CreditsSection();             // Reset
            current_section.title = trimmed_line.substr(1); // Remove the leading '_'
        }
        else
        {
            // This is content for the current section
            if (current_section.title.empty())
            { // If no section started yet, create "Introduction"
                current_section.title = "Introduction";
            }
            current_section.content.push_back(trimmed_line);
        }
    }

    // Push the last section after the loop finishes
    if (!current_section.title.empty() || !current_section.content.empty())
    {
        sections.push_back(current_section);
    }

    logger.info(std::format("Successfully parsed {} credits sections", sections.size()));
    return true;
}

} // namespace runeharbor::formats
