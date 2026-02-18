// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/credits_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp"

TEST_CASE("CreditsParser parsing Credits.txt", "[credits_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::CreditsParser parser(logger);

    // Sample data from test_extracted/Credits.txt
    std::string sample_data = R"(Intro Line 1
Intro Line 2

_Created by:
Jon Van Caneghem
Paul Rattner

_Executive Producer:
Mark Caldwell
)";

    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& sections = parser.getCreditsSections();
        REQUIRE(sections.size() == 3); // Introduction, Created by, Executive Producer
    }

    SECTION("Introduction section is correct")
    {
        parser.parse(data);
        const auto& sections = parser.getCreditsSections();
        REQUIRE(sections[0].title == "Introduction");
        REQUIRE(sections[0].content.size() == 2);
        REQUIRE(sections[0].content[0] == "Intro Line 1");
        REQUIRE(sections[0].content[1] == "Intro Line 2");
    }

    SECTION("Created by section is correct")
    {
        parser.parse(data);
        const auto& sections = parser.getCreditsSections();
        REQUIRE(sections[1].title == "Created by:");
        REQUIRE(sections[1].content.size() == 2);
        REQUIRE(sections[1].content[0] == "Jon Van Caneghem");
        REQUIRE(sections[1].content[1] == "Paul Rattner");
    }

    SECTION("Executive Producer section is correct")
    {
        parser.parse(data);
        const auto& sections = parser.getCreditsSections();
        REQUIRE(sections[2].title == "Executive Producer:");
        REQUIRE(sections[2].content.size() == 1);
        REQUIRE(sections[2].content[0] == "Mark Caldwell");
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getCreditsSections().empty());
    }
}
