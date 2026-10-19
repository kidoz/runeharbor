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

TEST_CASE("Town Portal defers travel until a destination is chosen", "[game][spells][utility]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData({makeSpell(20, "Town Portal", 1)});

    auto& ch = world.party().member(0);
    prepareCaster(ch);
    learnSkill(ch, SkillId::Air, 10, SkillMastery::Normal);
    ch.knownSpells[20] = true;
    world.setCurrentMap("out01.odm");

    const int spellPointsBefore = ch.spellPoints;
    const auto result = spells.castBuffSpell(0, 20);
    REQUIRE(result.success);
    CHECK(ch.spellPoints < spellPointsBefore);
    // The cast only validates/deducts mana; InGameState hosts the menu and
    // issues the travel request once the player picks a town.
    CHECK_FALSE(world.hasTravelRequest());

    // Script casts have no menu and choose the first visited town.
    world.markVisited("out02.odm");
    const auto scripted = spells.castScriptSpell(20, 1, 0);
    REQUIRE(scripted.success);
    auto request = world.takeTravelRequest();
    REQUIRE(request.has_value());
    CHECK(request->mapName == "out02.odm");
    CHECK(request->travelDays == 0);
}

TEST_CASE("Lloyd's Beacon sets and recalls", "[game][spells][utility]")
{
    util::ConsoleLogger logger;
    GameWorld world;
    SpellSystem spells(logger);
    spells.setGameWorld(&world);
    spells.loadSpellData({makeSpell(17, "Lloyd's Beacon", 1)});

    auto& ch = world.party().member(0);
    prepareCaster(ch);
    learnSkill(ch, SkillId::Air, 10, SkillMastery::Normal);
    ch.knownSpells[17] = true;
    world.setCurrentMap("out02.odm");
    world.party().setWorldPosition(100.0f, 200.0f, 300.0f);
    world.party().setOrientation(512.0f, 0.0f);

    const auto set = spells.castBuffSpell(0, 17);
    REQUIRE(set.success);
    REQUIRE(world.hasBeacon());
    CHECK(world.beacon().mapName == "out02.odm");
    CHECK(world.beacon().x == 100.0f);
    CHECK(world.beacon().y == 200.0f);
    CHECK(world.beacon().z == 300.0f);
    CHECK(world.beacon().yaw == 512.0f);
    CHECK_FALSE(world.hasTravelRequest());

    // Moving away and casting again requests a recall, not an immediate move.
    world.party().setWorldPosition(900.0f, 800.0f, 700.0f);
    world.party().setOrientation(0.0f, 0.0f);
    const auto recall = spells.castBuffSpell(0, 17);
    REQUIRE(recall.success);
    auto request = world.takeTravelRequest();
    REQUIRE(request.has_value());
    CHECK(request->mapName == "out02.odm");
    CHECK(request->x == 100.0f);
    CHECK(request->y == 200.0f);
    CHECK(request->z == 300.0f);
    CHECK(request->yaw == 512.0f);

    // The explicit set-here path overwrites the beacon.
    const auto reset = spells.castBuffSpell(0, 17, true);
    REQUIRE(reset.success);
    CHECK(world.beacon().x == 900.0f);
    CHECK_FALSE(world.hasTravelRequest());
}

TEST_CASE("GameWorld reset clears beacon and pending travel", "[game][spells][utility]")
{
    GameWorld world;
    world.setCurrentMap("out03.odm");
    world.setBeacon(BeaconLocation{
        .set = true, .mapName = "out03.odm", .x = 1.0f, .y = 2.0f, .z = 3.0f, .yaw = 4.0f});
    world.requestTravel({"out01.odm", 0.0f, 0.0f, 0.0f, 0.0f, 0});

    CHECK(world.hasBeacon());
    CHECK(world.hasTravelRequest());

    world.reset();
    CHECK_FALSE(world.hasBeacon());
    CHECK_FALSE(world.hasTravelRequest());
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
