// SPDX-License-Identifier: MIT
#include "items_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

ItemsParser::ItemsParser(util::ILogger& logger) : logger(logger) {}

bool ItemsParser::parse(const std::vector<uint8_t>& data)
{
    items.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Items data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Skip main column header
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read header line.");
        return false;
    }
    const std::string expected_header = "Item #\tPic File\tName\tValue\tEquip Stat\tSkill "
                                        "Group\tMod1\tMod2\tmaterial\tID/Rep/St\tNot identified "
                                        "name\tSprite Index\tVarA\tVarB\tEquip X\tEquip Y\tNotes";
    if (line != expected_header)
    {
        logger.error(
            std::format("Malformed header: Expected '{}', got '{}'", expected_header, line));
        return false;
    }
    logger.debug(std::format("Skipping main column header: {}", line));

    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty())
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        // Data line
        ItemEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect at least 17 fields based on items.md analysis
        // Item #	Pic File	Name	Value	Equip Stat	Skill Group	Mod1	Mod2
        // material	ID/Rep/St	Not identified name	Sprite Index	VarA	VarB
        // Equip X	Equip Y	Notes
        if (fields.size() < 17)
        {
            logger.warning(std::format("Skipping malformed item line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. id
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.id = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. picFile
            if (fieldIndex < fields.size())
                entry.picFile = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. name
            if (fieldIndex < fields.size())
                entry.name = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 4. value
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.value = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 5. equipStat
            if (fieldIndex < fields.size())
                entry.equipStat = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 6. skillGroup
            if (fieldIndex < fields.size())
                entry.skillGroup = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 7. mod1
            if (fieldIndex < fields.size())
                entry.mod1 = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 8. mod2
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.mod2 = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 9. material
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.material = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 10. idRepSt
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.idRepSt = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 11. notIdentifiedName
            if (fieldIndex < fields.size())
                entry.notIdentifiedName = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 12. spriteIndex
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.spriteIndex = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 13. varA
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.varA = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 14. varB
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.varB = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 15. equipX
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.equipX = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 16. equipY
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.equipY = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 17. notes
            if (fieldIndex < fields.size())
                entry.notes = util::trim(fields[fieldIndex]);
            fieldIndex++;

            items.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing item line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} item entries", items.size()));
    return true;
}

} // namespace runeharbor::formats
