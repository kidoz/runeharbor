// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

#include "../../src/game/combat.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::game;
using namespace runeharbor::formats;

namespace
{

class NullLogger : public runeharbor::util::ILogger
{
  public:
    void log(runeharbor::util::LogLevel, std::string_view) override {}
};

MonsterEntry makeGoblin()
{
    MonsterEntry e;
    e.id = 1;
    e.name = "Goblin";
    e.level = 3;
    e.hitPoints = 20;
    e.armorClass = 5;
    e.experience = 30;
    e.speed = 100;
    e.recovery = 10;
    e.aiType = "Normal";
    e.resistFire = 10;
    e.resistPhysical = 0;
    e.attack1.damage = "1d6";
    return e;
}

MonsterEntry makeDragon()
{
    MonsterEntry e;
    e.id = 2;
    e.name = "Dragon";
    e.level = 20;
    e.hitPoints = 500;
    e.armorClass = 30;
    e.experience = 1000;
    e.speed = 200;
    e.recovery = 15;
    e.aiType = "Aggressive";
    e.resistFire = 100;
    e.resistPhysical = 50;
    return e;
}

MonsterEntry makeFriendlyNpc()
{
    MonsterEntry e;
    e.id = 3;
    e.name = "Villager";
    e.level = 1;
    e.hitPoints = 10;
    e.armorClass = 0;
    e.experience = 0;
    e.speed = 50;
    e.recovery = 10;
    e.aiType = "Friendly";
    return e;
}

} // namespace

TEST_CASE("CombatSystem::rollDamage dice parsing", "[game][combat]")
{
    SECTION("plain number")
    {
        REQUIRE(CombatSystem::rollDamage("10") == 10);
        REQUIRE(CombatSystem::rollDamage("0") == 0);
    }

    SECTION("empty string returns 0")
    {
        REQUIRE(CombatSystem::rollDamage("") == 0);
    }

    SECTION("1d1 always returns 1")
    {
        for (int i = 0; i < 20; i++)
        {
            REQUIRE(CombatSystem::rollDamage("1d1") == 1);
        }
    }

    SECTION("NdM is in valid range")
    {
        for (int i = 0; i < 50; i++)
        {
            int result = CombatSystem::rollDamage("2d6");
            REQUIRE(result >= 2);
            REQUIRE(result <= 12);
        }
    }

    SECTION("NdM+B adds bonus")
    {
        for (int i = 0; i < 50; i++)
        {
            int result = CombatSystem::rollDamage("1d1+5");
            REQUIRE(result == 6);
        }
    }

    SECTION("NdM-B subtracts (clamped to 0)")
    {
        for (int i = 0; i < 20; i++)
        {
            int result = CombatSystem::rollDamage("1d1-10");
            REQUIRE(result == 0); // 1 - 10 = -9, clamped to 0
        }
    }
}

TEST_CASE("CombatSystem::hitChance", "[game][combat]")
{
    NullLogger logger;
    CombatSystem combat(logger);

    SECTION("base 50% with equal stats")
    {
        // 50 + (10-10)*2 + 1*2 = 52
        REQUIRE(combat.hitChance(1, 10, 10) == 52);
    }

    SECTION("high accuracy vs low AC gives high chance")
    {
        // 50 + (30-5)*2 + 10*2 = 50 + 50 + 20 = 120, clamped to 95
        REQUIRE(combat.hitChance(10, 30, 5) == 95);
    }

    SECTION("low accuracy vs high AC gives low chance")
    {
        // 50 + (1-50)*2 + 1*2 = 50 - 98 + 2 = -46, clamped to 5
        REQUIRE(combat.hitChance(1, 1, 50) == 5);
    }

    SECTION("always between 5 and 95")
    {
        REQUIRE(combat.hitChance(100, 100, 0) == 95);
        REQUIRE(combat.hitChance(0, 0, 100) == 5);
    }
}

TEST_CASE("CombatSystem damage calculation", "[game][combat]")
{
    NullLogger logger;
    CombatSystem combat(logger);

    MonsterInstance target;
    target.resistFire = 50;
    target.resistPhysical = 0;

    SECTION("no resistance: full damage (min 1)")
    {
        REQUIRE(combat.calculateDamage(10, DamageElement::Physical, target) == 10);
    }

    SECTION("50% fire resistance halves fire damage")
    {
        REQUIRE(combat.calculateDamage(10, DamageElement::Fire, target) == 5);
    }

    SECTION("100% resistance gives min 1 damage")
    {
        target.resistFire = 100;
        REQUIRE(combat.calculateDamage(10, DamageElement::Fire, target) == 1);
    }

    SECTION("over 100% resistance still gives min 1")
    {
        target.resistFire = 150;
        REQUIRE(combat.calculateDamage(10, DamageElement::Fire, target) == 1);
    }
}

TEST_CASE("CombatSystem monster spawning", "[game][combat]")
{
    NullLogger logger;
    CombatSystem combat(logger);

    std::vector<MonsterEntry> defs = {makeGoblin(), makeDragon(), makeFriendlyNpc()};
    combat.loadMonsterData(defs);

    SECTION("spawn returns valid index")
    {
        int idx = combat.spawnMonster(1, 100.0f, 200.0f, 0.0f);
        REQUIRE(idx == 0);
        REQUIRE(combat.getMonsters().size() == 1);
    }

    SECTION("spawned monster has correct stats")
    {
        combat.spawnMonster(1, 0, 0, 0);
        auto* m = combat.getMonster(0);
        REQUIRE(m != nullptr);
        REQUIRE(m->name == "Goblin");
        REQUIRE(m->currentHP == 20);
        REQUIRE(m->maxHP == 20);
        REQUIRE(m->armorClass == 5);
        REQUIRE(m->level == 3);
        REQUIRE(m->isAlive());
    }

    SECTION("friendly monster is not hostile")
    {
        combat.spawnMonster(3, 0, 0, 0);
        auto* m = combat.getMonster(0);
        REQUIRE_FALSE(m->hostile);
    }

    SECTION("aggressive monster has extended aggro range")
    {
        combat.spawnMonster(2, 0, 0, 0);
        auto* m = combat.getMonster(0);
        REQUIRE(m->hostile);
        REQUIRE(m->aggroRange > 2048.0f); // 2048 * 1.5
    }

    SECTION("unknown monster ID uses fallback")
    {
        int idx = combat.spawnMonster(999, 0, 0, 0);
        auto* m = combat.getMonster(idx);
        REQUIRE(m != nullptr);
        REQUIRE(m->isAlive());
        REQUIRE(m->name.find("Unknown") != std::string::npos);
    }

    SECTION("clearMonsters removes all")
    {
        combat.spawnMonster(1, 0, 0, 0);
        combat.spawnMonster(2, 0, 0, 0);
        REQUIRE(combat.getMonsters().size() == 2);
        combat.clearMonsters();
        REQUIRE(combat.getMonsters().empty());
        REQUIRE_FALSE(combat.inCombat());
    }
}

TEST_CASE("CombatSystem alive monster count", "[game][combat]")
{
    NullLogger logger;
    CombatSystem combat(logger);

    std::vector<MonsterEntry> defs = {makeGoblin()};
    combat.loadMonsterData(defs);
    combat.spawnMonster(1, 0, 0, 0);
    combat.spawnMonster(1, 10, 0, 0);

    SECTION("all alive initially")
    {
        REQUIRE(combat.aliveMonsterCount() == 2);
    }

    SECTION("dead monster not counted")
    {
        auto* m = combat.getMonster(0);
        m->aiState = MonsterInstance::AIState::Dead;
        m->currentHP = 0;
        REQUIRE(combat.aliveMonsterCount() == 1);
    }
}

TEST_CASE("CombatSystem monster hostility by group", "[game][combat]")
{
    NullLogger logger;
    CombatSystem combat(logger);

    std::vector<MonsterEntry> defs = {makeGoblin()};
    combat.loadMonsterData(defs);
    combat.spawnMonster(1, 0, 0, 0, /*group=*/5);
    combat.spawnMonster(1, 10, 0, 0, /*group=*/5);
    combat.spawnMonster(1, 20, 0, 0, /*group=*/9);

    SECTION("set group hostile")
    {
        combat.setMonsterHostileByGroup(5, false);
        REQUIRE_FALSE(combat.getMonster(0)->hostile);
        REQUIRE_FALSE(combat.getMonster(1)->hostile);
        REQUIRE(combat.getMonster(2)->hostile); // different group
    }
}

TEST_CASE("CombatSystem getMonsterDef", "[game][combat]")
{
    NullLogger logger;
    CombatSystem combat(logger);

    std::vector<MonsterEntry> defs = {makeGoblin()};
    combat.loadMonsterData(defs);

    SECTION("returns def for known ID")
    {
        auto* def = combat.getMonsterDef(1);
        REQUIRE(def != nullptr);
        REQUIRE(def->name == "Goblin");
    }

    SECTION("returns nullptr for unknown ID")
    {
        REQUIRE(combat.getMonsterDef(999) == nullptr);
    }
}
