// SPDX-License-Identifier: MIT
#include "hostile_parser.hpp"

#include <algorithm>
#include <format>
#include <set>     // For std::set in getEntityNames
#include <sstream> // For std::istringstream and std::getline

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

// HostileMatrix methods
std::optional<int> HostileMatrix::getHostility(const std::string& source,
                                               const std::string& target) const
{
    auto itRow = matrix.find(source);
    if (itRow != matrix.end())
    {
        auto itCol = itRow->second.find(target);
        if (itCol != itRow->second.end())
        {
            return itCol->second;
        }
    }
    return std::nullopt;
}

std::optional<int> HostileMatrix::getHostilityInsensitive(const std::string& source,
                                                          const std::string& target) const
{
    if (auto exact = getHostility(source, target); exact.has_value())
    {
        return exact;
    }

    const std::string sourceNeedle = util::toLower(util::trim(source));
    const std::string targetNeedle = util::toLower(util::trim(target));

    for (const auto& [rowName, cols] : matrix)
    {
        if (util::toLower(util::trim(rowName)) != sourceNeedle)
        {
            continue;
        }

        for (const auto& [colName, value] : cols)
        {
            if (util::toLower(util::trim(colName)) == targetNeedle)
            {
                return value;
            }
        }
    }

    return std::nullopt;
}

void HostileMatrix::addHostility(const std::string& source, const std::string& target, int value)
{
    // Add source and target to labels if not already present, maintaining order
    // This is optional depending on whether order matters outside of parsing.
    // For a simple map, this is not strictly needed for functionality but can be useful
    // for introspection or reconstructing the matrix in order.
    if (std::find(rowLabels.begin(), rowLabels.end(), source) == rowLabels.end())
    {
        rowLabels.push_back(source);
    }
    // Target is already handled by colLabels which is set once from the header.

    matrix[source][target] = value;
}

void HostileMatrix::setColLabels(const std::vector<std::string>& labels)
{
    colLabels = labels;
}

std::vector<std::string> HostileMatrix::getEntityNames() const
{
    std::set<std::string> uniqueNames;
    for (const auto& label : rowLabels)
    {
        uniqueNames.insert(label);
    }
    for (const auto& label : colLabels)
    {
        uniqueNames.insert(label);
    }
    return std::vector<std::string>(uniqueNames.begin(), uniqueNames.end());
}

// HostileParser methods
HostileParser::HostileParser(util::ILogger& logger) : logger(logger) {}

bool HostileParser::parse(const std::vector<uint8_t>& data)
{
    // Clear any previous data
    hostileMatrix = HostileMatrix();

    if (data.empty())
    {
        logger.error("Hostile data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(content);
    std::string line;

    // Read the first line for column headers
    if (!std::getline(iss, line))
    {
        logger.error("Failed to read column headers line.");
        return false;
    }
    std::vector<std::string> colFields = util::splitString(line, '	');
    if (colFields.empty())
    {
        logger.error("No column headers found.");
        return false;
    }

    std::vector<std::string> parsedColLabels;
    // The first field is empty, so start from the second field.
    for (size_t i = 1; i < colFields.size(); ++i)
    {
        std::string trimmed_field = util::trim(colFields[i]);
        if (!trimmed_field.empty())
        { // Ensure label is not empty
            parsedColLabels.push_back(trimmed_field);
        }
    }

    if (parsedColLabels.empty())
    {
        logger.error("No valid column labels extracted from header.");
        return false;
    }
    hostileMatrix.setColLabels(parsedColLabels);
    logger.debug(std::format("Parsed {} column labels.", parsedColLabels.size()));

    // Process data lines
    while (std::getline(iss, line))
    {
        if (line.empty() || util::trim(line).empty())
        {
            // Skip empty or whitespace-only lines
            continue;
        }

        std::vector<std::string> rowFields = util::splitString(line, '	');
        if (rowFields.empty())
        {
            logger.warning(std::format("Skipping empty row data line: '{}'", line));
            continue;
        }

        std::string rowLabel = util::trim(rowFields[0]);
        if (rowLabel.empty())
        {
            logger.warning(std::format("Skipping row with empty label: '{}'", line));
            continue;
        }

        // Data starts from the second field in the row, matching column labels
        if (rowFields.size() - 1 != parsedColLabels.size())
        { // -1 because row label is first field
            logger.warning(std::format("Row '{}' has {} data fields, but expected {}. Skipping.",
                                       rowLabel, rowFields.size() - 1, parsedColLabels.size()));
            continue;
        }

        for (size_t i = 1; i < rowFields.size(); ++i) // Start from 1 to skip row label
        {
            std::string targetLabel =
                parsedColLabels[i - 1]; // i-1 because parsedColLabels is 0-indexed
            try
            {
                int hostilityValue = std::stoi(util::trim(rowFields[i]));
                hostileMatrix.addHostility(rowLabel, targetLabel, hostilityValue);
            }
            catch (const std::exception& e)
            {
                logger.error(std::format("Error parsing hostility value for {}-{} in line '{}': {}",
                                         rowLabel, targetLabel, line, e.what()));
                // Continue parsing other values in the same row, or return false, depending on
                // desired error handling For now, continue to allow partial parsing if some values
                // are malformed.
            }
        }
    }

    logger.info(std::format("Successfully parsed hostility matrix with {} rows.",
                            hostileMatrix.getEntityNames().size()));
    return true;
}

} // namespace runeharbor::formats
