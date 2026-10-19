// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/monsters_parser.hpp"
#include "../../src/game/combat.hpp"
#include "../../src/game/game_world.hpp"
#include "../../src/game/inventory.hpp"
#include "../../src/util/console_logger.hpp"

using namespace runeharbor;
using namespace runeharbor::formats;
using namespace runeharbor::game;

namespace
{

MonsterEntry makeMonster(std::string treasure, std::string special)
{
    MonsterEntry def;
    def.id = 1;
    def.name = "Test Looter";
    def.level = 1;
    def.hitPoints = 10;
    def.experience = 0;
    def.treasure = std::move(treasure);
    def.special = std::move(special);
    def.recovery = 10;
    def.attack1.type = "Phys";
    def.attack1.damage = "1d1";
    def.hitPoints = 10;
    return def;
}

} // namespace

TEST_CASE("Monster death drops treasure into the spawned-item list", "[game][combat][loot]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    Inventory inventory(logger);
    CombatSystem combat(logger);
    combat.setGameWorld(&world);
    combat.setInventory(&inventory);
    world.setCurrentMap("test.odm");

    combat.loadMonsterData({makeMonster("100%10", "")});
    const int index = combat.spawnMonster(1, 100.0f, 100.0f, 0.0f);
    auto* monster = combat.getMonster(index);
    REQUIRE(monster != nullptr);

    combat.awardMonsterKill(*monster);

    const auto* items = world.getSpawnedMapItems("test.odm");
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    CHECK((*items)[0].itemType == 0); // gold
    CHECK((*items)[0].count == 10);

    // The death pipeline must be idempotent.
    combat.awardMonsterKill(*monster);
    items = world.getSpawnedMapItems("test.odm");
    REQUIRE(items != nullptr);
    CHECK(items->size() == 1);
}

TEST_CASE("Exploding monsters damage nearby party members on death", "[game][combat][loot]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    Inventory inventory(logger);
    CombatSystem combat(logger);
    combat.setGameWorld(&world);
    combat.setInventory(&inventory);
    world.setCurrentMap("test.odm");

    auto& ch = world.party().member(0);
    ch.maxHitPoints = 100;
    ch.hitPoints = 100;

    combat.loadMonsterData({makeMonster("", "\"explode,10,fire\"")});
    const int index = combat.spawnMonster(1, world.party().worldX(), world.party().worldY(),
                                          world.party().worldZ());
    auto* monster = combat.getMonster(index);
    REQUIRE(monster != nullptr);

    combat.awardMonsterKill(*monster);

    CHECK(ch.hitPoints == 90);
}

TEST_CASE("Monster bonus and damage-type mappings", "[game][combat]")
{
    CHECK(CombatSystem::conditionFromBonusName("Poison2") == ConditionIndex::Poison2);
    CHECK(CombatSystem::conditionFromBonusName("Disease3") == ConditionIndex::Disease3);
    CHECK(CombatSystem::conditionFromBonusName("Errad") == ConditionIndex::Eradicated);
    CHECK_FALSE(CombatSystem::conditionFromBonusName("0").has_value());
    CHECK_FALSE(CombatSystem::conditionFromBonusName("BrkArmor").has_value());

    CHECK(CombatSystem::damageElementFromMonsterType("Phys") == DamageElement::Physical);
    CHECK(CombatSystem::damageElementFromMonsterType("Fire") == DamageElement::Fire);
    CHECK(CombatSystem::damageElementFromMonsterType("Ener") == DamageElement::Light);
    CHECK(CombatSystem::damageElementFromMonsterType("unknown") == DamageElement::Physical);
}
