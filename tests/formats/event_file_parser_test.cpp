// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/event_file_parser.hpp"
#include "../../src/util/console_logger.hpp"

TEST_CASE("EventFileParser parsing 2DEvents.txt", "[event_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::EventFileParser parser(logger);

    // Sample data from test_extracted/2DEvents.txt
    // Using a minimal, representative snippet for unit testing
    std::string sample_data = R"(2D Events by Type ( NPC's present are from NPC database)
#	#	Type	Map	Picture	Name	Proprieter Name	Title	F9	F10	F11	F12	F13	F14	F15	F16	F17	F18	F19	F20	F21	F22	F23	F24	F25	F26	F27	F28	F29	F30
1	1	Weapon Shop	1	57	The Knight's Blade	Tor	Blacksmith	0	0	0	0	1.5	1		7	Notes1	Notes2	6	18	0	0	Restrictions	Text	0	0	0	0	0	0
2	2	Weapon Shop	2	57	Tempered Steel	Caldar	Blacksmith	0	0	0	0	1.5	1		14	Notes1	Notes2	6	18	0	0	Restrictions	Text	0	0	0	0	0	0
497	11	Alchemist	0	7	Aromatic Therapies	Kathleen	Alchemist	0	0	0	0	1.5	1		7	Notes1	Notes2	6	18	0	0	Restrictions	Text	0	0	0	0	0	0
522	0	Alice Hargreaves	48	81	Alice Hargreaves	Proprietor	Title	0	0	0	0	1.5	1		7	Notes1	Notes2	6	18	0	0	Restrictions	Text	0	0	0	0	0	0
)";
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& events = parser.getEvents();
        REQUIRE(events.size() == 4); // 4 data lines in sample_data
    }

    SECTION("First event entry is correct")
    {
        parser.parse(data);
        const auto& events = parser.getEvents();
        REQUIRE(events.size() > 0);
        REQUIRE(events[0].mainId == 1);
        REQUIRE(events[0].subId == 1);
        REQUIRE(events[0].type == "Weapon Shop");
        REQUIRE(events[0].mapId == 1);
        REQUIRE(events[0].pictureId == 57);
        REQUIRE(events[0].name == "The Knight's Blade");
        REQUIRE(events[0].proprietorName == "Tor");
        REQUIRE(events[0].title == "Blacksmith");
        REQUIRE(events[0].field9_int == 0);
        REQUIRE(events[0].field10_int == 0);
        REQUIRE(events[0].field11_int == 0);
        REQUIRE(events[0].field12_int == 0);
        REQUIRE(events[0].field13_float == Catch::Approx(1.5f));
        REQUIRE(events[0].field14_int == 1);
        REQUIRE(events[0].field15_str == "");
        REQUIRE(events[0].field16_int == 7);
        REQUIRE(events[0].field17_str == "Notes1");
        REQUIRE(events[0].field18_str == "Notes2");
        REQUIRE(events[0].field19_int == 6);
        REQUIRE(events[0].field20_int == 18);
        REQUIRE(events[0].field21_int == 0);
        REQUIRE(events[0].field22_int == 0);
        REQUIRE(events[0].field23_str == "Restrictions");
        REQUIRE(events[0].field24_str == "Text");
        REQUIRE(events[0].field25_int == 0);
        REQUIRE(events[0].field26_int == 0);
        REQUIRE(events[0].field27_int == 0);
        REQUIRE(events[0].field28_int == 0);
        REQUIRE(events[0].field29_int == 0);
        REQUIRE(events[0].field30_int == 0);
    }

    SECTION("Last event entry has correct data")
    {
        parser.parse(data);
        const auto& events = parser.getEvents();
        REQUIRE(events.size() > 3); // Ensure at least 4 entries
        // Check the modified Alice Hargreaves entry
        REQUIRE(events[3].mainId == 522);
        REQUIRE(events[3].subId == 0);
        REQUIRE(events[3].type == "Alice Hargreaves");
        REQUIRE(events[3].mapId == 48);
        REQUIRE(events[3].pictureId == 81);
        REQUIRE(events[3].name == "Alice Hargreaves");
        REQUIRE(events[3].proprietorName == "Proprietor");
        REQUIRE(events[3].title == "Title");
        REQUIRE(events[3].field9_int == 0);
        REQUIRE(events[3].field10_int == 0);
        REQUIRE(events[3].field11_int == 0);
        REQUIRE(events[3].field12_int == 0);
        REQUIRE(events[3].field13_float == Catch::Approx(1.5f));
        REQUIRE(events[3].field14_int == 1);
        REQUIRE(events[3].field15_str == "");
        REQUIRE(events[3].field16_int == 7);
        REQUIRE(events[3].field17_str == "Notes1");
        REQUIRE(events[3].field18_str == "Notes2");
        REQUIRE(events[3].field19_int == 6);
        REQUIRE(events[3].field20_int == 18);
        REQUIRE(events[3].field21_int == 0);
        REQUIRE(events[3].field22_int == 0);
        REQUIRE(events[3].field23_str == "Restrictions");
        REQUIRE(events[3].field24_str == "Text");
        REQUIRE(events[3].field25_int == 0);
        REQUIRE(events[3].field26_int == 0);
        REQUIRE(events[3].field27_int == 0);
        REQUIRE(events[3].field28_int == 0);
        REQUIRE(events[3].field29_int == 0);
        REQUIRE(events[3].field30_int == 0);
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getEvents().empty());
    }

    SECTION("Malformed data (less than 2 header lines) returns false")
    {
        std::string malformed_data = "Single header line";
        std::vector<uint8_t> malformed_vec(malformed_data.begin(), malformed_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec)); // Will fail on second getline
        REQUIRE(parser.getEvents().empty());
    }
}