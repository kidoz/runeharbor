// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/classes_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" 

TEST_CASE("ClassesParser parsing CLASS.TXT", "[classes_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::ClassesParser parser(logger);

    // Sample data from test_extracted/CLASS.TXT
    std::string sample_data = R"(Class	Descriptions	Notes
Knight	"The Knight class is the workhorse fighting class.  Knights start with the largest selection of weapons and armor, and may ultimately use any type of weapon or armor, Being the toughest warriors of the classes, Knights begin with the greatest number of hit points and get the most number of hit points when they advance in level.  Knights may never learn spells, nor may they ever learn (or need) the Meditation skill."	Knight
Cavalier	"The Cavalier class is the first promotion of the Knight.  Cavaliers may use any type of weapon or armor, but they may not learn spells.  Cavaliers enjoy the benefit of an extra two hit points per level, and can be promoted once more to either Champion or Black Knight status with another two hit point per level gain.  "	Knight
Thief	"The Thief class concentrates less on fighting and more on utility skills, such as disarming traps and stealing.  Although they aren't as good with weapons and armor as Knights, Thieves are still better fighters than the semi and full spell using classes.  Thieves may eventually use spells from the Elemental schools, but they will never be very good at them."	Thief
)";
    
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& classes = parser.getClasses();
        REQUIRE(classes.size() == 3); // 3 class entries in sample_data (excluding header)
    }

    SECTION("Knight entry is correct")
    {
        parser.parse(data);
        const auto& classes = parser.getClasses();
        REQUIRE(classes[0].className == "Knight");
        REQUIRE(classes[0].description == "The Knight class is the workhorse fighting class.  Knights start with the largest selection of weapons and armor, and may ultimately use any type of weapon or armor, Being the toughest warriors of the classes, Knights begin with the greatest number of hit points and get the most number of hit points when they advance in level.  Knights may never learn spells, nor may they ever learn (or need) the Meditation skill.");
        REQUIRE(classes[0].notes == "Knight");
    }

    SECTION("Cavalier entry is correct")
    {
        parser.parse(data);
        const auto& classes = parser.getClasses();
        REQUIRE(classes[1].className == "Cavalier");
        REQUIRE(classes[1].description == runeharbor::util::trim("The Cavalier class is the first promotion of the Knight.  Cavaliers may use any type of weapon or armor, but they may not learn spells.  Cavaliers enjoy the benefit of an extra two hit points per level, and can be promoted once more to either Champion or Black Knight status with another two hit point per level gain.  "));
        REQUIRE(classes[1].notes == "Knight");
    }

    SECTION("Thief entry is correct")
    {
        parser.parse(data);
        const auto& classes = parser.getClasses();
        REQUIRE(classes[2].className == "Thief");
        REQUIRE(classes[2].description == "The Thief class concentrates less on fighting and more on utility skills, such as disarming traps and stealing.  Although they aren't as good with weapons and armor as Knights, Thieves are still better fighters than the semi and full spell using classes.  Thieves may eventually use spells from the Elemental schools, but they will never be very good at them.");
        REQUIRE(classes[2].notes == "Thief");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getClasses().empty());
    }

    SECTION("Malformed header (incorrect text) returns false")
    {
        std::string malformed_header_data = R"(Class	Descriptions_WRONG	Notes
Knight	"Description"	Knight
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getClasses().empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(Class	Descriptions	Notes
Knight	"Description"	Knight
MALFORMED
Thief	"Description"	Thief
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getClasses().size() == 2); // 2 classes parsed (malformed line skipped)
    }
}
