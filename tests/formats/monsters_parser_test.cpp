// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/monsters_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp"

TEST_CASE("MonstersParser parsing MONSTERS.TXT", "[monsters_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::MonstersParser parser(logger);

    // Sample data from test_extracted/MONSTERS.TXT
    std::string sample_data =
        R"(Default Monster Data					Stats								Movement and Combat					Attack 1			Attack 2			    Spell Attack		    Spell Attack 2		Resistances										Misc
#	Name	Picture	LVL	 HP 	AC	 EXP 	Treasure	Quest	Fly	Move	AI Type	Hst	Spd	Rec	Pref	Bonus	Type	Damage	Miss	Att%	Type	Damage	Miss	Use%	"Spl,Mas,Skil"	Use%	"Spl,Mas,Skil"	Fire	Air	Water	Earth	Mind	Spirit	Body	Light	Dark	Phys	Special
1	Angel	Angel A	30	180	25	1200	5%50D20+L4Sword	1	Y	Free	Aggress	3	250	60	0	0	Phys	2D8+10	0	0	0	0	0	30	"Light Bolt,M,8"	20	"Dispel Magic,M,8"	20	20	20	20	30	15	30	Imm	10	20	0
2	Angel Lord	Angel B	50	400	35	3000	10%75D20+L5Sword	1	Y	Free	Aggress	3	275	50	0	0	Phys	2D8+15	0	0	0	0	0	30	"Light Bolt,M,12"	50	"Day of Protection,M,12"	30	30	30	30	40	15	40	Imm	10	20	0
3	Archangel	Angel C	70	700	45	5600	15%100D20+L6Sword	1	Y	Free	Suicidal	3	300	40	2	0	Phys	2D8+20	0	0	0	0	0	30	"Light Bolt,G,16"	50	"Hour of Power,G,16"	40	40	40	40	50	15	50	Imm	10	20	0
264	Mega-Dragon	zUltra Dragon C	100	" 1,300 "	100	" 11,000 "	0	1	Y	Short	Suicidal	4	300	30	4	Errad	Ener	20D8	Ener	0	0	0	0	0	0	0	0	Imm	70	70	70	70	15	Imm	30	30	70	"Summon,air,Dragon C"
)";

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& monsters = parser.getMonsters();
        REQUIRE(monsters.size() == 4); // 4 monster entries in sample_data (excluding two headers)
    }

    SECTION("Angel entry is correct")
    {
        parser.parse(data);
        const auto& monsters = parser.getMonsters();
        REQUIRE(monsters[0].id == 1);
        REQUIRE(monsters[0].name == "Angel");
        REQUIRE(monsters[0].picture == "Angel A");
        REQUIRE(monsters[0].level == 30);
        REQUIRE(monsters[0].hitPoints == 180);
        REQUIRE(monsters[0].armorClass == 25);
        REQUIRE(monsters[0].experience == 1200);
        REQUIRE(monsters[0].treasure == "5%50D20+L4Sword");
        REQUIRE(monsters[0].quest == 1);
        REQUIRE(monsters[0].canFly == true);
        REQUIRE(monsters[0].moveType == "Free");
        REQUIRE(monsters[0].aiType == "Aggress");
        REQUIRE(monsters[0].haste == 3);
        REQUIRE(monsters[0].speed == 250);
        REQUIRE(monsters[0].recovery == 60);
        REQUIRE(monsters[0].preferences == "0");
        REQUIRE(monsters[0].bonus == "0");

        REQUIRE(monsters[0].attack1.type == "Phys");
        REQUIRE(monsters[0].attack1.damage == "2D8+10");
        REQUIRE(monsters[0].attack1.miss == "0");
        REQUIRE(monsters[0].attack1.attPercent == 0);

        REQUIRE(monsters[0].attack2.type == "0");
        REQUIRE(monsters[0].attack2.damage == "0");
        REQUIRE(monsters[0].attack2.miss == "0");
        // No attPercent for attack2 in this entry

        REQUIRE(monsters[0].spellAttack1.usePercent == 30);
        REQUIRE(monsters[0].spellAttack1.spellMasterySkill == "Light Bolt,M,8");

        REQUIRE(monsters[0].spellAttack2.usePercent == 20);
        REQUIRE(monsters[0].spellAttack2.spellMasterySkill == "Dispel Magic,M,8");

        REQUIRE(monsters[0].resistFire == 20);
        REQUIRE(monsters[0].resistAir == 20);
        REQUIRE(monsters[0].resistWater == 20);
        REQUIRE(monsters[0].resistEarth == 20);
        REQUIRE(monsters[0].resistMind == 30);
        REQUIRE(monsters[0].resistSpirit == 15);
        REQUIRE(monsters[0].resistBody == 30);
        REQUIRE(monsters[0].resistLight == 100); // Imm
        REQUIRE(monsters[0].resistDark == 10);
        REQUIRE(monsters[0].resistPhysical == 20);
        REQUIRE(monsters[0].special == "0");
    }

    SECTION("Mega-Dragon entry with commas and Imm resistances is correct")
    {
        parser.parse(data);
        const auto& monsters = parser.getMonsters();
        // This is the last entry in the sample data
        REQUIRE(monsters[3].id == 264);
        REQUIRE(monsters[3].name == "Mega-Dragon");
        REQUIRE(monsters[3].hitPoints == 1300);   // Check cleaned HP
        REQUIRE(monsters[3].experience == 11000); // Check cleaned EXP
        REQUIRE(monsters[3].canFly == true);
        REQUIRE(monsters[3].aiType == "Suicidal");
        REQUIRE(monsters[3].bonus == "Errad");

        REQUIRE(monsters[3].attack1.type == "Ener");
        REQUIRE(monsters[3].attack1.damage == "20D8");

        REQUIRE(monsters[3].resistFire == 100); // Imm
        REQUIRE(monsters[3].resistAir == 70);
        REQUIRE(monsters[3].resistWater == 70);
        REQUIRE(monsters[3].resistEarth == 70);
        REQUIRE(monsters[3].resistMind == 70);
        REQUIRE(monsters[3].resistSpirit == 15);
        REQUIRE(monsters[3].resistBody == 100); // Imm
        REQUIRE(monsters[3].resistLight == 30);
        REQUIRE(monsters[3].resistDark == 30);
        REQUIRE(monsters[3].resistPhysical == 70);
        REQUIRE(monsters[3].special == "Summon,air,Dragon C");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getMonsters().empty());
    }

    SECTION("Malformed header (missing category header) returns false")
    {
        std::string malformed_header_data =
            R"(#	Name	Picture	LVL	 HP 	AC	 EXP 	Treasure	Quest	Fly	Move	AI Type	Hst	Spd	Rec	Pref	Bonus	Type	Damage	Miss	Att%	Type	Damage	Miss	Use%	"Spl,Mas,Skil"	Use%	"Spl,Mas,Skil"	Fire	Air	Water	Earth	Mind	Spirit	Body	Light	Dark	Phys	Special
1	Angel	Angel A	30	180	25	1200	5%50D20+L4Sword	1	Y	Free	Aggress	3	250	60	0	0	Phys	2D8+10	0	0	0	0	0	30	"Light Bolt,M,8"	20	"Dispel Magic,M,8"	20	20	20	20	30	15	30	Imm	10	20	0
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(),
                                           malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getMonsters().empty());
    }

    SECTION("Malformed header (incorrect field names header) returns false")
    {
        std::string malformed_header_data = R"(Default Monster Data
#	Name	Picture	LVL	 HP 	AC	 EXP 	Treasure	Quest	Fly	Move	AI Type	Hst	Spd	Rec	Pref	Bonus	Type	Damage	Miss	Att%	Type	Damage	Miss	Use%	"Spl,Mas,Skil"	Use%	"Spl,Mas,Skil"	Fire	Air	Water	Earth	Mind	Spirit	Body	Light	Dark	Phys	Special_WRONG
1	Angel	Angel A	30	180	25	1200	5%50D20+L4Sword	1	Y	Free	Aggress	3	250	60	0	0	Phys	2D8+10	0	0	0	0	0	30	"Light Bolt,M,8"	20	"Dispel Magic,M,8"	20	20	20	20	30	15	30	Imm	10	20	0
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(),
                                           malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getMonsters().empty());
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(Default Monster Data
#	Name	Picture	LVL	 HP 	AC	 EXP 	Treasure	Quest	Fly	Move	AI Type	Hst	Spd	Rec	Pref	Bonus	Type	Damage	Miss	Att%	Type	Damage	Miss	Use%	"Spl,Mas,Skil"	Use%	"Spl,Mas,Skil"	Fire	Air	Water	Earth	Mind	Spirit	Body	Light	Dark	Phys	Special
1	Angel	Angel A	30	180	25	1200	5%50D20+L4Sword	1	Y	Free	Aggress	3	250	60	0	0	Phys	2D8+10	0	0	0	0	0	30	"Light Bolt,M,8"	20	"Dispel Magic,M,8"	20	20	20	20	30	15	30	Imm	10	20	0
MALFORMED	LINE
2	Angel Lord	Angel B	50	400	35	3000	10%75D20+L5Sword	1	Y	Free	Aggress	3	275	50	0	0	Phys	2D8+15	0	0	0	0	0	30	"Light Bolt,M,12"	50	"Day of Protection,M,12"	30	30	30	30	40	15	40	Imm	10	20	0
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getMonsters().size() == 2); // 2 items parsed (malformed line skipped)
    }
}
