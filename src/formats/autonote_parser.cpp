// SPDX-License-Identifier: MIT
#include "autonote_parser.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <sstream> // For std::istringstream and std::getline
#include <string_view>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

AutonoteParser::AutonoteParser(util::ILogger& logger) : logger(logger) {}

bool AutonoteParser::parse(const std::vector<uint8_t>& data)
{
    entries.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("Autonote data is empty");
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
    const std::string expected_header = "Note bit	Autonote Text	Category";
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

        AutonoteEntry entry;
        std::vector<std::string> fields =
            util::splitString(line, '	', '"'); // Split by tab, handle quotes

        if (fields.empty())
        {
            continue;
        }

        // The shipped table ends with reserved note-bit rows that contain only
        // the bit number (e.g. "208") and no text/category. Preserve those as
        // empty notes instead of dropping them.
        const std::string bitText = util::trim(fields[0]);
        int noteBit = 0;
        const auto [ptr, ec] =
            std::from_chars(bitText.data(), bitText.data() + bitText.size(), noteBit);
        if (ec != std::errc{} || ptr != bitText.data() + bitText.size())
        {
            logger.warning(
                std::format("Skipping malformed Autonote line (bad note bit): {}", line));
            continue;
        }

        entry.noteBit = noteBit;
        if (fields.size() > 1)
        {
            entry.autonoteText = util::trim(fields[1]);
        }
        if (fields.size() > 2)
        {
            entry.category = util::trim(fields[2]);
        }
        entries.push_back(entry);
    }

    logger.info(std::format("Successfully parsed {} Autonote entries", entries.size()));
    return true;
}

} // namespace runeharbor::formats
