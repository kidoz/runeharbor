// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/global_text_parser.hpp"
#include "../../src/util/console_logger.hpp"

TEST_CASE("GlobalTextParser parsing Global.txt", "[global_text_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::GlobalTextParser parser(logger);

    // Sample data from test_extracted/Global.txt
    // Using a minimal, representative snippet for unit testing
    std::string sample_data = R"(Global Text

0	AC
1	Accuracy
2	Black Knight
3	Spy
15	Assertion failed at %d in %s
223

)"; // The last line is empty and should be skipped.

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& entries = parser.getTextEntries();
        REQUIRE(entries.size() == 5); // 5 data entries in sample_data
    }

    SECTION("Specific text entries are correct")
    {
        parser.parse(data);
        const auto& entries = parser.getTextEntries();

        REQUIRE(entries.at(0) == "AC");
        REQUIRE(entries.at(1) == "Accuracy");
        REQUIRE(entries.at(2) == "Black Knight");
        REQUIRE(entries.at(3) == "Spy");
        REQUIRE(entries.at(15) == "Assertion failed at %d in %s");
    }

    SECTION("getText retrieves correct text")
    {
        parser.parse(data);
        REQUIRE(parser.getText(0) == "AC");
        REQUIRE(parser.getText(3) == "Spy");
        REQUIRE(parser.getText(100) == std::nullopt); // Non-existent ID
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getTextEntries().empty());
    }

    SECTION("Malformed header returns true and skips header line if not 'Global Text'")
    {
        std::string malformed_data = R"(NOT Global Text
0	Test1
1	Test2)"; // Use raw string literal
        std::vector<uint8_t> malformed_vec(malformed_data.begin(), malformed_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getTextEntries().size() == 2); // "0	Test1" and "1	Test2"
        REQUIRE(parser.getText(0) == "Test1");
        REQUIRE(parser.getText(1) == "Test2");
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_data_line = R"(Global Text
0	Test1
MalformedLine
1	Test2)"; // Use raw string literal
        std::vector<uint8_t> malformed_vec(malformed_data_line.begin(), malformed_data_line.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getTextEntries().size() == 2); // "0	Test1" and "1	Test2"
        REQUIRE(parser.getText(0) == "Test1");
        REQUIRE(parser.getText(1) == "Test2");
    }

    SECTION("Malformed data line (non-integer ID) is skipped")
    {
        std::string malformed_id_line = R"(Global Text
ABC	Test1
1	Test2)"; // Use raw string literal
        std::vector<uint8_t> malformed_vec(malformed_id_line.begin(), malformed_id_line.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getTextEntries().size() == 1); // Only "1	Test2" should parse
        REQUIRE(parser.getText(1) == "Test2");
    }
}