// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

namespace runeharbor::util
{

// Function to split a string by a delimiter, optionally handling quoted fields.
std::vector<std::string> splitString(const std::string& s, char delimiter, char quoteChar = '\0');

// Function to trim whitespace from the beginning and end of a string.
std::string trim(const std::string& s);

// Function to clean numerical strings by removing spaces and commas.
std::string cleanNumericString(std::string s);

// Function to compare strings case-insensitively.
bool equalsIgnoreCase(std::string_view a, std::string_view b);

// Function to convert a string to lowercase.
std::string toLower(std::string_view s);

} // namespace runeharbor::util