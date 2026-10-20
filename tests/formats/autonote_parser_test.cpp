// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/autonote_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp"

TEST_CASE("AutonoteParser parsing AUTONOTE.TXT", "[autonote_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::AutonoteParser parser(logger);

    // Sample data from test_extracted/AUTONOTE.TXT
    std::string sample_data = R"(Note bit	Autonote Text	Category
1	Accepted fireball wand from Mr. Malwick on Emerald Island.	Misc	1	
2	50 points of temporary Fire resistance from the central town well on Emerald Island.	Stat	1	Stat
19	"25 points of temporary Might, Intellect, Personality, Endurance, Speed, Accuracy, and Luck from the central fountain in Celeste."	Stat	7
)";

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& entries = parser.getAutonoteEntries();
        REQUIRE(entries.size() == 3); // 3 entries in sample_data (excluding header)
    }

    SECTION("First entry is correct (non-quoted text, extra fields ignored)")
    {
        parser.parse(data);
        const auto& entries = parser.getAutonoteEntries();
        REQUIRE(entries[0].noteBit == 1);
        REQUIRE(entries[0].autonoteText ==
                "Accepted fireball wand from Mr. Malwick on Emerald Island.");
        REQUIRE(entries[0].category == "Misc");
    }

    SECTION("Second entry is correct (non-quoted text, extra fields ignored)")
    {
        parser.parse(data);
        const auto& entries = parser.getAutonoteEntries();
        REQUIRE(entries[1].noteBit == 2);
        REQUIRE(
            entries[1].autonoteText ==
            "50 points of temporary Fire resistance from the central town well on Emerald Island.");
        REQUIRE(entries[1].category == "Stat");
    }

    SECTION("Third entry is correct (quoted text, extra fields ignored)")
    {
        parser.parse(data);
        const auto& entries = parser.getAutonoteEntries();
        REQUIRE(entries[2].noteBit == 19);
        REQUIRE(entries[2].autonoteText ==
                "25 points of temporary Might, Intellect, Personality, Endurance, Speed, Accuracy, "
                "and Luck from the central fountain in Celeste.");
        REQUIRE(entries[2].category == "Stat");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getAutonoteEntries().empty());
    }

    SECTION("Malformed header (incorrect text) returns false")
    {
        std::string malformed_header_data = R"(Note bit_WRONG	Autonote Text	Category
1	"Some text"	Misc
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(),
                                           malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getAutonoteEntries().empty());
    }

    SECTION("Text-only line is accepted with an empty category")
    {
        std::string short_line_data = R"(Note bit	Autonote Text	Category
1	"Some text"
MALFORMED
2	"Another text"	Stat
)";
        std::vector<uint8_t> short_vec(short_line_data.begin(), short_line_data.end());
        REQUIRE(parser.parse(short_vec));
        const auto& entries = parser.getAutonoteEntries();
        REQUIRE(entries.size() == 2); // entry 1 (no category) and entry 2
        REQUIRE(entries[0].noteBit == 1);
        REQUIRE(entries[0].autonoteText == "Some text");
        REQUIRE(entries[0].category.empty());
        REQUIRE(entries[1].noteBit == 2);
    }

    SECTION("Reserved note-bit row with no text is preserved")
    {
        std::string reserved_data = R"(Note bit	Autonote Text	Category
1	"Some text"	Misc
208
)";
        std::vector<uint8_t> reserved_vec(reserved_data.begin(), reserved_data.end());
        REQUIRE(parser.parse(reserved_vec));
        const auto& entries = parser.getAutonoteEntries();
        REQUIRE(entries.size() == 2);
        REQUIRE(entries[1].noteBit == 208);
        REQUIRE(entries[1].autonoteText.empty());
        REQUIRE(entries[1].category.empty());
    }
}
