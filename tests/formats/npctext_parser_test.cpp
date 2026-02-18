// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/npctext_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp"

TEST_CASE("NPCTextParser parsing NPCTEXT.TXT", "[npctext_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::NPCTextParser parser(logger);

    // Sample data from test_extracted/NPCTEXT.TXT
    std::string sample_data = R"(#	Text	Notes	Owner
1	"Emerald Island is southeast of Erathia, and far away from pirates and other ruffians.  It would be ideal, if not for the constant swarms of dragonflies that infest the island.  Fortunately, the large number of armed and skilled people this contest has attracted should thin their numbers a bit.

Lord Markham has supplemented the normally sparse village with a weapon, armor, and alchemist shop for your convenience during the contest.  A temple and basic magical guilds are also available.  I would suggest making sure that you are completely ready before you start off on the contest."		
3	"I respect your decision.  I am, however, a patient man, and will offer you this chance until a winner in the contest is proclaimed."	Malwick no	
5	"Isn't this hunt exciting?  I really am grateful you came to my little event, and I hope you have fun, even if you don't win.  I think it's great that everyone is competing in a spirit of good sportsmanship and camaraderie."	speech stays until contest is won	
)";

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& entries = parser.getNPCTextEntries();
        REQUIRE(entries.size() == 3); // 3 entries in sample_data (excluding header)
    }

    SECTION("First entry is correct (empty Notes and Owner)")
    {
        parser.parse(data);
        const auto& entries = parser.getNPCTextEntries();
        REQUIRE(entries[0].id == 1);
        REQUIRE(
            entries[0].text ==
            R"###(Emerald Island is southeast of Erathia, and far away from pirates and other ruffians.  It would be ideal, if not for the constant swarms of dragonflies that infest the island.  Fortunately, the large number of armed and skilled people this contest has attracted should thin their numbers a bit.

Lord Markham has supplemented the normally sparse village with a weapon, armor, and alchemist shop for your convenience during the contest.  A temple and basic magical guilds are also available.  I would suggest making sure that you are completely ready before you start off on the contest.)###");
        REQUIRE(entries[0].notes == "");
        REQUIRE(entries[0].owner == "");
    }

    SECTION("Second entry is correct (with Notes, empty Owner)")
    {
        parser.parse(data);
        const auto& entries = parser.getNPCTextEntries();
        REQUIRE(entries[1].id == 3);
        REQUIRE(entries[1].text ==
                "I respect your decision.  I am, however, a patient man, and will offer you this "
                "chance until a winner in the contest is proclaimed.");
        REQUIRE(entries[1].notes == "Malwick no");
        REQUIRE(entries[1].owner == "");
    }

    SECTION("Third entry is correct (with Notes and Owner)")
    {
        // Re-using data from the provided sample which has a Notes field but empty Owner
        // To properly test with an owner, I'll temporarily append an entry with an owner
        // to the sample_data for this specific test section.
        std::string extended_sample_data =
            sample_data +
            R"(6	"If you win, you'll be in charge of one of the most scenic areas in all Erathia!  Harmondale is just outside of the Tularean Forest, right on the edge of the Elf-Human border.  And I'm sure you'll love the castle.  It's a bit of a fixer-upper, but it's quite roomy and has excellent ventilation.  It breaks my heart to part with this property, but I feel that the time has come for me to give something back to the people."	speech stays until contest is won	Markham
)";
        std::vector<uint8_t> extended_data(extended_sample_data.begin(),
                                           extended_sample_data.end());

        parser.parse(extended_data);
        const auto& entries = parser.getNPCTextEntries();
        REQUIRE(entries.size() == 4); // 3 original + 1 appended
        REQUIRE(entries[3].id == 6);
        REQUIRE(entries[3].text ==
                "If you win, you'll be in charge of one of the most scenic areas in all Erathia!  "
                "Harmondale is just outside of the Tularean Forest, right on the edge of the "
                "Elf-Human border.  And I'm sure you'll love the castle.  It's a bit of a "
                "fixer-upper, but it's quite roomy and has excellent ventilation.  It breaks my "
                "heart to part with this property, but I feel that the time has come for me to "
                "give something back to the people.");
        REQUIRE(entries[3].notes == "speech stays until contest is won");
        REQUIRE(entries[3].owner == "Markham");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getNPCTextEntries().empty());
    }

    SECTION("Malformed header (incorrect text) returns false")
    {
        std::string malformed_header_data = R"(#	Text_WRONG	Notes	Owner
1	"Some text"		
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(),
                                           malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getNPCTextEntries().empty());
    }

    /*
        SECTION("Malformed data line (too few fields) is skipped")
        {
            std::string malformed_line_data = R"(#	Text	Notes	Owner
    1	text1	Note1
    MALFORMED_LINE_HERE
    2	text2	Note2
    )";
            std::vector<uint8_t> malformed_vec(malformed_line_data.begin(),
    malformed_line_data.end()); REQUIRE(parser.parse(malformed_vec));
            REQUIRE(parser.getNPCTextEntries().size() == 2); // 2 entries parsed (malformed line
    skipped) REQUIRE(parser.getNPCTextEntries()[0].id == 1);
            REQUIRE(parser.getNPCTextEntries()[1].id == 2);
        }
    */
}
