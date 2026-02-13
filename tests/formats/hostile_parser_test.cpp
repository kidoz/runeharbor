// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/hostile_parser.hpp"
#include "../../src/util/console_logger.hpp"
#include "../../src/util/string_utils.hpp" 

TEST_CASE("HostileParser parsing HOSTILE.TXT", "[hostile_parser]")
{
    runeharbor::util::ConsoleLogger logger;
    runeharbor::formats::HostileParser parser(logger);

    // Sample data from test_extracted/HOSTILE.TXT - 3x3 matrix
    std::string sample_data = R"(	Party	Angel 	Archer
Party	0	0	0
Angel	0	0	0
Archer	0	0	0
)";
    
    std::vector<uint8_t> data(sample_data.begin(), sample_data.end());

    SECTION("Parser successfully parses sample data")
    {
        REQUIRE(parser.parse(data));
        const auto& hostileMatrix = parser.getHostileMatrix();

        // Check some specific hostility values
        REQUIRE(hostileMatrix.getHostility("Party", "Party") == 0);
        REQUIRE(hostileMatrix.getHostility("Party", "Angel") == 0);
        REQUIRE(hostileMatrix.getHostility("Party", "Archer") == 0);
        REQUIRE(hostileMatrix.getHostility("Angel", "Party") == 0);
        REQUIRE(hostileMatrix.getHostility("Angel", "Angel") == 0);
        REQUIRE(hostileMatrix.getHostility("Angel", "Archer") == 0);
        REQUIRE(hostileMatrix.getHostility("Archer", "Party") == 0);
        REQUIRE(hostileMatrix.getHostility("Archer", "Angel") == 0);
        REQUIRE(hostileMatrix.getHostility("Archer", "Archer") == 0);

        // Check non-existent entries
        REQUIRE(hostileMatrix.getHostility("NonExistent", "Party") == std::nullopt);
        REQUIRE(hostileMatrix.getHostility("Party", "NonExistent") == std::nullopt);

        // Check entity names (should be unique and sorted alphabetically from set)
        std::vector<std::string> expected_entities = {"Angel", "Archer", "Party"};
        auto actual_entities = hostileMatrix.getEntityNames();
        std::sort(actual_entities.begin(), actual_entities.end());
        REQUIRE(actual_entities == expected_entities);
    }

    SECTION("Empty data returns false")
    {
        std::vector<uint8_t> empty_data;
        REQUIRE_FALSE(parser.parse(empty_data));
        REQUIRE(parser.getHostileMatrix().getEntityNames().empty());
    }

    SECTION("Malformed header (missing column headers) returns false")
    {
        std::string malformed_header_data = R"(
Party	0	0	0
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getHostileMatrix().getEntityNames().empty());
    }

    SECTION("Malformed header (empty column labels after trimming) returns false")
    {
        std::string malformed_header_data = R"(	 	 	
Party	0	0	0
)";
        std::vector<uint8_t> malformed_vec(malformed_header_data.begin(), malformed_header_data.end());
        REQUIRE_FALSE(parser.parse(malformed_vec));
        REQUIRE(parser.getHostileMatrix().getEntityNames().empty());
    }

    SECTION("Malformed data row (missing row label) is skipped")
    {
        std::string malformed_row_data = R"(	Party	Angel 	Archer
	0	0	0
Angel	0	0	0
)";
        std::vector<uint8_t> malformed_vec(malformed_row_data.begin(), malformed_row_data.end());
        REQUIRE(parser.parse(malformed_vec));
        const auto& hostileMatrix = parser.getHostileMatrix();
        REQUIRE(hostileMatrix.getEntityNames().size() == 3); // Party, Angel, Archer
        REQUIRE(hostileMatrix.getHostility("Angel", "Party") == 0); // This row should still be parsed
        REQUIRE(hostileMatrix.getHostility("", "Party") == std::nullopt); // Empty row label is skipped
    }

    SECTION("Malformed data row (incorrect number of fields) is skipped")
    {
        std::string malformed_row_data = R"(	Party	Angel 	Archer
Party	0	0	0
Angel	0	0	0	0	0
Archer	0	0	0
)";
        std::vector<uint8_t> malformed_vec(malformed_row_data.begin(), malformed_row_data.end());
        REQUIRE(parser.parse(malformed_vec));
        const auto& hostileMatrix = parser.getHostileMatrix();
        REQUIRE(hostileMatrix.getEntityNames().size() == 3); // Party, Angel, Archer
        REQUIRE(hostileMatrix.getHostility("Party", "Party") == 0);
        REQUIRE(hostileMatrix.getHostility("Archer", "Party") == 0);
        REQUIRE(hostileMatrix.getHostility("Angel", "Party") == std::nullopt); // The malformed row was skipped, so Angel's data is not added.
    }
}
