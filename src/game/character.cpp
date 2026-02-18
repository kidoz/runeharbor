// SPDX-License-Identifier: MIT
#include "character.hpp"

#include <algorithm>

namespace runeharbor::game
{

int Character::effectiveStat(int statIndex) const
{
    return baseStats.byIndex(statIndex);
}

void Character::recalculateDerived()
{
    // Update current stats from base
    for (int i = 0; i < Stats::kCount; i++)
    {
        stats.byIndex(i) = effectiveStat(i);
    }

    // HP: base from endurance + level + class bonus
    int endBonus = std::max(0, (stats.endurance - 10) / 2);
    int classHpBase = 0;
    switch (baseClassIndex(charClass))
    {
    case 0: // Knight
        classHpBase = 12;
        break;
    case 3: // Paladin
        classHpBase = 10;
        break;
    case 4: // Archer
        classHpBase = 8;
        break;
    case 6: // Cleric
        classHpBase = 8;
        break;
    case 8: // Sorcerer
        classHpBase = 6;
        break;
    case 1: // Thief
        classHpBase = 8;
        break;
    case 2: // Monk
        classHpBase = 10;
        break;
    case 5: // Ranger
        classHpBase = 8;
        break;
    case 7: // Druid
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
    switch (baseClassIndex(charClass))
    {
    case 0: // Knight
        classSpBase = 0;
        break;
    case 3: // Paladin
        classSpBase = 4;
        break;
    case 4: // Archer
        classSpBase = 4;
        break;
    case 6: // Cleric
        classSpBase = 6;
        break;
    case 8: // Sorcerer
        classSpBase = 8;
        break;
    case 1: // Thief
        classSpBase = 0;
        break;
    case 2: // Monk
        classSpBase = 0;
        break;
    case 5: // Ranger
        classSpBase = 4;
        break;
    case 7: // Druid
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
