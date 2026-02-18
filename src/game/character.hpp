// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace runeharbor::game
{

// MM7 character races
enum class Race : uint8_t
{
    Human = 0,
    Elf,
    Dwarf,
    Goblin,
    Count
};

enum class Gender : uint8_t
{
    Male = 0,
    Female,
    Count
};

// MM7 base character classes (9 base classes, index 0-8)
// Promoted classes (Cavalier, Champion, etc.) will be added when class promotion is implemented.
enum class CharacterClass : uint8_t
{
    Knight = 0,
    Paladin,
    Archer,
    Cleric,
    Sorcerer,
    Thief,
    Monk,
    Ranger,
    Druid,
    Count
};

// MM7 skills
enum class SkillId : uint8_t
{
    Staff = 0,
    Sword,
    Dagger,
    Axe,
    Spear,
    Bow,
    Mace,
    Blaster,
    Shield,
    Leather,
    Chain,
    Plate,
    Fire,
    Air,
    Water,
    Earth,
    Spirit,
    Mind,
    Body,
    Light,
    Dark,
    ItemId,
    Merchant,
    Repair,
    BodyBuilding,
    Meditation,
    Perception,
    Diplomacy,
    Thievery,
    DisarmTrap,
    Dodging,
    Unarmed,
    MonsterLore,
    Armsmaster,
    Stealing,
    Alchemy,
    Learning,
    Count
};

// Skill mastery levels
enum class SkillMastery : uint8_t
{
    None = 0,
    Normal = 1,
    Expert = 2,
    Master = 3,
    GrandMaster = 4
};

// Packed skill value: mastery in high bits, level in low bits
struct SkillValue
{
    uint8_t level = 0;
    SkillMastery mastery = SkillMastery::None;

    bool learned() const { return mastery != SkillMastery::None; }
    int effective() const { return learned() ? level : 0; }
};

// Character conditions (bitfield)
enum class Condition : uint16_t
{
    None = 0,
    Cursed = 1 << 0,
    Weak = 1 << 1,
    Asleep = 1 << 2,
    Afraid = 1 << 3,
    Drunk = 1 << 4,
    Insane = 1 << 5,
    Poisoned1 = 1 << 6,
    Diseased1 = 1 << 7,
    Poisoned2 = 1 << 8,
    Diseased2 = 1 << 9,
    Poisoned3 = 1 << 10,
    Diseased3 = 1 << 11,
    Paralyzed = 1 << 12,
    Unconscious = 1 << 13,
    Dead = 1 << 14,
    Stoned = 1 << 15,
};

// Equipment slots
enum class EquipSlot : uint8_t
{
    MainHand = 0,
    OffHand,
    Bow,
    Armor,
    Helmet,
    Belt,
    Cloak,
    Gauntlets,
    Boots,
    Amulet,
    Ring1,
    Ring2,
    Ring3,
    Ring4,
    Ring5,
    Ring6,
    Count
};

// Lightweight item reference (full item system is separate)
struct ItemSlot
{
    int itemId = 0;
    bool occupied() const { return itemId != 0; }
};

// The 7 primary stats
struct Stats
{
    int might = 11;
    int intellect = 11;
    int personality = 11;
    int endurance = 11;
    int speed = 11;
    int accuracy = 11;
    int luck = 11;

    static constexpr int kCount = 7;

    int& byIndex(int i)
    {
        switch (i)
        {
        case 1:
            return intellect;
        case 2:
            return personality;
        case 3:
            return endurance;
        case 4:
            return speed;
        case 5:
            return accuracy;
        case 6:
            return luck;
        default:
            return might;
        }
    }

    int byIndex(int i) const
    {
        switch (i)
        {
        case 0:
            return might;
        case 1:
            return intellect;
        case 2:
            return personality;
        case 3:
            return endurance;
        case 4:
            return speed;
        case 5:
            return accuracy;
        case 6:
            return luck;
        default:
            return 0;
        }
    }
};

struct Character
{
    std::string name = "New Hero";
    int faceId = 0;
    CharacterClass charClass = CharacterClass::Knight;
    Race race = Race::Human;
    Gender gender = Gender::Male;

    // Primary attributes
    Stats stats;
    Stats baseStats;

    // Derived attributes
    int level = 1;
    int experience = 0;
    int hitPoints = 0;
    int maxHitPoints = 0;
    int spellPoints = 0;
    int maxSpellPoints = 0;
    int armorClass = 0;
    int age = 20;

    // Resistances
    int fireResistance = 0;
    int airResistance = 0;
    int waterResistance = 0;
    int earthResistance = 0;
    int mindResistance = 0;
    int bodyResistance = 0;
    int spiritResistance = 0;

    // Skills
    std::array<SkillValue, static_cast<size_t>(SkillId::Count)> skillLevels = {};
    std::vector<std::string> skills; // Display names (for character creation UI)

    // Equipment
    std::array<ItemSlot, static_cast<size_t>(EquipSlot::Count)> equipment = {};

    // Conditions
    uint16_t conditions = 0;

    bool hasCondition(Condition c) const { return (conditions & static_cast<uint16_t>(c)) != 0; }

    void setCondition(Condition c) { conditions |= static_cast<uint16_t>(c); }
    void clearCondition(Condition c) { conditions &= ~static_cast<uint16_t>(c); }

    bool isAlive() const
    {
        return !hasCondition(Condition::Dead) && !hasCondition(Condition::Stoned);
    }

    bool isConscious() const { return isAlive() && !hasCondition(Condition::Unconscious); }

    // Stat bonuses from race
    static Stats racialBonuses(Race race);

    // Calculate effective stat (base + bonuses)
    int effectiveStat(int statIndex) const;

    // Recalculate derived stats (HP, SP, AC, resistances) from base stats + equipment
    void recalculateDerived();
};

} // namespace runeharbor::game
