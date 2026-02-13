// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/quests_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" 

TEST_CASE("QuestsParser parsing QUESTS.TXT", "[quests_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::QuestsParser parser(logger);

    // Sample data from test_extracted/QUESTS.TXT
    // Note: Some lines in the actual file have more than 4 fields.
    // The parser should handle this by only parsing the first 4 fields.
    std::string sample_data = R"(Q Bit	Quest Note Text	Notes	Owner	
1	Return a red potion to the Judge on Emerald Island.	0	Bryan	
2	Return a seashell to the Judge on Emerald Island.	0	Bryan	
3	Return a longbow to the Judge on Emerald Island.	0	Bryan	
7		Finished Scavenger Hunt	Bryan	
18	"Go to Lord Markham's estate in Tatalia, steal the vase there, and return it to William Lasker in the Erathian Sewers."	0	Bryan	0
)";
    
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& quests = parser.getQuests();
        REQUIRE(quests.size() == 5); // 5 quest entries in sample_data (excluding header)
    }

    SECTION("First quest entry is correct (4 fields)")
    {
        parser.parse(data);
        const auto& quests = parser.getQuests();
        REQUIRE(quests[0].qBit == 1);
        REQUIRE(quests[0].questNoteText == "Return a red potion to the Judge on Emerald Island.");
        REQUIRE(quests[0].notes == "0");
        REQUIRE(quests[0].owner == "Bryan");
    }

    SECTION("Quest entry with empty Quest Note Text is correct (4 fields)")
    {
        parser.parse(data);
        const auto& quests = parser.getQuests();
        REQUIRE(quests[3].qBit == 7);
        REQUIRE(quests[3].questNoteText == ""); // It should be empty string, not "Finished Scavenger Hunt"
        REQUIRE(quests[3].notes == "Finished Scavenger Hunt");
        REQUIRE(quests[3].owner == "Bryan");
    }

    SECTION("Quest entry with quoted text and extra field is correct (5 fields in raw, parse 4)")
    {
        parser.parse(data);
        const auto& quests = parser.getQuests();
        REQUIRE(quests[4].qBit == 18);
        REQUIRE(quests[4].questNoteText == "Go to Lord Markham's estate in Tatalia, steal the vase there, and return it to William Lasker in the Erathian Sewers.");
        REQUIRE(quests[4].notes == "0");
        REQUIRE(quests[4].owner == "Bryan");
        // The extra '0' from the raw data line for quest 18 is ignored as we only parse first 4 fields.
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getQuests().empty());
    }

    SECTION("Malformed header (incorrect text) returns false")
    {
        std::string malformed_header_data = R"(Q Bit	Quest Note Text	Notes	Owner_WRONG
1	Return a red potion to the Judge on Emerald Island.	0	Bryan
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getQuests().empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(Q Bit	Quest Note Text	Notes	Owner
1	Return a red potion to the Judge on Emerald Island.	0	Bryan
MALFORMED
2	Return a seashell to the Judge on Emerald Island.	0	Bryan
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getQuests().size() == 2); // 2 quests parsed (malformed line skipped)
    }
}
