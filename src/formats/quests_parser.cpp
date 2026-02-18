// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "quests_parser.hpp"

namespace runeharbor::formats
{

QuestsParser::QuestsParser(util::ILogger& logger) : logger(logger) {}

bool QuestsParser::parse(const std::vector<uint8_t>& data)
{
    quests.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Quests data is empty");
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
    const std::string expected_header = "Q Bit	Quest Note Text	Notes	Owner";
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

        QuestEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect exactly 4 fields based on quests.md analysis
        if (fields.size() < 4) // Use < 4 for robustness
        {
            logger.warning(std::format("Skipping malformed quest line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. qBit
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.qBit = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. questNoteText
            if (fieldIndex < fields.size())
                entry.questNoteText = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. notes
            if (fieldIndex < fields.size())
                entry.notes = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 4. owner
            if (fieldIndex < fields.size())
                entry.owner = util::trim(fields[fieldIndex]);
            fieldIndex++;

            quests.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing quest line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} quest entries", quests.size()));
    return true;
}

} // namespace runeharbor::formats
