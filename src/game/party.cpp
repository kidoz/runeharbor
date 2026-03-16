// SPDX-License-Identifier: MIT
#include "party.hpp"

#include <algorithm>

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

void Party::setActiveMemberIndex(int index)
{
    activeMemberIndex_ = std::clamp(index, -1, kPartySize - 1);
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
    activeMemberIndex_ = 0;

    // Emerald Island starting position (MM7 default new-game spawn)
    worldX_ = 12552.0f;
    worldY_ = 1816.0f;
    worldZ_ = 512.0f;
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    currentMap_ = "out01.odm";
}

void Party::recalculateAll()
{
    for (auto& m : members_)
    {
        m.recalculateDerived();
    }
}

void Party::awardExperience(int amount)
{
    if (amount <= 0)
    {
        return;
    }

    // Distribute evenly among conscious members? Or give all amount to each?
    // In MM7 usually it's divided by conscious members if it's monster XP,
    // but quest XP is often awarded fully to each. Let's just give to all conscious for now.
    int count = consciousCount();
    if (count == 0)
    {
        return;
    }

    int share = amount / count;
    for (auto& m : members_)
    {
        m.addExperience(share);
    }
}

bool Party::rest(int hours)
{
    // Need food for resting >= 8 hours
    if (hours >= 8)
    {
        // 1 food unit per rest period
        if (food_ < 1)
        {
            return false;
        }
        consumeFood(1);
    }

    for (auto& m : members_)
    {
        m.rest(hours);
    }

    // 128 ticks per real second. Let's assume 1 hour game time = 3600 real seconds?
    // Actually from docs: "128 Ticks per second (time multiplier)", "0xE10 (3600) Seconds per hour"
    // Game time advances. We just add roughly 3600 seconds per hour to game time?
    // Wait, the docs say: 3600 seconds per hour. So 3600 * 128 ticks per hour?
    uint64_t ticksPerHour = 3600ULL * 128ULL;
    advanceTime(hours * ticksPerHour);

    return true;
}

} // namespace runeharbor::game
