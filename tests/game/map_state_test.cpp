// SPDX-License-Identifier: MIT
//
// Per-map monster runtime storage. An entry (including an empty list) marks a
// visited map, distinguishing "cleared" from "never entered" so the generated
// roster is not respawned after the party has killed everything.
#include <catch2/catch_test_macros.hpp>

#include "../../src/game/game_world.hpp"

using namespace runeharbor::game;

TEST_CASE("Saved map monster lists distinguish cleared from unvisited maps", "[game][state]")
{
    GameWorld world;
    CHECK_FALSE(world.hasSavedMapMonsters("out01.odm"));
    CHECK(world.getSavedMapMonsters("out01.odm") == nullptr);

    // An empty list is stored (not erased): the map was visited and cleared.
    world.setSavedMapMonsters("out01.odm", {});
    CHECK(world.hasSavedMapMonsters("out01.odm"));
    REQUIRE(world.getSavedMapMonsters("out01.odm") != nullptr);
    CHECK(world.getSavedMapMonsters("out01.odm")->empty());

    SavedMonsterState monster;
    monster.monsterId = 7;
    monster.currentHP = 12;
    monster.x = 1.0f;
    world.setSavedMapMonsters("out02.odm", {monster});
    REQUIRE(world.getSavedMapMonsters("out02.odm") != nullptr);
    REQUIRE(world.getSavedMapMonsters("out02.odm")->size() == 1);
    CHECK((*world.getSavedMapMonsters("out02.odm"))[0].monsterId == 7);

    world.clearSavedMapMonsters("out01.odm");
    CHECK_FALSE(world.hasSavedMapMonsters("out01.odm"));

    world.reset();
    CHECK_FALSE(world.hasSavedMapMonsters("out02.odm"));
}
