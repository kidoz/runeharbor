// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <set> // For unique entity names

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

// Represents the hostility matrix
class HostileMatrix
{
public:
    // Returns the hostility value from 'source' to 'target'
    std::optional<int> getHostility(const std::string& source, const std::string& target) const;

    // Adds a hostility value
    void addHostility(const std::string& source, const std::string& target, int value);

    // Sets the column labels for the matrix.
    void setColLabels(const std::vector<std::string>& labels);

    // Returns all unique entity names (both source and target)
    std::vector<std::string> getEntityNames() const;

    // Get the column labels
    const std::vector<std::string>& getColLabels() const { return colLabels; }

private:
    std::map<std::string, std::map<std::string, int>> matrix;
    std::vector<std::string> rowLabels; // To maintain order and unique names
    std::vector<std::string> colLabels; // To maintain order and unique names
};

class HostileParser
{
public:
    explicit HostileParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const HostileMatrix& getHostileMatrix() const { return hostileMatrix; }

private:
    util::ILogger& logger;
    HostileMatrix hostileMatrix;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
