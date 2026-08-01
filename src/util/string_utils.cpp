// SPDX-License-Identifier: MIT
#include "string_utils.hpp"

#include <algorithm>

#include <cctype> // For std::isspace

namespace runeharbor::util
{

// Trim from start (in place)
static inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

// Trim from end (in place)
static inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(),
            s.end());
}

// Trim from both ends (copying)
std::string trim(const std::string& s)
{
    std::string s_copy = s;
    ltrim(s_copy);
    rtrim(s_copy);
    return s_copy;
}

std::vector<std::string> splitString(const std::string& s, char delimiter, char quoteChar)
{
    std::vector<std::string> tokens;
    std::string currentToken;
    bool inQuote = false;

    for (size_t i = 0; i < s.length(); ++i)
    {
        if (quoteChar != '\0' && s[i] == quoteChar)
        {
            if (!inQuote)
            { // Found opening quote
                inQuote = true;
            }
            else
            { // Found closing quote or escaped quote
                if (i + 1 < s.length() && s[i + 1] == quoteChar)
                {
                    // Escaped quote: "" becomes "
                    currentToken += quoteChar;
                    i++; // Skip the second quote
                }
                else
                {
                    // Closing quote
                    inQuote = false;
                }
            }
        }
        else if (s[i] == delimiter && !inQuote)
        {
            tokens.push_back(currentToken);
            currentToken.clear();
        }
        else
        {
            currentToken += s[i];
        }
    }
    tokens.push_back(currentToken); // Add the last token

    return tokens;
}

// Function to clean numerical strings by removing spaces and commas.
std::string cleanNumericString(std::string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return std::isspace(c) || c == ','; }),
            s.end());
    return s;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
    return std::ranges::equal(a, b,
                              [](char c1, char c2)
                              {
                                  return std::tolower(static_cast<unsigned char>(c1)) ==
                                         std::tolower(static_cast<unsigned char>(c2));
                              });
}

std::string toLower(std::string_view s)
{
    std::string result(s);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::vector<std::string> wordWrap(const std::string& text, int maxWidthChars)
{
    std::vector<std::string> lines;
    if (text.empty() || maxWidthChars <= 0)
        return lines;

    auto pushLine = [&](std::string line)
    {
        // Hard-break any single word longer than the column so output never
        // overflows; the rest of the line is then split at maxWidthChars.
        while (static_cast<int>(line.size()) > maxWidthChars)
        {
            lines.push_back(line.substr(0, static_cast<size_t>(maxWidthChars)));
            line = line.substr(static_cast<size_t>(maxWidthChars));
        }
        if (!line.empty())
            lines.push_back(std::move(line));
    };

    std::string word;
    std::string line;
    for (char c : text)
    {
        if (c == ' ' || c == '\n')
        {
            // Would adding `word` (plus a separating space) overflow the line?
            if (!line.empty() && static_cast<int>(line.size() + word.size() + 1) > maxWidthChars)
            {
                pushLine(std::move(line));
                line.clear();
            }
            if (!line.empty())
                line += ' ';
            line += word;
            word.clear();
            if (c == '\n')
            {
                pushLine(std::move(line));
                line.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    // Flush the trailing word.
    if (!word.empty())
    {
        if (!line.empty() && static_cast<int>(line.size() + word.size() + 1) > maxWidthChars)
        {
            pushLine(std::move(line));
            line.clear();
        }
        if (!line.empty())
            line += ' ';
        line += word;
    }
    if (!line.empty())
        pushLine(std::move(line));
    return lines;
}

} // namespace runeharbor::util