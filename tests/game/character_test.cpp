// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

#include "../../src/game/character.hpp"

using namespace runeharbor::game;

namespace
{

Character makeKnight()
{
    Character ch;
    ch.name = "TestKnight";
    ch.charClass = CharacterClass::Knight;
    ch.baseStats = {14, 9, 7, 14, 11, 11, 7};
    ch.level = 1;
    ch.experience = 0;
    ch.recalculateDerived();
    ch.hitPoints = ch.maxHitPoints;
    ch.spellPoints = ch.maxSpellPoints;
    return ch;
}

Character makeSorcerer()
{
    Character ch;
    ch.name = "TestSorcerer";
    ch.charClass = CharacterClass::Sorcerer;
    ch.baseStats = {7, 14, 11, 9, 9, 13, 7};
    ch.level = 1;
    ch.experience = 0;
    ch.recalculateDerived();
    ch.hitPoints = ch.maxHitPoints;
    ch.spellPoints = ch.maxSpellPoints;
    return ch;
}

} // namespace

TEST_CASE("Character conditions", "[game][character]")
{
    Character ch = makeKnight();

    SECTION("default has no conditions")
    {
        REQUIRE_FALSE(ch.hasCondition(ConditionIndex::Dead));
        REQUIRE_FALSE(ch.hasCondition(ConditionIndex::Cursed));
        REQUIRE(ch.isAlive());
        REQUIRE(ch.isConscious());
    }

    SECTION("setting Dead makes isAlive false")
    {
        ch.setCondition(ConditionIndex::Dead, 100);
        REQUIRE(ch.hasCondition(ConditionIndex::Dead));
        REQUIRE_FALSE(ch.isAlive());
        REQUIRE_FALSE(ch.isConscious());
    }

    SECTION("setting Stoned makes isAlive false")
    {
        ch.setCondition(ConditionIndex::Stoned, 50);
        REQUIRE_FALSE(ch.isAlive());
    }

    SECTION("setting Eradicated makes isAlive false")
    {
        ch.setCondition(ConditionIndex::Eradicated, 1);
        REQUIRE_FALSE(ch.isAlive());
    }

    SECTION("setting Unconscious: alive but not conscious")
    {
        ch.setCondition(ConditionIndex::Unconscious, 10);
        REQUIRE(ch.isAlive());
        REQUIRE_FALSE(ch.isConscious());
    }

    SECTION("clearCondition removes condition")
    {
        ch.setCondition(ConditionIndex::Cursed, 1);
        REQUIRE(ch.hasCondition(ConditionIndex::Cursed));
        ch.clearCondition(ConditionIndex::Cursed);
        REQUIRE_FALSE(ch.hasCondition(ConditionIndex::Cursed));
    }
}

TEST_CASE("Character stats and derived values", "[game][character]")
{
    SECTION("Knight gets 12 base HP")
    {
        Character ch = makeKnight();
        // Knight classHpBase=12, endurance=14, endBonus=(14-10)/2=2
        // maxHP = 12 + (12+2)*(1-1) = 12
        REQUIRE(ch.maxHitPoints == 12);
    }

    SECTION("Sorcerer gets 6 base HP and 8 base SP")
    {
        Character ch = makeSorcerer();
        // Sorcerer classHpBase=6, endurance=9, endBonus=0
        // maxHP = 6 + (6+0)*(1-1) = 6
        REQUIRE(ch.maxHitPoints == 6);
        // classSpBase=8, intellect=14, intBonus=(14-10)/2=2
        // maxSP = 8 + (8+2)*(1-1) = 8
        REQUIRE(ch.maxSpellPoints == 8);
    }

    SECTION("Knight has 0 SP (no magic)")
    {
        Character ch = makeKnight();
        REQUIRE(ch.maxSpellPoints == 0);
    }

    SECTION("level 5 Knight HP scales correctly")
    {
        Character ch = makeKnight();
        ch.level = 5;
        ch.recalculateDerived();
        // endBonus = (14-10)/2 = 2
        // maxHP = 12 + (12+2)*4 = 12 + 56 = 68
        REQUIRE(ch.maxHitPoints == 68);
    }

    SECTION("AC derived from speed")
    {
        Character ch = makeKnight();
        // speed=11, armorClass = max(0, (11-10)/2) = 0
        REQUIRE(ch.armorClass == 0);

        ch.baseStats.speed = 20;
        ch.recalculateDerived();
        // armorClass = max(0, (20-10)/2) = 5
        REQUIRE(ch.armorClass == 5);
    }

    SECTION("effectiveStat returns base stat")
    {
        Character ch = makeKnight();
        REQUIRE(ch.effectiveStat(0) == 14); // might
        REQUIRE(ch.effectiveStat(1) == 9);  // intellect
    }

    SECTION("recalculate clamps HP/SP downward")
    {
        Character ch = makeKnight();
        ch.hitPoints = 100;
        ch.recalculateDerived();
        REQUIRE(ch.hitPoints == ch.maxHitPoints);
    }
}

TEST_CASE("Character leveling", "[game][character]")
{
    Character ch = makeKnight();

    SECTION("XP required for level 2 is 1000")
    {
        // targetLevel=2, (2*1/2)*1000 = 1000
        REQUIRE(ch.xpRequiredForNextLevel() == 1000);
    }

    SECTION("cannot level up without enough XP")
    {
        ch.experience = 999;
        REQUIRE_FALSE(ch.canLevelUp());
    }

    SECTION("can level up with enough XP")
    {
        ch.experience = 1000;
        REQUIRE(ch.canLevelUp());
    }

    SECTION("levelUp increments level")
    {
        ch.experience = 1000;
        ch.levelUp();
        REQUIRE(ch.level == 2);
    }

    SECTION("levelUp does nothing without enough XP")
    {
        ch.experience = 0;
        ch.levelUp();
        REQUIRE(ch.level == 1);
    }

    SECTION("addExperience only works when conscious")
    {
        ch.addExperience(500);
        REQUIRE(ch.experience == 500);

        ch.setCondition(ConditionIndex::Dead);
        ch.addExperience(500);
        REQUIRE(ch.experience == 500); // unchanged
    }

    SECTION("addExperience ignores non-positive")
    {
        ch.addExperience(-100);
        REQUIRE(ch.experience == 0);
        ch.addExperience(0);
        REQUIRE(ch.experience == 0);
    }
}

TEST_CASE("Character rest", "[game][character]")
{
    Character ch = makeKnight();
    ch.hitPoints = 1;
    ch.spellPoints = 0;

    SECTION("8+ hour rest fully restores HP/SP and clears Weak")
    {
        ch.setCondition(ConditionIndex::Weak, 1);
        ch.rest(8);
        REQUIRE(ch.hitPoints == ch.maxHitPoints);
        REQUIRE(ch.spellPoints == ch.maxSpellPoints);
        REQUIRE_FALSE(ch.hasCondition(ConditionIndex::Weak));
    }

    SECTION("partial rest regenerates proportionally")
    {
        ch.hitPoints = 0;
        ch.rest(4);
        // 4/8 = 0.5 fraction, 0 + int(maxHP * 0.5)
        int expected = static_cast<int>(ch.maxHitPoints * 0.5f);
        REQUIRE(ch.hitPoints == expected);
    }

    SECTION("dead characters do not rest")
    {
        ch.setCondition(ConditionIndex::Dead);
        ch.rest(8);
        REQUIRE(ch.hitPoints == 1); // unchanged
    }
}

TEST_CASE("SkillValue", "[game][character]")
{
    SkillValue sv;

    SECTION("default is not learned")
    {
        REQUIRE_FALSE(sv.learned());
        REQUIRE(sv.effective() == 0);
    }

    SECTION("learned skill returns level")
    {
        sv.mastery = SkillMastery::Expert;
        sv.level = 5;
        REQUIRE(sv.learned());
        REQUIRE(sv.effective() == 5);
    }
}

TEST_CASE("baseClassIndex", "[game][character]")
{
    REQUIRE(baseClassIndex(CharacterClass::Knight) == 0);
    REQUIRE(baseClassIndex(CharacterClass::Champion) == 0);
    REQUIRE(baseClassIndex(CharacterClass::BlackKnight) == 0);
    REQUIRE(baseClassIndex(CharacterClass::Thief) == 1);
    REQUIRE(baseClassIndex(CharacterClass::Sorcerer) == 8);
    REQUIRE(baseClassIndex(CharacterClass::Lich) == 8);
    REQUIRE(baseClassIndex(CharacterClass::Paladin) == 3);
    REQUIRE(baseClassIndex(CharacterClass::Cleric) == 6);
}
