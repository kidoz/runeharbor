// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

#include "../../src/game/party.hpp"

using namespace runeharbor::game;

TEST_CASE("Party default initialization", "[game][party]")
{
    Party party;

    SECTION("has 4 members with correct names")
    {
        REQUIRE(party.member(0).name == "Zoltan");
        REQUIRE(party.member(1).name == "Roderick");
        REQUIRE(party.member(2).name == "Serena");
        REQUIRE(party.member(3).name == "Alexis");
    }

    SECTION("has correct default classes")
    {
        REQUIRE(party.member(0).charClass == CharacterClass::Knight);
        REQUIRE(party.member(1).charClass == CharacterClass::Paladin);
        REQUIRE(party.member(2).charClass == CharacterClass::Archer);
        REQUIRE(party.member(3).charClass == CharacterClass::Cleric);
    }

    SECTION("starts with 200 gold and 7 food")
    {
        REQUIRE(party.gold() == 200);
        REQUIRE(party.food() == 7);
    }

    SECTION("all members alive and conscious")
    {
        REQUIRE(party.isPartyAlive());
        REQUIRE(party.aliveCount() == 4);
        REQUIRE(party.consciousCount() == 4);
    }

    SECTION("starts at Emerald Island")
    {
        REQUIRE(party.currentMap() == "out01.odm");
    }

    SECTION("all members at full HP")
    {
        for (int i = 0; i < kPartySize; i++)
        {
            REQUIRE(party.member(i).hitPoints == party.member(i).maxHitPoints);
            REQUIRE(party.member(i).hitPoints > 0);
        }
    }
}

TEST_CASE("Party gold management", "[game][party]")
{
    Party party;

    SECTION("addGold increases gold")
    {
        party.addGold(100);
        REQUIRE(party.gold() == 300);
    }

    SECTION("spendGold succeeds when enough")
    {
        REQUIRE(party.spendGold(100));
        REQUIRE(party.gold() == 100);
    }

    SECTION("spendGold fails when not enough")
    {
        REQUIRE_FALSE(party.spendGold(201));
        REQUIRE(party.gold() == 200); // unchanged
    }

    SECTION("setGold replaces")
    {
        party.setGold(999);
        REQUIRE(party.gold() == 999);
    }
}

TEST_CASE("Party food management", "[game][party]")
{
    Party party;

    SECTION("consumeFood succeeds when enough")
    {
        REQUIRE(party.consumeFood(3));
        REQUIRE(party.food() == 4);
    }

    SECTION("consumeFood fails when not enough")
    {
        REQUIRE_FALSE(party.consumeFood(8));
        REQUIRE(party.food() == 7);
    }

    SECTION("addFood increases food")
    {
        party.addFood(5);
        REQUIRE(party.food() == 12);
    }
}

TEST_CASE("Party alive/conscious tracking", "[game][party]")
{
    Party party;

    SECTION("killing one member reduces alive count")
    {
        party.member(0).setCondition(ConditionIndex::Dead);
        REQUIRE(party.aliveCount() == 3);
        REQUIRE(party.isPartyAlive());
    }

    SECTION("knocking one unconscious reduces conscious count but not alive")
    {
        party.member(1).setCondition(ConditionIndex::Unconscious);
        REQUIRE(party.aliveCount() == 4);
        REQUIRE(party.consciousCount() == 3);
    }

    SECTION("all dead means party not alive")
    {
        for (int i = 0; i < kPartySize; i++)
        {
            party.member(i).setCondition(ConditionIndex::Dead);
        }
        REQUIRE_FALSE(party.isPartyAlive());
        REQUIRE(party.aliveCount() == 0);
    }
}

TEST_CASE("Party experience distribution", "[game][party]")
{
    Party party;

    SECTION("XP divided among conscious members")
    {
        int xpBefore = party.member(0).experience;
        party.awardExperience(400);
        // 400 / 4 conscious = 100 each
        for (int i = 0; i < kPartySize; i++)
        {
            REQUIRE(party.member(i).experience == xpBefore + 100);
        }
    }

    SECTION("unconscious members get no XP")
    {
        party.member(0).setCondition(ConditionIndex::Unconscious);
        party.awardExperience(300);
        // 300 / 3 conscious = 100 each (unconscious gets 0)
        REQUIRE(party.member(0).experience == 0);
        REQUIRE(party.member(1).experience == 100);
    }

    SECTION("zero or negative XP does nothing")
    {
        party.awardExperience(0);
        party.awardExperience(-100);
        for (int i = 0; i < kPartySize; i++)
        {
            REQUIRE(party.member(i).experience == 0);
        }
    }

    SECTION("no conscious members means no XP")
    {
        for (int i = 0; i < kPartySize; i++)
        {
            party.member(i).setCondition(ConditionIndex::Dead);
        }
        party.awardExperience(1000); // should not crash
    }
}

TEST_CASE("Party rest", "[game][party]")
{
    Party party;
    // Damage everyone
    for (int i = 0; i < kPartySize; i++)
    {
        party.member(i).hitPoints = 1;
    }

    SECTION("8-hour rest consumes 1 food and restores HP")
    {
        REQUIRE(party.rest(8));
        REQUIRE(party.food() == 6);
        for (int i = 0; i < kPartySize; i++)
        {
            REQUIRE(party.member(i).hitPoints == party.member(i).maxHitPoints);
        }
    }

    SECTION("8-hour rest fails without food")
    {
        party.setFood(0);
        REQUIRE_FALSE(party.rest(8));
        // HP should be unchanged
        REQUIRE(party.member(0).hitPoints == 1);
    }

    SECTION("short rest does not consume food")
    {
        REQUIRE(party.rest(4));
        REQUIRE(party.food() == 7);
    }

    SECTION("rest leaves the legacy party clock to GameWorld")
    {
        // Party::gameTime_ is a compatibility-save field; GameWorld::calendar()
        // is the authoritative clock and is advanced by the rest widget/inn.
        uint64_t timeBefore = party.gameTime();
        party.rest(1);
        REQUIRE(party.gameTime() == timeBefore);
    }
}

TEST_CASE("Party position and orientation", "[game][party]")
{
    Party party;

    SECTION("setWorldPosition updates position")
    {
        party.setWorldPosition(100.0f, 200.0f, 300.0f);
        REQUIRE(party.worldX() == 100.0f);
        REQUIRE(party.worldY() == 200.0f);
        REQUIRE(party.worldZ() == 300.0f);
    }

    SECTION("setOrientation updates yaw and pitch")
    {
        party.setOrientation(1.5f, 0.3f);
        REQUIRE(party.yaw() == 1.5f);
        REQUIRE(party.pitch() == 0.3f);
    }
}

TEST_CASE("Party active member index", "[game][party]")
{
    Party party;

    SECTION("default active member is 0")
    {
        REQUIRE(party.activeMemberIndex() == 0);
    }

    SECTION("setActiveMemberIndex clamps to valid range")
    {
        party.setActiveMemberIndex(3);
        REQUIRE(party.activeMemberIndex() == 3);

        party.setActiveMemberIndex(10);
        REQUIRE(party.activeMemberIndex() == 3); // clamped

        party.setActiveMemberIndex(-1);
        REQUIRE(party.activeMemberIndex() == -1); // -1 is valid (no active)

        party.setActiveMemberIndex(-5);
        REQUIRE(party.activeMemberIndex() == -1); // clamped
    }
}

TEST_CASE("Default party spawns at the original's Emerald Island transform", "[game][party]")
{
    Party party;
    party.initDefault();

    // MM7-Rel.exe 0x460A4A-0x460A97 writes vPosition = (12552, 1816, 0) and
    // sRotationY = 512 when it seeds a new game.
    CHECK(party.worldX() == 12552.0f);
    CHECK(party.worldY() == 1816.0f);
    // Z stays at ground level so the arrival snaps the party onto the terrain;
    // 512 belongs to the rotation, not the height.
    CHECK(party.worldZ() == 0.0f);
    // Rotation is in MM7 angle units (2048 == full turn), so 512 is a quarter turn.
    CHECK(party.yaw() == 512.0f);
    CHECK(party.pitch() == 0.0f);
    CHECK(party.currentMap() == "out01.odm");
}

TEST_CASE("Party hired NPC roster", "[game][party][npc]")
{
    Party party;
    party.clearHiredNpcs();

    HiredNpc first;
    first.npcId = 10;
    first.professionId = 3;
    first.name = "Arvid";
    first.professionName = "Scout";
    first.hireCost = 100;

    HiredNpc second = first;
    second.npcId = 11;
    second.name = "Brenna";

    REQUIRE(party.hireNpc(first));
    CHECK(party.isNpcHired(10));
    CHECK_FALSE(party.isNpcHired(11));
    CHECK(party.hiredNpcs().size() == 1);
    CHECK(party.hiredNpcs()[0].name == "Arvid");

    // Duplicates and a third hireling are rejected (MM7 allows two).
    CHECK_FALSE(party.hireNpc(first));
    REQUIRE(party.hireNpc(second));
    HiredNpc third = first;
    third.npcId = 12;
    third.name = "Cedric";
    CHECK_FALSE(party.hireNpc(third));
    CHECK(party.hiredNpcs().size() == 2);

    CHECK(party.dismissNpc(10));
    CHECK_FALSE(party.isNpcHired(10));
    CHECK(party.hiredNpcs().size() == 1);
    CHECK_FALSE(party.dismissNpc(10));

    // A slot freed by dismissal can be reused.
    CHECK(party.hireNpc(third));
    CHECK(party.hiredNpcs().size() == 2);

    party.initDefault();
    CHECK(party.hiredNpcs().empty());
}
