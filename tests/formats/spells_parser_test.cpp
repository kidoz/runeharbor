// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/spells_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" // For trim if needed in test, or for robustness in splitString based parsing if any issues are found.

TEST_CASE("SpellsParser parsing SPELLS.TXT", "[spells_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::SpellsParser parser(logger);

    // Sample data from test_extracted/SPELLS.TXT
    std::string sample_data = R"(	

	Lvl	Fire Spells	Res		Spell Description					
#	Lvl	Fire Spells	Res	Short Name	Spell Description	Normal	Expert	Master	Grand Master	Stats
1	1	Torch Light	none	Torch Light	"Torch light increases the radius of light surrounding your party in the dark.  "	Duration 1 hour per point of  skill	Brighter light	Brightest Light	Faster recovery	P
2	2	Fire Bolt	Fire	Fire Bolt	"Launches a burst of fire at a single target.  Damage is 1-3 points of damage per point of skill in Fire Magic, but casting cost is low.  Firebolt is safe and effectivethe Old Reliable of the Sorcerers arsenal."	Slow rate of recovery	Faster recovery rate	Faster recovery rate	Fastest recovery rate	PMEC
3	3	Fire Resistance	none	Prot Fire	Increases all your characters resistance to fire magic by an amount equal to your skill in Fire and lasts one hour per point of skill.	1 point resistance per point of skill	2 points resistance per point of skill	3 points resistance per point of skill	4 points resistance per point of skill	P
)"; // Added a few more lines for robustness

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& spells = parser.getSpells();
        REQUIRE(spells.size() == 3); // 3 spell entries in sample_data
    }

    SECTION("First spell entry is correct")
    {
        parser.parse(data);
        const auto& spells = parser.getSpells();
        REQUIRE(spells[0].id == 1);
        REQUIRE(spells[0].level == 1);
        REQUIRE(spells[0].name == "Torch Light");
        REQUIRE(spells[0].resistance == "none");
        REQUIRE(spells[0].shortName == "Torch Light");
        REQUIRE(spells[0].description ==
                "Torch light increases the radius of light surrounding your party in the dark.  ");
        REQUIRE(spells[0].normalEffect == "Duration 1 hour per point of  skill");
        REQUIRE(spells[0].expertEffect == "Brighter light");
        REQUIRE(spells[0].masterEffect == "Brightest Light");
        REQUIRE(spells[0].grandMasterEffect == "Faster recovery");
        REQUIRE(spells[0].stats == "P");
    }

    SECTION("Second spell entry (with complex description) is correct")
    {
        parser.parse(data);
        const auto& spells = parser.getSpells();
        REQUIRE(spells[1].id == 2);
        REQUIRE(spells[1].name == "Fire Bolt");
        REQUIRE(spells[1].description.rfind("Old Reliable") !=
                std::string::npos); // Check for part of the description
        REQUIRE(spells[1].stats == "PMEC");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getSpells().empty());
    }

    SECTION("Malformed header (missing main header) still parses data")
    {
        std::string malformed_header_data = R"(	

	Lvl	Fire Spells	Res		Spell Description					
1	1	Torch Light	none	Torch Light	"Torch light increases the radius of light surrounding your party in the dark.  "	Duration 1 hour per point of  skill	Brighter light	Brightest Light	Faster recovery	P
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(),
                                           malformed_header_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getSpells().size() == 1); // Only 1 spell parsed
    }

    SECTION("Malformed data line (too few fields) is skipped")
    {
        std::string malformed_line_data = R"(	

	Lvl	Fire Spells	Res		Spell Description					
#	Lvl	Fire Spells	Res	Short Name	Spell Description	Normal	Expert	Master	Grand Master	Stats
1	1	Torch Light	none	Torch Light	"Torch light increases the radius of light surrounding your party in the dark.  "	Duration 1 hour per point of  skill	Brighter light	Brightest Light	Faster recovery	P
MALFORMED	LINE
2	2	Fire Bolt	Fire	Fire Bolt	"Launches a burst of fire at a single target.  Damage is 1-3 points of damage per point of skill in Fire Magic, but casting cost is low.  Firebolt is safe and effectivethe Old Reliable of the Sorcerers arsenal."	Slow rate of recovery	Faster recovery rate	Faster recovery rate	Fastest recovery rate	PMEC
)";
        std::vector<uint8_t> malformed_vec(malformed_line_data.begin(), malformed_line_data.end());
        REQUIRE(parser.parse(malformed_vec));
        REQUIRE(parser.getSpells().size() == 2); // 2 spells parsed (malformed line skipped)
    }
}
