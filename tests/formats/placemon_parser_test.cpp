// SPDX-License-Identifier: MIT
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../../src/formats/placemon_parser.hpp"
#include "../../src/util/console_logger.hpp"

using namespace runeharbor;
using namespace runeharbor::formats;
using namespace runeharbor::util;

TEST_CASE("PlacemonParser parses the id-to-name table", "[placemon_parser]")
{
    ConsoleLogger logger;
    PlacemonParser parser(logger);

    SECTION("Parses CRLF rows and ignores the header")
    {
        const std::string sample = "\tPlaced Monster Names\r\n"
                                   "1\tguard\r\n"
                                   "2\tCleric of Baa\r\n"
                                   "3\tHigh Priest of Baa\r\n"
                                   "14\t0\r\n";

        std::vector<uint8_t> data(sample.begin(), sample.end());
        REQUIRE(parser.parse(data));

        const auto& entries = parser.getEntries();
        REQUIRE(entries.size() == 3);
        REQUIRE(entries[0].id == 1);
        REQUIRE(entries[0].name == "guard");
        REQUIRE(entries[1].id == 2);
        REQUIRE(entries[1].name == "Cleric of Baa");
        REQUIRE(entries[2].id == 3);
        REQUIRE(entries[2].name == "High Priest of Baa");
    }

    SECTION("Empty input is rejected")
    {
        std::vector<uint8_t> empty;
        REQUIRE_FALSE(parser.parse(empty));
        REQUIRE(parser.getEntries().empty());
    }

    SECTION("Header-only input is rejected")
    {
        const std::string sample = "\tPlaced Monster Names\r\n";
        std::vector<uint8_t> data(sample.begin(), sample.end());
        REQUIRE_FALSE(parser.parse(data));
        REQUIRE(parser.getEntries().empty());
    }
}
