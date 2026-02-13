#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "npctext_parser.hpp"

#include <algorithm>
#include <format>
#include <sstream> // For std::istringstream and std::getline


namespace runeharbor::formats
{

NPCTextParser::NPCTextParser(util::ILogger& logger) : logger(logger) {}

bool NPCTextParser::parse(const std::vector<uint8_t>& data)
{
    entries.clear(); // Clear any previous data

    if (data.empty())
    {
        logger.error("NPCText data is empty");
        return false;
    }

    // Convert byte vector to string
    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    size_t current_pos = 0;

    // Skip header line
    size_t header_end_pos = content.find('\n', current_pos);
    if (header_end_pos == std::string::npos)
    {
        logger.error("Failed to find header line.");
        return false;
    }
    std::string header_line = content.substr(current_pos, header_end_pos - current_pos);
    current_pos = header_end_pos + 1; // Move past the newline

    const std::string expected_header = "#\tText\tNotes\tOwner";
    if (util::trim(header_line) != expected_header)
    {
        logger.error(std::format("Malformed header: Expected '{}', got '{}'", expected_header, util::trim(header_line)));
        return false;
    }
    logger.debug(std::format("Skipping header: {}", header_line));

    // Process data lines
    while (current_pos < content.length())
    {
        // Find the start of the next significant line (skip empty lines)
        size_t line_start = current_pos;
        while (line_start < content.length() && (content[line_start] == '\n' || content[line_start] == '\r')) {
            line_start++;
        }
        if (line_start >= content.length()) {
            break; // No more content
        }

        current_pos = line_start;
        NPCTextEntry entry; 

        // Parse ID (first field, before first tab)
        size_t id_end = content.find('\t', current_pos);
        if (id_end == std::string::npos) {
            logger.warning(std::format("Skipping malformed NPCText entry (missing tab after ID): '{}'", content.substr(current_pos, std::min(content.length() - current_pos, (size_t)50))));
            // Skip the rest of this line and try next record
            size_t next_newline = content.find('\n', current_pos);
            if (next_newline == std::string::npos) {
                current_pos = content.length();
            } else {
                current_pos = next_newline + 1; // Move past newline to start of next potential record
            }
            continue; // Go to next record
        }
        std::string id_str = content.substr(current_pos, id_end - current_pos);
        try {
            entry.id = std::stoi(util::trim(id_str));
        } catch (const std::exception& e) {
            logger.error(std::format("Error parsing NPCText ID '{}': {}", id_str, e.what()));
            // Try to recover by skipping to next line, find next record.
            current_pos = content.find('\n', current_pos);
            if (current_pos == std::string::npos) current_pos = content.length();
            continue;
        }
        current_pos = id_end + 1; // Move past tab

        // Parse Text (second field, potentially quoted and multi-line)
        if (current_pos < content.length() && content[current_pos] == '"')
        {
            // Quoted field, find closing quote
            size_t quote_start = current_pos;
            current_pos++; // Move past opening quote

            size_t quote_end = std::string::npos;
            while (current_pos < content.length()) {
                if (content[current_pos] == '"') {
                    if (current_pos + 1 < content.length() && content[current_pos + 1] == '"') {
                        // Escaped double quote ""
                        current_pos += 2;
                    } else {
                        // Closing quote
                        quote_end = current_pos;
                        break;
                    }
                } else {
                    current_pos++;
                }
            }

            if (quote_end == std::string::npos) {
                logger.error(std::format("Malformed NPCText entry: Missing closing quote for text starting at '{}'", content.substr(quote_start, std::min(content.length() - quote_start, (size_t)50))));
                current_pos = content.length(); // End parsing
                break;
            }

            std::string quoted_text = content.substr(quote_start + 1, quote_end - (quote_start + 1));
            // Handle escaped quotes within the text ("" -> ")
            size_t esc_pos = 0;
            while ((esc_pos = quoted_text.find("\"\"", esc_pos)) != std::string::npos) {
                quoted_text.replace(esc_pos, 2, "\"");
            }
            entry.text = quoted_text;
            current_pos = quote_end + 1; // Move past closing quote
        }
        else
        {
            // Unquoted text field (should not happen for NPCTEXT.TXT as per analysis, but for robustness)
            size_t text_end = content.find('\t', current_pos);
            if (text_end == std::string::npos) text_end = content.find('\n', current_pos);
            if (text_end == std::string::npos) text_end = content.length();

            entry.text = util::trim(content.substr(current_pos, text_end - current_pos));
            current_pos = text_end;
        }

        // Move past potential tab after text field
        if (current_pos < content.length() && content[current_pos] == '\t') {
            current_pos++;
        }
        
        // Parse Notes (third field, optional)
        size_t notes_end = content.find('\t', current_pos);
        if (notes_end == std::string::npos) notes_end = content.find('\n', current_pos);
        if (notes_end == std::string::npos) notes_end = content.length();

        entry.notes = util::trim(content.substr(current_pos, notes_end - current_pos));
        current_pos = notes_end;

        // Move past potential tab after notes field
        if (current_pos < content.length() && current_pos != notes_end && content[current_pos] == '\t') {
            current_pos++;
        }

        // Parse Owner (fourth field, optional)
        size_t owner_end = content.find('\n', current_pos);
        if (owner_end == std::string::npos) owner_end = content.length();

        entry.owner = util::trim(content.substr(current_pos, owner_end - current_pos));
        current_pos = owner_end;

        // Move past the newline characters at the end of the record
        while (current_pos < content.length() && (content[current_pos] == '\n' || content[current_pos] == '\r')) {
            current_pos++;
        }

        entries.push_back(entry);
    }

    logger.info(std::format("Successfully parsed {} NPCText entries", entries.size()));
    return true;
}

} // namespace runeharbor::formats
