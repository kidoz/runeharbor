// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "spells_parser.hpp"

namespace runeharbor::formats
{

SpellsParser::SpellsParser(util::ILogger& logger) : logger(logger) {}

bool SpellsParser::parse(const std::vector<uint8_t>& data)
{
    spells.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Spells data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty())
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        // Skip section headers (e.g., "	Lvl	Fire Spells	Res		Spell
        // Description")
        if (line.length() >= 4 && line[0] == '	' && line[1] == 'L' && line[2] == 'v' &&
            line[3] == 'l')
        {
            logger.debug(std::format("Skipping section header: {}", line));
            continue;
        }

        // Skip main column header (e.g., "#	Lvl	Fire Spells	Res	Short Name...")
        if (line.length() > 0 && line[0] == '#')
        {
            logger.debug(std::format("Skipping main column header: {}", line));
            continue;
        }

        // Data line
        SpellEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect at least 11 fields based on spells.md analysis
        // # Lvl Name Res Short Name Description Normal Expert Master Grand Master Stats
        if (fields.size() < 11)
        {
            logger.warning(std::format("Skipping malformed spell line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. ID
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.id = std::stoi(fields[fieldIndex]);
            fieldIndex++;

            // 2. Lvl
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.level = std::stoi(fields[fieldIndex]);
            fieldIndex++;

            // 3. Name (Spell Name)
            if (fieldIndex < fields.size())
                entry.name = fields[fieldIndex];
            fieldIndex++;

            // 4. Res (Resistance)
            if (fieldIndex < fields.size())
                entry.resistance = fields[fieldIndex];
            fieldIndex++;

            // 5. Short Name
            if (fieldIndex < fields.size())
                entry.shortName = fields[fieldIndex];
            fieldIndex++;

            // 6. Description
            if (fieldIndex < fields.size())
                entry.description = fields[fieldIndex];
            fieldIndex++;

            // 7. Normal Effect
            if (fieldIndex < fields.size())
                entry.normalEffect = fields[fieldIndex];
            fieldIndex++;

            // 8. Expert Effect
            if (fieldIndex < fields.size())
                entry.expertEffect = fields[fieldIndex];
            fieldIndex++;

            // 9. Master Effect
            if (fieldIndex < fields.size())
                entry.masterEffect = fields[fieldIndex];
            fieldIndex++;

            // 10. Grand Master Effect
            if (fieldIndex < fields.size())
                entry.grandMasterEffect = fields[fieldIndex];
            fieldIndex++;

            // 11. Stats
            if (fieldIndex < fields.size())
                entry.stats = fields[fieldIndex];
            fieldIndex++;

            spells.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing spell line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} spell entries", spells.size()));
    return true;
}

} // namespace runeharbor::formats
