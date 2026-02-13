#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "mapstats_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline


namespace runeharbor::formats
{

MapStatsParser::MapStatsParser(util::ILogger& logger) : logger(logger) {}

bool MapStatsParser::parse(const std::vector<uint8_t>& data)
{
    mapStats.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("MapStats data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Skip the first two lines (category headers)
    if (!std::getline(iss, line) || !std::getline(iss, line))
    {
        logger.error("Failed to read category header lines.");
        return false;
    }
    logger.debug("Skipping category headers.");

    // Read and validate the actual field names header line (third line)
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read field names header line.");
        return false;
    }
    // Using raw string literal for the expected header for exact match and easier handling of quotes/tabs
    const std::string expected_field_header = R"(#	Name	File name	#	Day	0-20	Days	Days	Perm	0-20	0-10	0-6	%	%	%	%	Mon1 Pic	Mon 1	 1-5	#	Mon2 Pic	Mon 2	 1-5	#	Mon3 Pic	Mon 3	 1-5	#	Track	EAX Environments	Map Designer	Notes	in area)";
    if (util::trim(line) != expected_field_header)
    {
        logger.error(std::format("Malformed field names header: Expected '{}', got '{}'", expected_field_header, util::trim(line)));
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

        MapStatsEntry entry;
        std::vector<std::string> fields = util::splitString(line, '	', '"'); // Split by tab, handle quotes

        // We expect at least 33 fields, making the last field 'inArea' optional.
        if (fields.size() < 32) // Use < 32 for robustness
        {
            logger.warning(std::format("Skipping malformed mapstats line (too few fields): {}", line));
            continue;
        }

        try
        {
            size_t fieldIndex = 0;

            // 1. id
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.id = std::stoi(util::trim(fields[fieldIndex]));
            fieldIndex++;

            // 2. name
            if (fieldIndex < fields.size()) entry.name = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 3. fileName
            if (fieldIndex < fields.size()) entry.fileName = util::trim(fields[fieldIndex]);
            fieldIndex++;
            
            // 4. resetCount
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.resetCount = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 5. visitDay
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.visitDay = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 6. per
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.per = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 7. refillDays
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.refillDays = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 8. alertDays
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.alertDays = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 9. perm
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.perm = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 10. steal
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.steal = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 11. lock
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.lock = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 12. trap
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.trap = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 13. tres
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.tres = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 14. enc
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.enc = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 15. m1
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.m1 = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // 16. m2
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.m2 = std::stoi(util::cleanNumericString(util::trim(fields[fieldIndex])));
            fieldIndex++;

            // Monster 1 Encounter
            // 17. monster1.picture
            if (fieldIndex < fields.size()) entry.monster1.picture = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 19. monster1.name
            if (fieldIndex < fields.size()) entry.monster1.name = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 20. monster1.countRange
            if (fieldIndex < fields.size()) entry.monster1.countRange = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 21. monster1.id
            if (fieldIndex < fields.size()) entry.monster1.id = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Monster 2 Encounter
            // 22. monster2.picture
            if (fieldIndex < fields.size()) entry.monster2.picture = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 23. monster2.name
            if (fieldIndex < fields.size()) entry.monster2.name = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 24. monster2.countRange
            if (fieldIndex < fields.size()) entry.monster2.countRange = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 25. monster2.id
            if (fieldIndex < fields.size()) entry.monster2.id = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // Monster 3 Encounter
            // 26. monster3.picture
            if (fieldIndex < fields.size()) entry.monster3.picture = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 27. monster3.name
            if (fieldIndex < fields.size()) entry.monster3.name = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 28. monster3.countRange
            if (fieldIndex < fields.size()) entry.monster3.countRange = util::trim(fields[fieldIndex]);
            fieldIndex++;
            // 29. monster3.id
            if (fieldIndex < fields.size()) entry.monster3.id = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 30. track
            if (fieldIndex < fields.size()) entry.track = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 31. eaxEnvironments
            if (fieldIndex < fields.size()) entry.eaxEnvironments = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 32. mapDesigner
            if (fieldIndex < fields.size() && !fields[fieldIndex].empty()) entry.mapDesigner = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 33. notes
            if (fieldIndex < fields.size()) entry.notes = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 34. notesExtraField (empty field)
            if (fieldIndex < fields.size()) entry.notesExtraField = util::trim(fields[fieldIndex]);
            fieldIndex++;

            // 35. inArea (previous 34)
            if (fieldIndex < fields.size()) entry.inArea = util::trim(fields[fieldIndex]);
            fieldIndex++;

            mapStats.push_back(entry);
        }
        catch (const std::exception& e)
        {
            logger.error(std::format("Error parsing mapstats line '{}': {}", line, e.what()));
            continue;
        }
    }

    logger.info(std::format("Successfully parsed {} mapstat entries", mapStats.size()));
    return true;
}

} // namespace runeharbor::formats
