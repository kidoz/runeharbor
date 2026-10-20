// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/game/game_world.hpp"

using namespace runeharbor::game;

TEST_CASE("Condition timers damage and expire over game time", "[game][conditions]")
{
    GameWorld world;
    auto& ch = world.party().member(0);
    ch.maxHitPoints = 100;
    ch.hitPoints = 100;
    world.processConditions(); // establish the baseline tick

    SECTION("Poison2 deals two damage per in-game hour")
    {
        ch.setCondition(ConditionIndex::Poison2, world.calendar().totalTicks);
        world.calendar().advanceTicks(GameCalendar::kTicksPerHour * 3);
        world.processConditions();
        CHECK(ch.hitPoints == 94);
    }

    SECTION("Disease and poison stack their hourly damage")
    {
        ch.setCondition(ConditionIndex::Poison1, world.calendar().totalTicks);
        ch.setCondition(ConditionIndex::Disease2, world.calendar().totalTicks);
        world.calendar().advanceTicks(GameCalendar::kTicksPerHour * 2);
        world.processConditions();
        CHECK(ch.hitPoints == 94); // (1 + 2) per hour for 2 hours
    }

    SECTION("Timed conditions expire after their duration")
    {
        ch.setCondition(ConditionIndex::Afraid, world.calendar().totalTicks);
        ch.setCondition(ConditionIndex::Cursed, world.calendar().totalTicks);
        world.calendar().advanceTicks(GameCalendar::kTicksPerHour * 9);
        world.processConditions();
        CHECK_FALSE(ch.hasCondition(ConditionIndex::Afraid));
        CHECK(ch.hasCondition(ConditionIndex::Cursed)); // 24h duration
    }

    SECTION("Disabling conditions block actions")
    {
        CHECK(ch.canAct());
        ch.setCondition(ConditionIndex::Asleep);
        CHECK_FALSE(ch.canAct());
        ch.clearCondition(ConditionIndex::Asleep);
        ch.setCondition(ConditionIndex::Paralyzed);
        CHECK_FALSE(ch.canAct());
        ch.clearCondition(ConditionIndex::Paralyzed);
        CHECK(ch.canAct());
    }
}

TEST_CASE("Game over requires every member to be dead", "[game][conditions][gameover]")
{
    GameWorld world;
    CHECK_FALSE(world.isGameOver());

    for (int i = 0; i < kPartySize; ++i)
    {
        world.party().member(i).setCondition(ConditionIndex::Dead, 1);
    }
    CHECK(world.isGameOver());

    world.party().member(2).clearCondition(ConditionIndex::Dead);
    CHECK_FALSE(world.isGameOver());

    world.party().member(2).setCondition(ConditionIndex::Stoned, 1);
    CHECK(world.isGameOver());
}

