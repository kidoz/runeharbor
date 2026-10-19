// SPDX-License-Identifier: MIT
//
// Party-wide utility spell effects: Light (night-blend reduction), Fly and
// Water Walk (timed movement flags). These bypass per-character TimedEffects
// and live on GameWorld so physics and rendering can query them.
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

TEST_CASE("Utility effects are timed party state", "[game][spells][utility]")
{
    GameWorld world;
    const int64_t now = world.calendar().totalTicks;

    world.setUtilityEffect(UtilityEffect::Light, 2, now + 100);
    CHECK(world.hasUtilityEffect(UtilityEffect::Light));
    CHECK(world.utilityEffectStrength(UtilityEffect::Light) == 2);

    world.calendar().advanceTicks(100);
    world.clearExpiredUtilityEffects();
    CHECK_FALSE(world.hasUtilityEffect(UtilityEffect::Light));
    CHECK(world.utilityEffectStrength(UtilityEffect::Light) == 0);

    // No expiry means active until removed.
    world.setUtilityEffect(UtilityEffect::Fly, 1, 0);
    world.calendar().advanceTicks(GameCalendar::kTicksPerYear);
    CHECK(world.hasUtilityEffect(UtilityEffect::Fly));
}

TEST_CASE("Light reduces the effective night blend", "[game][spells][utility]")
{
    GameWorld world;
    world.calendar().totalTicks = 22 * GameCalendar::kTicksPerHour;
    REQUIRE(world.calendar().nightBlend() == 1.0f);

    CHECK(world.lightNightReduction() == 0.0f);
    CHECK(world.effectiveNightBlend() == 1.0f);

    world.setUtilityEffect(UtilityEffect::Light, 2, 0);
    CHECK(world.lightNightReduction() == 0.5f);
    CHECK_THAT(world.effectiveNightBlend(), Catch::Matchers::WithinAbs(0.5f, 0.0001f));

    // Grandmaster strength caps at 90% reduction.
    world.setUtilityEffect(UtilityEffect::Light, 4, 0);
    CHECK(world.lightNightReduction() == 0.9f);
    CHECK_THAT(world.effectiveNightBlend(), Catch::Matchers::WithinAbs(0.1f, 0.0001f));
}

TEST_CASE("Utility spells cast as party-wide effects", "[game][spells][utility]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData(
        {makeSpell(1, "Torch Light", 1), makeSpell(16, "Fly", 1), makeSpell(25, "Water Walk", 1)});

    // Utility spells must not be classified as enemy-targeted damage spells.
    REQUIRE(spells.getSpell(1) != nullptr);
    REQUIRE(spells.getSpell(16) != nullptr);
    REQUIRE(spells.getSpell(25) != nullptr);
    CHECK(spells.getSpell(1)->target == SpellTarget::AllAllies);
    CHECK(spells.getSpell(16)->target == SpellTarget::AllAllies);
    CHECK(spells.getSpell(25)->target == SpellTarget::AllAllies);

    auto& ch = world.party().member(0);
    prepareCaster(ch);
    learnSkill(ch, SkillId::Fire, 10, SkillMastery::GrandMaster);
    learnSkill(ch, SkillId::Air, 10, SkillMastery::Normal);
    learnSkill(ch, SkillId::Water, 10, SkillMastery::Normal);
    ch.knownSpells[1] = true;
    ch.knownSpells[16] = true;
    ch.knownSpells[25] = true;

    const auto light = spells.castBuffSpell(0, 1);
    REQUIRE(light.success);
    CHECK(world.hasUtilityEffect(UtilityEffect::Light));
    CHECK(world.utilityEffectStrength(UtilityEffect::Light) == 4);
    CHECK(ch.timedEffects.empty()); // party effect, not a character buff

    REQUIRE(spells.castBuffSpell(0, 16).success);
    CHECK(world.hasUtilityEffect(UtilityEffect::Fly));

    REQUIRE(spells.castBuffSpell(0, 25).success);
    CHECK(world.hasUtilityEffect(UtilityEffect::WaterWalk));

    // Recasting refreshes the deadline instead of stacking.
    const int64_t firstExpiry = world.utilityEffect(UtilityEffect::Fly).expiresAtTicks;
    world.calendar().advanceTicks(GameCalendar::kTicksPerHour);
    REQUIRE(spells.castBuffSpell(0, 16).success);
    CHECK(world.utilityEffect(UtilityEffect::Fly).expiresAtTicks > firstExpiry);
}

TEST_CASE("Script casts apply utility spells to the party", "[game][spells][utility]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData({makeSpell(16, "Fly", 1)});

    auto& ch = world.party().member(0);
    ch.maxHitPoints = 100;
    ch.hitPoints = 100;

    const auto result = spells.castScriptSpell(16, 2, 0);
    REQUIRE(result.success);
    CHECK(result.damage == 0);
    CHECK(ch.hitPoints == 100);
    CHECK(world.hasUtilityEffect(UtilityEffect::Fly));

    const int64_t expiry = world.utilityEffect(UtilityEffect::Fly).expiresAtTicks;
    CHECK(expiry == world.calendar().totalTicks + 2 * GameCalendar::kTicksPerHour);
}
