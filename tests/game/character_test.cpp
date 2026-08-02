// SPDX-License-Identifier: MIT
#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

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

    SECTION("takeDamage reaching 0 HP sets Unconscious")
    {
        // Regression: takeDamage centralizes HP mutation so 0 HP downs the
        // character consistently — previously the event engine's direct HP
        // write left a 0-HP character "conscious".
        REQUIRE(ch.hitPoints > 5);
        ch.takeDamage(ch.hitPoints); // drop to exactly 0
        REQUIRE(ch.hitPoints == 0);
        REQUIRE(ch.hasCondition(ConditionIndex::Unconscious));
        REQUIRE_FALSE(ch.isConscious());
    }

    SECTION("takeDamage clamps at 0 (no negative HP)")
    {
        const int max = ch.maxHitPoints;
        ch.takeDamage(max + 999);
        REQUIRE(ch.hitPoints == 0);
        REQUIRE(ch.hasCondition(ConditionIndex::Unconscious));
    }

    SECTION("heal clamps at maxHitPoints and does not clear conditions")
    {
        ch.takeDamage(ch.hitPoints); // down to 0, Unconscious
        REQUIRE(ch.hasCondition(ConditionIndex::Unconscious));
        ch.heal(9999);
        REQUIRE(ch.hitPoints == ch.maxHitPoints);
        // heal intentionally does NOT auto-clear conditions — the caller must.
        REQUIRE(ch.hasCondition(ConditionIndex::Unconscious));
    }

    SECTION("takeDamage does not override a worse condition")
    {
        ch.setCondition(ConditionIndex::Dead);
        const int hp = ch.hitPoints;
        ch.takeDamage(hp); // would normally set Unconscious
        REQUIRE(ch.hitPoints == 0);
        // Dead remains the worst condition; Unconscious was not added.
        REQUIRE(ch.hasCondition(ConditionIndex::Dead));
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

TEST_CASE("Character skill name mapping", "[game][character]")
{
    SECTION("maps creation display names to skill ids")
    {
        REQUIRE(skillIdFromName("Sword") == SkillId::Sword);
        REQUIRE(skillIdFromName("Leather Armor") == SkillId::Leather);
        REQUIRE(skillIdFromName("Spirit Magic") == SkillId::Spirit);
        REQUIRE(skillIdFromName("Body Bld") == SkillId::BodyBuilding);
        REQUIRE(skillIdFromName("Mon. Lore") == SkillId::MonsterLore);
    }

    SECTION("syncs display skills into gameplay skill levels")
    {
        Character ch;
        ch.skills = {"Mace", "Spirit Magic", "Perception"};

        syncSkillLevelsFromDisplaySkills(ch);

        REQUIRE(ch.skillLevels[static_cast<size_t>(SkillId::Mace)].learned());
        REQUIRE(ch.skillLevels[static_cast<size_t>(SkillId::Spirit)].learned());
        REQUIRE(ch.skillLevels[static_cast<size_t>(SkillId::Perception)].learned());
        REQUIRE_FALSE(ch.skillLevels[static_cast<size_t>(SkillId::Sword)].learned());
    }

    SECTION("forgetSkill clears one skill")
    {
        Character ch;
        learnSkill(ch, SkillId::Sword);
        REQUIRE(ch.skillLevels[static_cast<size_t>(SkillId::Sword)].learned());

        forgetSkill(ch, SkillId::Sword);
        REQUIRE_FALSE(ch.skillLevels[static_cast<size_t>(SkillId::Sword)].learned());
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

TEST_CASE("Face id maps to the original's race groups", "[game][character][creation]")
{
    // fcn.00490101: 0-7 -> human, 8-11 -> elf, 12-15 -> dwarf, 16-19 -> goblin.
    CHECK(faceGroupFromFaceId(0) == 0);
    CHECK(faceGroupFromFaceId(7) == 0);
    CHECK(faceGroupFromFaceId(8) == 1);
    CHECK(faceGroupFromFaceId(11) == 1);
    CHECK(faceGroupFromFaceId(12) == 2);
    CHECK(faceGroupFromFaceId(15) == 2);
    CHECK(faceGroupFromFaceId(16) == 3);
    CHECK(faceGroupFromFaceId(19) == 3);
}

TEST_CASE("Attribute base/max table matches MM7-Rel.exe 0x4ED658", "[game][character][creation]")
{
    // Stat index order: 0=Might 1=Intellect 2=Personality 3=Endurance
    //                   4=Accuracy 5=Speed 6=Luck
    SECTION("humans are uniform 11/25 apart from endurance and luck")
    {
        const int expectedBase[] = {11, 11, 11, 9, 11, 11, 9};
        for (int i = 0; i < Stats::kCount; i++)
        {
            CHECK(attributeRule(0, i).base == expectedBase[i]);
            CHECK(attributeRule(0, i).max == 25);
        }
    }

    SECTION("elves are accurate, not fast")
    {
        // The distinguishing pair: Accuracy 14/30, Speed 11/25 (not the reverse).
        CHECK(attributeRule(1, 4).base == 14);
        CHECK(attributeRule(1, 4).max == 30);
        CHECK(attributeRule(1, 5).base == 11);
        CHECK(attributeRule(1, 5).max == 25);
    }

    SECTION("dwarves (faces 12-15) are strong and tough but slow")
    {
        const int expectedBase[] = {14, 11, 11, 14, 7, 7, 9};
        const int expectedMax[] = {30, 25, 25, 30, 15, 15, 20};
        for (int i = 0; i < Stats::kCount; i++)
        {
            CHECK(attributeRule(2, i).base == expectedBase[i]);
            CHECK(attributeRule(2, i).max == expectedMax[i]);
        }
    }

    SECTION("goblins (faces 16-19) are fast, not accurate")
    {
        const int expectedBase[] = {14, 7, 7, 11, 11, 14, 9};
        const int expectedMax[] = {30, 15, 15, 25, 25, 30, 20};
        for (int i = 0; i < Stats::kCount; i++)
        {
            CHECK(attributeRule(3, i).base == expectedBase[i]);
            CHECK(attributeRule(3, i).max == expectedMax[i]);
        }
    }

    SECTION("out-of-range indices clamp rather than read past the table")
    {
        CHECK(attributeRule(-1, 0).base == attributeRule(0, 0).base);
        CHECK(attributeRule(99, 0).base == attributeRule(3, 0).base);
        CHECK(attributeRule(0, -1).base == attributeRule(0, 0).base);
        CHECK(attributeRule(0, 99).base == attributeRule(0, Stats::kCount - 1).base);
    }
}

TEST_CASE("Attribute buy/sell rates follow the original's asymmetry", "[game][character][creation]")
{
    SECTION("humans pay one point per point, both directions")
    {
        const AttributeRule& might = attributeRule(0, 0); // {11, 25, 1, 1}
        CHECK(attributeIncreaseStep(might, 11) == 1);
        CHECK(attributeIncreaseCost(might, 11) == 1);
        CHECK(attributeDecreaseStep(might, 11) == 1);
        CHECK(attributePointsSpent(might, 11) == 0);
        CHECK(attributePointsSpent(might, 15) == 4);
        CHECK(attributePointsSpent(might, 9) == -2); // refunds below base
    }

    SECTION("a favoured attribute gains +2 per point spent")
    {
        const AttributeRule& intellect = attributeRule(1, 1); // elf {14, 30, 1, 2}
        CHECK(attributeIncreaseStep(intellect, 14) == 2);
        CHECK(attributeIncreaseCost(intellect, 14) == 1);
        // 14 -> 18 is four points of stat for two points of budget.
        CHECK(attributePointsSpent(intellect, 18) == 2);
    }

    SECTION("a weak attribute costs 2 points per +1")
    {
        const AttributeRule& might = attributeRule(1, 0); // elf {7, 15, 2, 1}
        CHECK(attributeIncreaseStep(might, 7) == 1);
        CHECK(attributeIncreaseCost(might, 7) == 2);
        CHECK(attributePointsSpent(might, 9) == 4);
    }

    SECTION("below the base the rates invert, matching the decrease step")
    {
        const AttributeRule& intellect = attributeRule(1, 1); // elf {14, 30, 1, 2}
        // Dropping below base moves in 1s and refunds 2 per point.
        CHECK(attributeDecreaseStep(intellect, 14) == 1);
        CHECK(attributeIncreaseStep(intellect, 13) == 1);
        CHECK(attributeIncreaseCost(intellect, 13) == 2);
        CHECK(attributePointsSpent(intellect, 13) == -2);
        CHECK(attributePointsSpent(intellect, 12) == -4);

        const AttributeRule& might = attributeRule(1, 0); // elf {7, 15, 2, 1}
        CHECK(attributeDecreaseStep(might, 7) == 2);
        CHECK(attributePointsSpent(might, 5) == -1); // -2 stat refunds 1 point
    }

    SECTION("decrease step above the base mirrors the increase step")
    {
        const AttributeRule& intellect = attributeRule(1, 1);
        CHECK(attributeDecreaseStep(intellect, 18) == 2);
        const AttributeRule& might = attributeRule(1, 0);
        CHECK(attributeDecreaseStep(might, 9) == 1);
    }
}

TEST_CASE("A default party spends none of its 50 creation points", "[game][character][creation]")
{
    // Every member sitting at its racial base should leave the pool untouched.
    int spent = 0;
    for (int faceId : {0, 8, 12, 16})
    {
        const int group = faceGroupFromFaceId(faceId);
        for (int i = 0; i < Stats::kCount; i++)
        {
            const AttributeRule& rule = attributeRule(group, i);
            spent += attributePointsSpent(rule, rule.base);
        }
    }
    CHECK(spent == 0);
    CHECK(kCreationBonusPoints - spent == 50);
}

TEST_CASE("Class starting skills match MM7-Rel.exe 0x4ED6C8", "[game][character][creation]")
{
    // Entries of 2 in the original's [9][37] class skill table; Player::SetClass
    // (fcn.00490242) learns exactly these at level 1.
    using P = std::pair<SkillId, SkillId>;
    const P expected[] = {
        {SkillId::Sword, SkillId::Leather},   // Knight
        {SkillId::Dagger, SkillId::Stealing}, // Thief
        {SkillId::Dodging, SkillId::Unarmed}, // Monk
        {SkillId::Mace, SkillId::Spirit},     // Paladin
        {SkillId::Bow, SkillId::Air},         // Archer
        {SkillId::Axe, SkillId::Perception},  // Ranger
        {SkillId::Mace, SkillId::Body},       // Cleric
        {SkillId::Dagger, SkillId::Earth},    // Druid
        {SkillId::Staff, SkillId::Fire},      // Sorcerer
    };

    for (int i = 0; i < kBaseClassCount; i++)
    {
        const auto& actual = classStartingSkills(i);
        CHECK(actual[0] == expected[i].first);
        CHECK(actual[1] == expected[i].second);
    }
}

TEST_CASE("Every class offers exactly nine further skills", "[game][character][creation]")
{
    // The original marks nine entries with 1 per class, which is what fills the
    // 3x3 selection grid on the creation screen.
    for (int i = 0; i < kBaseClassCount; i++)
    {
        CHECK(classAvailableSkills(i).size() == 9);
    }
}

TEST_CASE("Class available skills match MM7-Rel.exe 0x4ED6C8", "[game][character][creation]")
{
    const std::vector<std::vector<SkillId>> expected = {
        // Knight
        {SkillId::Axe, SkillId::Spear, SkillId::Bow, SkillId::Mace, SkillId::Shield, SkillId::Chain,
         SkillId::BodyBuilding, SkillId::Perception, SkillId::Armsmaster},
        // Thief
        {SkillId::Sword, SkillId::Bow, SkillId::Leather, SkillId::ItemId, SkillId::Merchant,
         SkillId::Perception, SkillId::DisarmTrap, SkillId::Dodging, SkillId::Alchemy},
        // Monk
        {SkillId::Staff, SkillId::Sword, SkillId::Dagger, SkillId::Spear, SkillId::Leather,
         SkillId::BodyBuilding, SkillId::Perception, SkillId::MonsterLore, SkillId::Armsmaster},
        // Paladin
        {SkillId::Sword, SkillId::Dagger, SkillId::Axe, SkillId::Shield, SkillId::Leather,
         SkillId::Merchant, SkillId::Repair, SkillId::BodyBuilding, SkillId::Armsmaster},
        // Archer
        {SkillId::Sword, SkillId::Axe, SkillId::Spear, SkillId::Leather, SkillId::Fire,
         SkillId::Water, SkillId::Perception, SkillId::Armsmaster, SkillId::Learning},
        // Ranger
        {SkillId::Sword, SkillId::Dagger, SkillId::Bow, SkillId::Leather, SkillId::BodyBuilding,
         SkillId::DisarmTrap, SkillId::Dodging, SkillId::MonsterLore, SkillId::Armsmaster},
        // Cleric
        {SkillId::Shield, SkillId::Leather, SkillId::Spirit, SkillId::Mind, SkillId::Merchant,
         SkillId::Repair, SkillId::Meditation, SkillId::Alchemy, SkillId::Learning},
        // Druid
        {SkillId::Mace, SkillId::Leather, SkillId::Water, SkillId::Spirit, SkillId::Body,
         SkillId::Meditation, SkillId::Perception, SkillId::Alchemy, SkillId::Learning},
        // Sorcerer
        {SkillId::Dagger, SkillId::Leather, SkillId::Air, SkillId::Water, SkillId::Earth,
         SkillId::ItemId, SkillId::Merchant, SkillId::MonsterLore, SkillId::Alchemy},
    };

    for (int i = 0; i < kBaseClassCount; i++)
    {
        const auto& actual = classAvailableSkills(i);
        for (size_t j = 0; j < expected[static_cast<size_t>(i)].size(); j++)
        {
            CHECK(actual[j] == expected[static_cast<size_t>(i)][j]);
        }
    }
}

TEST_CASE("A class never offers a skill it already starts with", "[game][character][creation]")
{
    for (int i = 0; i < kBaseClassCount; i++)
    {
        const auto& start = classStartingSkills(i);
        const auto& avail = classAvailableSkills(i);
        for (const SkillId s : start)
        {
            CHECK(std::find(avail.begin(), avail.end(), s) == avail.end());
        }
    }
}

TEST_CASE("Skill display names round-trip through skillIdFromName", "[game][character]")
{
    // The starting-equipment mapping keys off SkillId, but the creation UI
    // stores display names, so the two must agree for every skill.
    for (int i = 0; i < static_cast<int>(SkillId::Count); i++)
    {
        const auto id = static_cast<SkillId>(i);
        const std::string_view name = skillDisplayName(id);
        REQUIRE_FALSE(name.empty());
        const auto resolved = skillIdFromName(name);
        REQUIRE(resolved.has_value());
        CHECK(*resolved == id);
    }
}
