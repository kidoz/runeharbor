// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/items_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" 

TEST_CASE("ItemsParser parsing ITEMS.TXT", "[items_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::ItemsParser parser(logger);

    // Sample data from test_extracted/ITEMS.TXT
    std::string sample_data = R"(Item #	Pic File	Name	Value	Equip Stat	Skill Group	Mod1	Mod2	material	ID/Rep/St	Not identified name	Sprite Index	VarA	VarB	Equip X	Equip Y	Notes
0	null	_item0	0	0	0	0	0	0	0	_item0	0	0	0	0	0	Description here.
1	item001	Crude Longsword	50	Weapon	Sword	3d3	0	8	1	Longsword	1	0	0	5	120	"Though notched and dented, this longsword is still an effective weapon."
2	item002	Elven Saber	200	Weapon	Sword	3d3	3	8	3	Saber	1	0	0	6	153	"A common elven weapon, this saber is a deadly, if unremarkable weapon."
3	item003	Keen Longsword	350	Weapon	Sword	3d3	6	8	6	Longsword	1	0	0	-2	118	"Although this longsword appears quite old, the edge of the blade is unusually sharp.  It was quite probably enchanted to remain that way during its creation."
160	null	_item160	0	0	0	0	0	0	0	_item160	0	0	0	0	0	Description here.
)"; // Adding an item with all zeros and a simple description for robust testing.
    
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& items = parser.getItems();
        REQUIRE(items.size() == 5); // 5 item entries in sample_data (excluding header)
    }

    SECTION("First item entry is correct")
    {
        parser.parse(data);
        const auto& items = parser.getItems();
        REQUIRE(items[0].id == 0);
        REQUIRE(items[0].picFile == "null");
        REQUIRE(items[0].name == "_item0");
        REQUIRE(items[0].value == 0);
        REQUIRE(items[0].equipStat == "0");
        REQUIRE(items[0].skillGroup == "0");
        REQUIRE(items[0].mod1 == "0");
        REQUIRE(items[0].mod2 == 0);
        REQUIRE(items[0].material == 0);
        REQUIRE(items[0].idRepSt == 0);
        REQUIRE(items[0].notIdentifiedName == "_item0");
        REQUIRE(items[0].spriteIndex == 0);
        REQUIRE(items[0].varA == 0);
        REQUIRE(items[0].varB == 0);
        REQUIRE(items[0].equipX == 0);
        REQUIRE(items[0].equipY == 0);
        REQUIRE(items[0].notes == "Description here.");
    }

    SECTION("Crude Longsword entry is correct")
    {
        parser.parse(data);
        const auto& items = parser.getItems();
        REQUIRE(items[1].id == 1);
        REQUIRE(items[1].picFile == "item001");
        REQUIRE(items[1].name == "Crude Longsword");
        REQUIRE(items[1].value == 50);
        REQUIRE(items[1].equipStat == "Weapon");
        REQUIRE(items[1].skillGroup == "Sword");
        REQUIRE(items[1].mod1 == "3d3");
        REQUIRE(items[1].mod2 == 0);
        REQUIRE(items[1].material == 8);
        REQUIRE(items[1].idRepSt == 1);
        REQUIRE(items[1].notIdentifiedName == "Longsword");
        REQUIRE(items[1].spriteIndex == 1);
        REQUIRE(items[1].varA == 0);
        REQUIRE(items[1].varB == 0);
        REQUIRE(items[1].equipX == 5);
        REQUIRE(items[1].equipY == 120);
        REQUIRE(items[1].notes == "Though notched and dented, this longsword is still an effective weapon.");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getItems().empty());
    }

    SECTION("Malformed header (missing first line) returns false")
    {
        std::string malformed_header_data = R"(1	item001	Crude Longsword	50	Weapon	Sword	3d3	0	8	1	Longsword	1	0	0	5	120	"Notes"
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getItems().empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(Item #	Pic File	Name	Value	Equip Stat	Skill Group	Mod1	Mod2	material	ID/Rep/St	Not identified name	Sprite Index	VarA	VarB	Equip X	Equip Y	Notes
1	item001	Crude Longsword	50	Weapon	Sword	3d3	0	8	1	Longsword	1	0	0	5	120	"Notes"
MALFORMED	LINE
2	item002	Elven Saber	200	Weapon	Sword	3d3	3	8	3	Saber	1	0	0	6	153	"Notes"
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getItems().size() == 2); // 2 items parsed (malformed line skipped)
    }
}
