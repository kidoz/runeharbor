// SPDX-License-Identifier: MIT
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/spells_parser.hpp"
#include "../../src/game/character.hpp"
#include "../../src/game/game_world.hpp"
#include "../../src/game/spells.hpp"
#include "../../src/util/console_logger.hpp"

using namespace runeharbor;
using namespace runeharbor::formats;
using namespace runeharbor::game;

namespace
{

SpellEntry makeSpell(int id, const char* name, int level)
{
    SpellEntry spell;
    spell.id = id;
    spell.name = name;
    spell.shortName = name;
    spell.level = level;
    return spell;
}

void prepareCaster(Character& ch)
{
    ch.charClass = CharacterClass::Sorcerer;
    ch.level = 10;
    ch.maxHitPoints = 200;
    ch.hitPoints = 200;
    ch.maxSpellPoints = 200;
    ch.spellPoints = 200;
    ch.stats.intellect = 40;
    ch.baseStats = ch.stats;
    ch.recalculateDerived();
    ch.maxSpellPoints = 200;
    ch.spellPoints = 200;
}

} // namespace

TEST_CASE("Spellbook gates casting and available spells", "[game][spells]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData({makeSpell(1, "Fire Bolt", 1), makeSpell(45, "Shield", 1)});

    auto& ch = world.party().member(0);
    prepareCaster(ch);
    learnSkill(ch, SkillId::Fire, 10, SkillMastery::Master);
    learnSkill(ch, SkillId::Spirit, 10, SkillMastery::Master);

    // Skill and mana alone are not enough: the spell must be in the spellbook.
    CHECK_FALSE(spells.canCast(0, 1));
    CHECK(spells.getAvailableSpells(0).empty());

    ch.knownSpells[1] = true;
    CHECK(spells.canCast(0, 1));
    const auto available = spells.getAvailableSpells(0);
    CHECK(available.size() == 1);
    CHECK(available[0] == 1);
}

TEST_CASE("Buff spells add timed effects that expire", "[game][spells][buffs]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData({makeSpell(45, "Shield", 1), makeSpell(46, "Bless", 1)});

    auto& ch = world.party().member(0);
    prepareCaster(ch);
    learnSkill(ch, SkillId::Spirit, 10, SkillMastery::Master);
    ch.knownSpells[45] = true;
    ch.knownSpells[46] = true;
    ch.recalculateDerived();

    const int armorBefore = ch.armorClass;
    const int accuracyBefore = ch.stats.accuracy;

    const auto shield = spells.castBuffSpell(0, 45);
    REQUIRE(shield.success);
    CHECK(ch.armorClass > armorBefore);
    REQUIRE(ch.timedEffects.size() == 1);
    CHECK(ch.timedEffects[0].spellId == 45);

    const auto bless = spells.castBuffSpell(0, 46);
    REQUIRE(bless.success);
    CHECK(ch.stats.accuracy > accuracyBefore);
    REQUIRE(ch.timedEffects.size() == 2);

    // Expire every buff at its own deadline, then restore derived stats.
    const int64_t latestExpiry = ch.timedEffects[0].expiresAtTicks;
    ch.clearExpiredEffects(latestExpiry);
    CHECK(ch.timedEffects.empty());
    CHECK(ch.armorClass == armorBefore);
    CHECK(ch.stats.accuracy == accuracyBefore);
}

TEST_CASE("Buff spells refresh instead of stacking", "[game][spells][buffs]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData({makeSpell(45, "Shield", 1)});

    auto& ch = world.party().member(0);
    prepareCaster(ch);
    learnSkill(ch, SkillId::Spirit, 10, SkillMastery::Master);
    ch.knownSpells[45] = true;
    ch.recalculateDerived();

    REQUIRE(spells.castBuffSpell(0, 45).success);
    REQUIRE(spells.castBuffSpell(0, 45).success);
    CHECK(ch.timedEffects.size() == 1);

    const int oneShieldAc = ch.armorClass;
    CHECK(oneShieldAc >= 5);
}
