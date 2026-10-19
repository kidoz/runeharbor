// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/formats/items_parser.hpp"
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

MonsterEntry makeTarget(int hitPoints)
{
    MonsterEntry def;
    def.id = 2;
    def.name = "Test Target";
    def.level = 1;
    def.hitPoints = hitPoints;
    def.experience = 0;
    def.recovery = 10;
    def.attack1.type = "Phys";
    def.attack1.damage = "1d1";
    return def;
}

ItemEntry makeWeapon(int id, std::string equipStat, std::string skillGroup, std::string mod1)
{
    ItemEntry def;
    def.id = id;
    def.name = "Test Weapon";
    def.value = 10;
    def.equipStat = std::move(equipStat);
    def.skillGroup = std::move(skillGroup);
    def.mod1 = std::move(mod1);
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

namespace
{

// Build a party member with a loaded bow or sword equipped. Returns the
// monster instance index spawned at (x, y, z).
struct RangedTestSetup
{
    util::ConsoleLogger logger;
    GameWorld world;
    Inventory inventory{logger};
    CombatSystem combat{logger};
    int monsterIndex = -1;

    RangedTestSetup(bool withBow, bool withSword, float monsterX, int monsterHP)
    {
        combat.setGameWorld(&world);
        combat.setInventory(&inventory);
        inventory.setParty(&world.party());
        world.setCurrentMap("test.odm");
        world.party().setWorldPosition(0.0f, 0.0f, 0.0f);

        auto& ch = world.party().member(0);
        ch.stats.accuracy = 100;
        ch.baseStats.accuracy = 100;
        ch.stats.might = 10;
        ch.baseStats.might = 10;
        ch.skillLevels[static_cast<size_t>(SkillId::Bow)] = {.level = 10,
                                                             .mastery = SkillMastery::Master};
        ch.skillLevels[static_cast<size_t>(SkillId::Sword)] = {.level = 1,
                                                               .mastery = SkillMastery::Normal};

        std::vector<ItemEntry> defs;
        if (withBow)
        {
            defs.push_back(makeWeapon(100, "missile", "Bow", "10d1"));
        }
        if (withSword)
        {
            defs.push_back(makeWeapon(101, "weapon", "Sword", "1d1"));
        }
        inventory.loadItemData(defs);

        if (withBow)
        {
            Item bow;
            bow.itemId = 100;
            bow.identified = true;
            REQUIRE(inventory.addToBackpack(0, bow));
            REQUIRE(inventory.equip(0, 0));
        }
        if (withSword)
        {
            Item sword;
            sword.itemId = 101;
            sword.identified = true;
            REQUIRE(inventory.addToBackpack(0, sword));
            REQUIRE(inventory.equip(0, 0));
        }

        combat.loadMonsterData({makeTarget(monsterHP)});
        monsterIndex = combat.spawnMonster(2, monsterX, 0.0f, 0.0f);
        REQUIRE(combat.getMonster(monsterIndex) != nullptr);
    }
};

} // namespace

TEST_CASE("Player uses the equipped bow beyond melee reach", "[game][combat][ranged]")
{
    RangedTestSetup setup(/*withBow=*/true, /*withSword=*/false, /*monsterX=*/2000.0f,
                          /*monsterHP=*/100000);

    bool sawHit = false;
    for (int i = 0; i < 40; ++i)
    {
        const auto result = setup.combat.playerAttack(0, setup.monsterIndex);
        CHECK_FALSE(result.description.contains("cannot reach"));
        CHECK_FALSE(result.description.contains("out of range"));
        if (result.hit)
        {
            sawHit = true;
            // 10d1 dice + 20 Master training bonus; melee never applies here.
            CHECK(result.damage >= 10);
        }
    }
    CHECK(sawHit);
}

TEST_CASE("Bow is used at melee range when no melee weapon is equipped", "[game][combat][ranged]")
{
    RangedTestSetup setup(/*withBow=*/true, /*withSword=*/false, /*monsterX=*/100.0f,
                          /*monsterHP=*/100000);

    bool sawHit = false;
    for (int i = 0; i < 40; ++i)
    {
        const auto result = setup.combat.playerAttack(0, setup.monsterIndex);
        if (result.hit)
        {
            sawHit = true;
            // An unarmed fallback would deal ~1 damage; the bow deals 10+.
            CHECK(result.damage >= 10);
        }
    }
    CHECK(sawHit);
}

TEST_CASE("Melee attack still works inside reach with a sword", "[game][combat][ranged]")
{
    RangedTestSetup setup(/*withBow=*/false, /*withSword=*/true, /*monsterX=*/100.0f,
                          /*monsterHP=*/100000);

    bool sawHit = false;
    for (int i = 0; i < 40; ++i)
    {
        const auto result = setup.combat.playerAttack(0, setup.monsterIndex);
        CHECK_FALSE(result.description.contains("cannot reach"));
        if (result.hit)
        {
            sawHit = true;
            CHECK(result.damage >= 1);
            CHECK(result.damage <= 2); // 1d1, at most a double-damage critical
        }
    }
    CHECK(sawHit);
}

TEST_CASE("Unarmed attack cannot reach a distant target", "[game][combat][ranged]")
{
    RangedTestSetup setup(/*withBow=*/false, /*withSword=*/false, /*monsterX=*/2000.0f,
                          /*monsterHP=*/100000);

    const auto result = setup.combat.playerAttack(0, setup.monsterIndex);
    CHECK_FALSE(result.hit);
    CHECK(result.description.contains("cannot reach"));
}

TEST_CASE("Ranged attack rejects targets beyond the projectile range limit",
          "[game][combat][ranged]")
{
    RangedTestSetup setup(/*withBow=*/true, /*withSword=*/false, /*monsterX=*/6000.0f,
                          /*monsterHP=*/100000);

    const auto result = setup.combat.playerAttack(0, setup.monsterIndex);
    CHECK_FALSE(result.hit);
    CHECK(result.description.contains("out of range"));
}
