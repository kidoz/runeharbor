// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/mapstats_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" 

TEST_CASE("MapStatsParser parsing MAPSTATS.TXT", "[mapstats_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::MapStatsParser parser(logger);

    // Sample data from test_extracted/MAPSTATS.TXT
    std::string sample_data = R"(	Map Stats			First					x5	D20's		Random Encounter																					
			Reset	Visit	Per	Refil	Alert	Steal	Lock	Trap	Tres	Enc	M1	M2	M3			Dif	Appear			Dif	Appear			Dif	Appear	Redbook					
#	Name	File name	#	Day	0-20	Days	Days	Perm	0-20	0-10	0-6	%	%	%	%	Mon1 Pic	Mon 1	 1-5	#	Mon2 Pic	Mon 2	 1-5	#	Mon3 Pic	Mon 3	 1-5	#	Track	EAX Environments	Map Designer	Notes	in area	
1	Emerald Island	Out01.Odm	0	0	0	672	7	0	0	1	0	10	100	0	0	Dragonfly	Dragonfly	1	 2-5	0	0	1	 1-3	0	0	1	 1-3	20	FOREST	0	"Training Island--Lush, tropical island"		x
2	Harmondale	Out02.Odm	0	0	2	672	7	1	2	1	0	10	100	0	0	Goblin	Goblin	3	 2-5	Swordsman	Swordsman	3	 1-3	0	0	1	 1-3	4	PLAINS	0	Home town map--hills and scattered trees.  Water ok		x
3	Erathia	Out03.odm	0	0	4	672	7	3	4	2	1	10	50	50	0	Fighter Leather	Fighter Leather	2	 1-3	Griffin	Griffin	1	 1-3	0	0	1	 1-3	17	PLAINS	0	"Human Capital--low hills, river (with fort built on top), near lake"
)";
    
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& mapStats = parser.getMapStats();
        REQUIRE(mapStats.size() == 3); // 3 map entries in sample_data (excluding three headers)
    }

    SECTION("Emerald Island entry is correct")
    {
        parser.parse(data);
        const auto& mapStats = parser.getMapStats();
        REQUIRE(mapStats[0].id == 1);
        REQUIRE(mapStats[0].name == "Emerald Island");
        REQUIRE(mapStats[0].fileName == "Out01.Odm");
        REQUIRE(mapStats[0].resetCount == 0);
        REQUIRE(mapStats[0].visitDay == 0);
        REQUIRE(mapStats[0].per == 0);
        REQUIRE(mapStats[0].refillDays == 672);
        REQUIRE(mapStats[0].alertDays == 7);
        REQUIRE(mapStats[0].perm == 0);
        REQUIRE(mapStats[0].steal == 0);
        REQUIRE(mapStats[0].lock == 1);
        REQUIRE(mapStats[0].trap == 0);
        REQUIRE(mapStats[0].tres == 10);
        REQUIRE(mapStats[0].enc == 100);
        REQUIRE(mapStats[0].m1 == 0);
        REQUIRE(mapStats[0].m2 == 0);

        REQUIRE(mapStats[0].monster1.picture == "Dragonfly");
        REQUIRE(mapStats[0].monster1.name == "Dragonfly");
        REQUIRE(mapStats[0].monster1.countRange == "1");
        REQUIRE(mapStats[0].monster1.id == "2-5");

        REQUIRE(mapStats[0].monster2.picture == "0");
        REQUIRE(mapStats[0].monster2.name == "0");
        REQUIRE(mapStats[0].monster2.countRange == "1");
        REQUIRE(mapStats[0].monster2.id == "1-3");

        REQUIRE(mapStats[0].monster3.picture == "0");
        REQUIRE(mapStats[0].monster3.name == "0");
        REQUIRE(mapStats[0].monster3.countRange == "1");
        REQUIRE(mapStats[0].monster3.id == "1-3");

        REQUIRE(mapStats[0].track == "20");
        REQUIRE(mapStats[0].eaxEnvironments == "FOREST");
        REQUIRE(mapStats[0].mapDesigner == "0");
        REQUIRE(mapStats[0].notes == "Training Island--Lush, tropical island");
        REQUIRE(mapStats[0].inArea == "x");
    }

    SECTION("Erathia entry is correct")
    {
        parser.parse(data);
        const auto& mapStats = parser.getMapStats();
        REQUIRE(mapStats[2].id == 3);
        REQUIRE(mapStats[2].name == "Erathia");
        REQUIRE(mapStats[2].fileName == "Out03.odm");
        REQUIRE(mapStats[2].resetCount == 0);
        REQUIRE(mapStats[2].visitDay == 0);
        REQUIRE(mapStats[2].per == 4);
        REQUIRE(mapStats[2].refillDays == 672);
        REQUIRE(mapStats[2].alertDays == 7);
        REQUIRE(mapStats[2].perm == 3);
        REQUIRE(mapStats[2].steal == 4);
        REQUIRE(mapStats[2].lock == 2);
        REQUIRE(mapStats[2].trap == 1);
        REQUIRE(mapStats[2].tres == 10);
        REQUIRE(mapStats[2].enc == 50);
        REQUIRE(mapStats[2].m1 == 50);
        REQUIRE(mapStats[2].m2 == 0);

        REQUIRE(mapStats[2].monster1.picture == "Fighter Leather");
        REQUIRE(mapStats[2].monster1.name == "Fighter Leather");
        REQUIRE(mapStats[2].monster1.countRange == "2");
        REQUIRE(mapStats[2].monster1.id == "1-3");

        REQUIRE(mapStats[2].monster2.picture == "Griffin");
        REQUIRE(mapStats[2].monster2.name == "Griffin");
        REQUIRE(mapStats[2].monster2.countRange == "1");
        REQUIRE(mapStats[2].monster2.id == "1-3");

        REQUIRE(mapStats[2].monster3.picture == "0");
        REQUIRE(mapStats[2].monster3.name == "0");
        REQUIRE(mapStats[2].monster3.countRange == "1");
        REQUIRE(mapStats[2].monster3.id == "1-3");

        REQUIRE(mapStats[2].track == "17");
        REQUIRE(mapStats[2].eaxEnvironments == "PLAINS");
        REQUIRE(mapStats[2].mapDesigner == "0");
        REQUIRE(mapStats[2].notes == "Human Capital--low hills, river (with fort built on top), near lake");
        REQUIRE(mapStats[2].inArea == ""); // Empty for this entry
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getMapStats().empty());
    }

    SECTION("Malformed header (missing first line) returns false")
    {
        std::string malformed_header_data = R"(			Reset	Visit	Per	Refil	Alert	Steal	Lock	Trap	Tres	Enc	M1	M2	M3			Dif	Appear			Dif	Appear			Dif	Appear	Redbook					
#	Name	File name	#	Day	0-20	Days	Days	Perm	0-20	0-10	0-6	%	%	%	%	Mon1 Pic	Mon 1	 1-5	#	Mon2 Pic	Mon 2	 1-5	#	Mon3 Pic	Mon 3	 1-5	#	Track	EAX Environments	Map Designer	Notes	in area	
1	Emerald Island	Out01.Odm	0	0	0	672	7	0	0	1	0	10	100	0	0	Dragonfly	Dragonfly	1	 2-5	0	0	1	 1-3	0	0	1	 1-3	20	FOREST	0	"Training Island--Lush, tropical island"		x
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getMapStats().empty());
    }

    SECTION("Malformed header (incorrect field names header) returns false")
    {
        std::string malformed_header_data = R"(	Map Stats			First					x5	D20's		Random Encounter																					
			Reset	Visit	Per	Refil	Alert	Steal	Lock	Trap	Tres	Enc	M1	M2	M3			Dif	Appear			Dif	Appear			Dif	Appear	Redbook					
#	Name	File name	#	Day_WRONG	0-20	Days	Days	Perm	0-20	0-10	0-6	%	%	%	%	Mon1 Pic	Mon 1	 1-5	#	Mon2 Pic	Mon 2	 1-5	#	Mon3 Pic	Mon 3	 1-5	#	Track	EAX Environments	Map Designer	Notes	in area	
1	Emerald Island	Out01.Odm	0	0	0	672	7	0	0	1	0	10	100	0	0	Dragonfly	Dragonfly	1	 2-5	0	0	1	 1-3	0	0	1	 1-3	20	FOREST	0	"Training Island--Lush, tropical island"		x
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getMapStats().empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(	Map Stats			First					x5	D20's		Random Encounter																					
			Reset	Visit	Per	Refil	Alert	Steal	Lock	Trap	Tres	Enc	M1	M2	M3			Dif	Appear			Dif	Appear			Dif	Appear	Redbook					
#	Name	File name	#	Day	0-20	Days	Days	Perm	0-20	0-10	0-6	%	%	%	%	Mon1 Pic	Mon 1	 1-5	#	Mon2 Pic	Mon 2	 1-5	#	Mon3 Pic	Mon 3	 1-5	#	Track	EAX Environments	Map Designer	Notes	in area	
1	Emerald Island	Out01.Odm	0	0	0	672	7	0	0	1	0	10	100	0	0	Dragonfly	Dragonfly	1	 2-5	0	0	1	 1-3	0	0	1	 1-3	20	FOREST	0	"Training Island--Lush, tropical island"		x
MALFORMED	LINE
2	Harmondale	Out02.Odm	0	0	2	672	7	1	2	1	0	10	100	0	0	Goblin	Goblin	3	 2-5	Swordsman	Swordsman	3	 1-3	0	0	1	 1-3	4	PLAINS	0	Home town map--hills and scattered trees.  Water ok		x
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getMapStats().size() == 2); // 2 items parsed (malformed line skipped)
    }
}
