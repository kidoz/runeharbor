// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline

#include "monsters_parser.hpp"

namespace runeharbor::formats
{

MonstersParser::MonstersParser(util::ILogger& logger) : logger(logger) {}

bool MonstersParser::parse(const std::vector<uint8_t>& data)
{
    monsters.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Monsters data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Skip the first line (category header)
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read category header line.");
        return false;
    }
    logger.debug(std::format("Skipping category header: {}", line));

    // Read and validate the actual field names header line
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read field names header line.");
        return false;
    }
    const std::string expected_field_header =
        R"(#	Name	Picture	LVL	 HP 	AC	 EXP 	Treasure	Quest	Fly	Move	AI Type	Hst	Spd	Rec	Pref	Bonus	Type	Damage	Miss	Att%	Type	Damage	Miss	Use%	"Spl,Mas,Skil"	Use%	"Spl,Mas,Skil"	Fire	Air	Water	Earth	Mind	Spirit	Body	Light	Dark	Phys	Special)";
    if (util::trim(line) != expected_field_header)
    {
        logger.error(std::format("Malformed field names header: Expected '{}', got '{}'",
                                 expected_field_header, util::trim(line)));
        return false;
    }
    logger.debug(std::format("Skipping field names header: {}", line));

    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty())
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        MonsterEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect exactly 39 fields based on monsters.md analysis
        if (fields.size() < 39) // Use < 39 for robustness, allowing for trailing empty fields not
                                // explicitly counted
        {
            logger.warning(
                std::format("Skipping malformed monster line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. id
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.id = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. name
            if (fieldIndex < fields.size())
                entry.name = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. picture
            if (fieldIndex < fields.size())
                entry.picture = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 4. level
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.level = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 5. hitPoints (clean numeric string)
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.hitPoints =
                    std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 6. armorClass
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.armorClass = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 7. experience (clean numeric string)
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.experience =
                    std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 8. treasure
            if (fieldIndex < fields.size())
                entry.treasure = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 9. quest
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.quest = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 10. canFly
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.canFly = (util::trim(fields[fieldIndex]) == "Y");
            fieldIndex++;

            // 11. moveType
            if (fieldIndex < fields.size())
                entry.moveType = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 12. aiType
            if (fieldIndex < fields.size())
                entry.aiType = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 13. haste
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.haste = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 14. speed
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.speed = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 15. recovery
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.recovery = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 16. preferences
            if (fieldIndex < fields.size())
                entry.preferences = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 17. bonus
            if (fieldIndex < fields.size())
                entry.bonus = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Attack 1
            // 18. attack1.type
            if (fieldIndex < fields.size())
                entry.attack1.type = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 19. attack1.damage
            if (fieldIndex < fields.size())
                entry.attack1.damage = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 20. attack1.miss
            if (fieldIndex < fields.size())
                entry.attack1.miss = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 21. attack1.attPercent
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.attack1.attPercent = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // Attack 2
            // 22. attack2.type
            if (fieldIndex < fields.size())
                entry.attack2.type = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 23. attack2.damage
            if (fieldIndex < fields.size())
                entry.attack2.damage = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 24. attack2.miss
            if (fieldIndex < fields.size())
                entry.attack2.miss = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Spell Attack 1
            // 25. spellAttack1.usePercent
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.spellAttack1.usePercent = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;
            // 26. spellAttack1.spellMasterySkill
            if (fieldIndex < fields.size())
                entry.spellAttack1.spellMasterySkill = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Spell Attack 2
            // 27. spellAttack2.usePercent
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.spellAttack2.usePercent = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;
            // 28. spellAttack2.spellMasterySkill
            if (fieldIndex < fields.size())
                entry.spellAttack2.spellMasterySkill = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Resistances
            // 29. resistFire
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistFire =
                    (s == "Imm" ? 100 : std::stoi(s)); // Assuming Imm means 100% resistance
            }
            fieldIndex++;
            // 30. resistAir
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistAir = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 31. resistWater
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistWater = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 32. resistEarth
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistEarth = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 33. resistMind
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistMind = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 34. resistSpirit
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistSpirit = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 35. resistBody
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistBody = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 36. resistLight
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistLight = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 37. resistDark
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
            {
                std::string s = util::trim(fields[fieldIndex]);
                entry.resistDark = (s == "Imm" ? 100 : std::stoi(s));
            }
            fieldIndex++;
            // 38. resistPhysical
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty())
                entry.resistPhysical = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 39. special
            if (fieldIndex < fields.size())
                entry.special = util::trim(fields[fieldIndex]);
            fieldIndex++;

            monsters.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing monster line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} monster entries", monsters.size()));
    return true;
}

} // namespace runeharbor::formats
