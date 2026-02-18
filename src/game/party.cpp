// SPDX-License-Identifier: MIT
#include "party.hpp"

#include <cassert>

namespace runeharbor::game
{

Party::Party()
{
    initDefault();
}

Character& Party::member(int index)
{
    assert(index >= 0 && index < kPartySize);
    return members_[static_cast<size_t>(index)];
}

const Character& Party::member(int index) const
{
    assert(index >= 0 && index < kPartySize);
    return members_[static_cast<size_t>(index)];
}

bool Party::spendGold(int amount)
{
    if (amount > gold_)
    {
        return false;
    }
    gold_ -= amount;
    return true;
}

bool Party::consumeFood(int amount)
{
    if (amount > food_)
    {
        return false;
    }
    food_ -= amount;
    return true;
}

bool Party::isPartyAlive() const
{
    for (const auto& m : members_)
    {
        if (m.isAlive())
        {
            return true;
        }
    }
    return false;
}

int Party::aliveCount() const
{
    int count = 0;
    for (const auto& m : members_)
    {
        if (m.isAlive())
        {
            count++;
        }
    }
    return count;
}

int Party::consciousCount() const
{
    int count = 0;
    for (const auto& m : members_)
    {
        if (m.isConscious())
        {
            count++;
        }
    }
    return count;
}

void Party::setWorldPosition(float x, float y, float z)
{
    worldX_ = x;
    worldY_ = y;
    worldZ_ = z;
}

void Party::setOrientation(float newYaw, float newPitch)
{
    yaw_ = newYaw;
    pitch_ = newPitch;
}

void Party::initDefault()
{
    // MM7 default party: Knight, Paladin, Archer, Cleric
    struct DefaultChar
    {
        const char* name;
        CharacterClass charClass;
        int faceId;
        Stats stats;
    };

    constexpr DefaultChar defaults[kPartySize] = {
        {"Zoltan", CharacterClass::Knight, 0, {14, 9, 7, 14, 11, 11, 7}},
        {"Roderick", CharacterClass::Paladin, 4, {11, 11, 13, 11, 9, 9, 9}},
        {"Serena", CharacterClass::Archer, 8, {9, 13, 9, 9, 13, 13, 7}},
        {"Alexis", CharacterClass::Cleric, 12, {7, 13, 14, 9, 9, 7, 14}},
    };


    for (int i = 0; i < kPartySize; i++)
    {
        auto& ch = members_[static_cast<size_t>(i)];
        ch = Character{};
        ch.name = defaults[i].name;
        ch.charClass = defaults[i].charClass;
        ch.faceId = defaults[i].faceId;
        ch.baseStats = defaults[i].stats;
        ch.gender = (i < 2) ? Gender::Male : Gender::Female;
        ch.level = 1;
        ch.experience = 0;
        ch.recalculateDerived();
        ch.hitPoints = ch.maxHitPoints;
        ch.spellPoints = ch.maxSpellPoints;
    }

    gold_ = 200;
    food_ = 7;
    alignment_ = Alignment::Neutral;
    reputation_ = 0;
    gameTime_ = 0;
}

void Party::recalculateAll()
{
    for (auto& m : members_)
    {
        m.recalculateDerived();
    }
}

} // namespace runeharbor::game
