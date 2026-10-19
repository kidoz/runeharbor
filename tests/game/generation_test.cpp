// SPDX-License-Identifier: MIT
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/odm_map.hpp"
#include "../../src/game/generation.hpp"
#include "../../src/util/console_logger.hpp"

using namespace runeharbor;
using namespace runeharbor::game;

TEST_CASE("Generated chests carry the spawn position", "[game][generation][chest]")
{
    util::ConsoleLogger logger;
    ContentGenerator generator(logger);
    generator.setWorldSeed(0x1234);

    std::vector<formats::ODMSpawnPoint> spawns;
    for (int i = 0; i < 24; ++i)
    {
        formats::ODMSpawnPoint spawn{};
        spawn.x = 1000 + i * 256;
        spawn.y = 2000 - i * 128;
        spawn.z = 300 + i;
        spawn.radius = 32;
        spawn.objectType = 0x16; // chest-bearing spawn type
        spawn.objectIndex = 0;
        spawn.group = 0;
        spawns.push_back(spawn);
    }

    GenerationConfig config;
    config.mapDifficulty = 10;
    config.maxMonsters = 64;
    config.maxChests = 16;

    const auto content = generator.generateForMap("test.odm", {}, spawns, config);

    REQUIRE_FALSE(content.chests.empty());
    for (const auto& chest : content.chests)
    {
        bool matchesSpawn = false;
        for (const auto& spawn : spawns)
        {
            if (chest.x == static_cast<float>(spawn.x) && chest.y == static_cast<float>(spawn.y) &&
                chest.z == static_cast<float>(spawn.z))
            {
                matchesSpawn = true;
                break;
            }
        }
        CHECK(matchesSpawn);
    }
}
