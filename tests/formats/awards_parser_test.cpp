// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/awards_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp"

TEST_CASE("AwardsParser parsing AWARDS.TXT", "[awards_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::AwardsParser parser(logger);

    // Sample data from test_extracted/AWARDS.TXT
    std::string sample_data = R"(A Bit	Awards	Sort	Notes
1	Current Fines Due: %lu	1	
2	Won the Scavenger Hunt on Emerald Island	2	
3	Found the missing contestants on Emerald Island	3	
47	Retrieved Both Temple Pieces	2	Used on two sides
80	Retrieved Soul Jars	2	Used on two sides
103	ArcoMage wins: %lu	1
)";

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& awards = parser.getAwards();
        REQUIRE(awards.size() == 6); // 6 award entries in sample_data (excluding header)
    }

    SECTION("First award entry is correct (empty Notes)")
    {
        parser.parse(data);
        const auto& awards = parser.getAwards();
        REQUIRE(awards[0].aBit == 1);
        REQUIRE(awards[0].awardText == "Current Fines Due: %lu");
        REQUIRE(awards[0].sortOrder == 1);
        REQUIRE(awards[0].notes == "");
    }

    SECTION("Award entry with Notes field is correct")
    {
        parser.parse(data);
        const auto& awards = parser.getAwards();
        REQUIRE(awards[3].aBit == 47);
        REQUIRE(awards[3].awardText == "Retrieved Both Temple Pieces");
        REQUIRE(awards[3].sortOrder == 2);
        REQUIRE(awards[3].notes == "Used on two sides");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getAwards().empty());
    }

    SECTION("Malformed header (incorrect text) returns false")
    {
        std::string malformed_header_data = R"(A Bit	Awards_WRONG	Sort	Notes
1	Award Text	1	Note
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(),
                                           malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getAwards().empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(A Bit	Awards	Sort	Notes
1	Award Text	1	Note
MALFORMED
2	Another Award	2	Another Note
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getAwards().size() == 2); // 2 awards parsed (malformed line skipped)
    }
}
