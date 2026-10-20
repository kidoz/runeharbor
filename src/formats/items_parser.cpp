// SPDX-License-Identifier: MIT
#include "items_parser.hpp"

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

// Strict integer parse after trimming. std::stoi throws on "Artifact" /
// "of Alchemy", which used to drop entire artifact/relic/special rows.
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
    // The shipped ITEMS.TXT uses CRLF line endings. std::getline leaves the
    // trailing carriage return in place, so compare the trimmed line.
    const std::string header = util::trim(line);
    if (header != expected_header)
    {
        logger.error(
            std::format("Malformed header: Expected '{}', got '{}'", expected_header, header));
        return false;
    }
    logger.debug(std::format("Skipping main column header: {}", header));

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

            const auto parseNum = [&](size_t index, const char* fieldName, int fallback = 0) -> int
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
                if (const auto parsed = parseInt(raw))
                {
                    return *parsed;
                }
                logger.warning(std::format("Item {} field '{}' ('{}') is not numeric", entry.id,
                                           fieldName, raw));
                return fallback;
            };

            // 1. id
            entry.id = parseNum(fieldIndex, "id");
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
            entry.value = parseNum(fieldIndex, "value");
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
            entry.mod2 = parseNum(fieldIndex, "mod2");
            fieldIndex++;

            // 9. material: numeric for ordinary items, but artifacts/relics/
            // specials put a quality token here ("Artifact", "Relic", "Special").
            if (fieldIndex < fields.size())
            {
                const std::string raw = util::trim(fields[fieldIndex]);
                if (const auto parsed = parseInt(raw))
                {
                    entry.material = *parsed;
                }
                else
                {
                    entry.materialName = raw;
                }
            }
            fieldIndex++;

            // 10. idRepSt
            entry.idRepSt = parseNum(fieldIndex, "idRepSt");
            fieldIndex++;

            // 11. notIdentifiedName
            if (fieldIndex < fields.size())
                entry.notIdentifiedName = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 12. spriteIndex
            entry.spriteIndex = parseNum(fieldIndex, "spriteIndex");
            fieldIndex++;

            // 13. varA: numeric for ordinary items, but special-enchant rows
            // put a name here ("of Alchemy", "of the Dragon").
            if (fieldIndex < fields.size())
            {
                const std::string raw = util::trim(fields[fieldIndex]);
                if (const auto parsed = parseInt(raw))
                {
                    entry.varA = *parsed;
                }
                else
                {
                    entry.varAName = raw;
                }
            }
            fieldIndex++;

            // 14. varB
            entry.varB = parseNum(fieldIndex, "varB");
            fieldIndex++;

            // 15. equipX
            entry.equipX = parseNum(fieldIndex, "equipX");
            fieldIndex++;

            // 16. equipY
            entry.equipY = parseNum(fieldIndex, "equipY");
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
