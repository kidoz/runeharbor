// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/npcnamess_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" 

TEST_CASE("NPCNamesParser parsing NPCNAMES.TXT", "[npcnamess_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::NPCNamesParser parser(logger);

    // Sample data from test_extracted/NPCNAMES.TXT
    std::string sample_data = R"(Male	Female
Aaron	Alice
Abe	Allison
Adam	Amber
MaleOnly	
	FemaleOnly
)";
    
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& names = parser.getNPCNames();
        REQUIRE(names.maleNames.size() == 5); // Aaron, Abe, Adam, MaleOnly, ""
        REQUIRE(names.femaleNames.size() == 5); // Alice, Allison, Amber, "", FemaleOnly
        // Verify specific entries
        REQUIRE(names.maleNames[0] == "Aaron");
        REQUIRE(names.femaleNames[0] == "Alice");
        REQUIRE(names.maleNames[3] == "MaleOnly");
        REQUIRE(names.femaleNames[3] == "");
        REQUIRE(names.maleNames[4] == "");
        REQUIRE(names.femaleNames[4] == "FemaleOnly");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getNPCNames().maleNames.empty());
        REQUIRE(parser.getNPCNames().femaleNames.empty());
    }

    SECTION("Malformed header (incorrect text) returns false")
    {
        std::string malformed_header_data = R"(Male_WRONG	Female
Aaron	Alice
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getNPCNames().maleNames.empty());
        REQUIRE(parser.getNPCNames().femaleNames.empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(Male	Female
Aaron	Alice
MALFORMED_LINE
Abe	Allison
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        const auto& names = parser.getNPCNames();
        REQUIRE(names.maleNames.size() == 2); // Aaron, Abe
        REQUIRE(names.femaleNames.size() == 2); // Alice, Allison
        REQUIRE(names.maleNames[0] == "Aaron");
        REQUIRE(names.femaleNames[0] == "Alice");
        REQUIRE(names.maleNames[1] == "Abe");
        REQUIRE(names.femaleNames[1] == "Allison");
    }
}