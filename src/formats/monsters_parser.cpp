// SPDX-License-Identifier: MIT
#include "monsters_parser.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <sstream> // For std::istringstream and std::getline
#include <string_view>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

namespace
{

std::optional<int> parseInt(std::string_view text)
{
    const std::string trimmed = util::trim(std::string(text));
    if (trimmed.empty())
    {
        return 0;
    }

    int value = 0;
    const char* begin = trimmed.data();
    const char* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
    {
        return std::nullopt;
    }
    return value;
}

} // namespace

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
        // Some broken extraction paths can prepend binary data before MONSTERS.TXT content.
        // Recover by searching for the known field header inside the payload when
        // the prefix clearly looks non-textual.
        bool hasBinaryPrefix = false;
        const size_t probeLen = std::min<size_t>(content.size(), 512);
        for (size_t i = 0; i < probeLen; i++)
        {
            const unsigned char c = static_cast<unsigned char>(content[i]);
            if (c == '\n' || c == '\r' || c == '\t')
            {
                continue;
            }
            if (c < 0x20 || c >= 0x80)
            {
                hasBinaryPrefix = true;
                break;
            }
        }

        if (!hasBinaryPrefix)
        {
            logger.error(std::format("Malformed field names header: Expected '{}', got '{}'",
                                     expected_field_header, util::trim(line)));
            return false;
        }

        size_t headerPos = content.find(expected_field_header);
        if (headerPos == std::string::npos)
        {
            logger.error(std::format("Malformed field names header: Expected '{}', got '{}'",
                                     expected_field_header, util::trim(line)));
            return false;
        }

        logger.warning("Monsters payload had binary prefix; recovering from embedded table header");
        iss.clear();
        iss.str(content.substr(headerPos));

        // Consume the recovered header line so the data loop starts from the first monster entry.
        if (!std::getline(iss, line))
        {
            logger.error("Failed to recover monsters header line");
            return false;
        }
    }
    logger.debug(std::format("Skipping field names header: {}", util::trim(line)));

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

            const auto parseNum = [&](size_t index, const char* fieldName, bool clean = false,
                                      int fallback = 0) -> int
            {
                if (index >= fields.size())
                {
                    return fallback;
                }
                const std::string raw = util::trim(fields[index]);
                if (raw.empty())
                {
                    return fallback;
                }
                const std::string candidate = clean ? util::cleanNumericString(raw) : raw;
                if (const auto parsed = parseInt(candidate))
                {
                    return *parsed;
                }
                logger.warning(std::format("Monster {} field '{}' ('{}') is not numeric", entry.id,
                                           fieldName, raw));
                return fallback;
            };

            // 1. id
            entry.id = parseNum(fieldIndex, "id");
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
            entry.level = parseNum(fieldIndex, "level");
            fieldIndex++;

            // 5. hitPoints (clean numeric string; retail uses "1,300")
            entry.hitPoints = parseNum(fieldIndex, "hitPoints", true);
            fieldIndex++;

            // 6. armorClass
            entry.armorClass = parseNum(fieldIndex, "armorClass");
            fieldIndex++;

            // 7. experience (clean numeric string; retail uses "11,000")
            entry.experience = parseNum(fieldIndex, "experience", true);
            fieldIndex++;

            // 8. treasure
            if (fieldIndex < fields.size())
                entry.treasure = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 9. quest
            entry.quest = parseNum(fieldIndex, "quest");
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
            entry.haste = parseNum(fieldIndex, "haste");
            fieldIndex++;

            // 14. speed
            entry.speed = parseNum(fieldIndex, "speed");
            fieldIndex++;

            // 15. recovery
            entry.recovery = parseNum(fieldIndex, "recovery");
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
            entry.attack1.attPercent = parseNum(fieldIndex, "attack1.attPercent");
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
            entry.spellAttack1.usePercent = parseNum(fieldIndex, "spellAttack1.usePercent");
            fieldIndex++;
            // 26. spellAttack1.spellMasterySkill
            if (fieldIndex < fields.size())
                entry.spellAttack1.spellMasterySkill = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Spell Attack 2
            // 27. spellAttack2.usePercent
            entry.spellAttack2.usePercent = parseNum(fieldIndex, "spellAttack2.usePercent");
            fieldIndex++;
            // 28. spellAttack2.spellMasterySkill
            if (fieldIndex < fields.size())
                entry.spellAttack2.spellMasterySkill = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Resistances. "Imm" means 100% resistance.
            const auto parseResist = [&](const char* fieldName, int& target)
            {
                if (fieldIndex >= fields.size())
                {
                    fieldIndex++;
                    return;
                }
                const std::string raw = util::trim(fields[fieldIndex]);
                if (!raw.empty())
                {
                    if (raw == "Imm")
                    {
                        target = 100;
                    }
                    else
                    {
                        target = parseNum(fieldIndex, fieldName);
                    }
                }
                fieldIndex++;
            };

            parseResist("resistFire", entry.resistFire);
            parseResist("resistAir", entry.resistAir);
            parseResist("resistWater", entry.resistWater);
            parseResist("resistEarth", entry.resistEarth);
            parseResist("resistMind", entry.resistMind);
            parseResist("resistSpirit", entry.resistSpirit);
            parseResist("resistBody", entry.resistBody);
            parseResist("resistLight", entry.resistLight);
            parseResist("resistDark", entry.resistDark);
            parseResist("resistPhysical", entry.resistPhysical);

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
