// SPDX-License-Identifier: MIT
#include "character.hpp"

#include <algorithm>

namespace runeharbor::game
{

Stats Character::racialBonuses(Race race)
{
    // MM7 racial stat bonuses
    Stats bonus = {0, 0, 0, 0, 0, 0, 0};
    switch (race)
    {
    case Race::Human:
        // Humans are balanced, no bonuses
        break;
    case Race::Elf:
        bonus.intellect = 2;
        bonus.speed = 2;
        bonus.accuracy = 2;
        bonus.endurance = -2;
        bonus.might = -2;
        break;
    case Race::Dwarf:
        bonus.might = 2;
        bonus.endurance = 2;
        bonus.speed = -2;
        bonus.accuracy = -2;
        break;
    case Race::Goblin:
        bonus.speed = 2;
        bonus.accuracy = 2;
        bonus.might = -2;
        bonus.personality = -2;
        break;
    default:
        break;
    }
    return bonus;
}

int Character::effectiveStat(int statIndex) const
{
    int base = baseStats.byIndex(statIndex);
    Stats racial = racialBonuses(race);
    return base + racial.byIndex(statIndex);
}

void Character::recalculateDerived()
{
    // Update current stats from base + racial
    for (int i = 0; i < Stats::kCount; i++)
    {
        stats.byIndex(i) = effectiveStat(i);
    }

    // HP: base from endurance + level + class bonus
    int endBonus = std::max(0, (stats.endurance - 10) / 2);
    int classHpBase = 0;
    switch (static_cast<int>(charClass))
    {
    case 0: // Knight
        classHpBase = 12;
        break;
    case 1: // Paladin
        classHpBase = 10;
        break;
    case 2: // Archer
        classHpBase = 8;
        break;
    case 3: // Cleric
        classHpBase = 8;
        break;
    case 4: // Sorcerer
        classHpBase = 6;
        break;
    case 5: // Thief
        classHpBase = 8;
        break;
    case 6: // Monk
        classHpBase = 10;
        break;
    case 7: // Ranger
        classHpBase = 8;
        break;
    case 8: // Druid
        classHpBase = 6;
        break;
    default:
        classHpBase = 8;
        break;
    }
    maxHitPoints = classHpBase + (classHpBase + endBonus) * (level - 1);
    hitPoints = std::min(hitPoints, maxHitPoints);

    // SP: base from intellect/personality + level + class bonus
    int intBonus = std::max(0, (stats.intellect - 10) / 2);
    int perBonus = std::max(0, (stats.personality - 10) / 2);
    int classSpBase = 0;
    switch (static_cast<int>(charClass))
    {
    case 0: // Knight
        classSpBase = 0;
        break;
    case 1: // Paladin
        classSpBase = 4;
        break;
    case 2: // Archer
        classSpBase = 4;
        break;
    case 3: // Cleric
        classSpBase = 6;
        break;
    case 4: // Sorcerer
        classSpBase = 8;
        break;
    case 5: // Thief
        classSpBase = 0;
        break;
    case 6: // Monk
        classSpBase = 0;
        break;
    case 7: // Ranger
        classSpBase = 4;
        break;
    case 8: // Druid
        classSpBase = 6;
        break;
    default:
        classSpBase = 0;
        break;
    }
    int spBonus = std::max(intBonus, perBonus); // Use whichever is higher
    maxSpellPoints = classSpBase + (classSpBase > 0 ? (classSpBase + spBonus) * (level - 1) : 0);
    spellPoints = std::min(spellPoints, maxSpellPoints);

    // AC: base from speed
    armorClass = std::max(0, (stats.speed - 10) / 2);
}

} // namespace runeharbor::game
