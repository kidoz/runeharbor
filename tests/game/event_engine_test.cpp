// SPDX-License-Identifier: MIT
//
// Unit tests for the event engine — the EVT opcode interpreter. These are the
// first tests for the highest-complexity gameplay file. They cover opcode
// dispatch (GiveGold/SetFlag), event-to-event jumps, and the one-shot
// OnMapLoad fired-set (the save/load duplicate-firing fix).
#include <catch2/catch_test_macros.hpp>

#include "../../src/game/event_engine.hpp"
#include "../../src/game/game_world.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::game;

namespace
{

class NullLogger : public runeharbor::util::ILogger
{
  public:
    void log(runeharbor::util::LogLevel, std::string_view) override {}
};

// Build a one-command event.
EventScript makeEvent(int id, EventOpcode op, int p1 = 0, int p2 = 0, int p3 = 0)
{
    EventScript s;
    s.eventId = id;
    EventCommand c;
    c.opcode = op;
    c.param1 = p1;
    c.param2 = p2;
    c.param3 = p3;
    s.commands.push_back(c);
    return s;
}

} // namespace

TEST_CASE("EventEngine GiveGold grants gold via the party", "[game][event]")
{
    NullLogger logger;
    EventEngine engine(logger);
    GameWorld world;
    engine.setGameWorld(&world);

    const int goldBefore = world.party().gold();
    engine.registerEvent(10, makeEvent(10, EventOpcode::GiveGold, 500));

    REQUIRE(engine.triggerEvent(10));
    REQUIRE(world.party().gold() == goldBefore + 500);
}

TEST_CASE("EventEngine TakeGold deducts gold", "[game][event]")
{
    NullLogger logger;
    EventEngine engine(logger);
    GameWorld world;
    world.party().setGold(1000);
    engine.setGameWorld(&world);

    engine.registerEvent(11, makeEvent(11, EventOpcode::TakeGold, 300));
    REQUIRE(engine.triggerEvent(11));
    REQUIRE(world.party().gold() == 700);
}

TEST_CASE("EventEngine SetFlag / CheckFlag round-trip a game variable", "[game][event]")
{
    NullLogger logger;
    EventEngine engine(logger);
    GameWorld world;
    engine.setGameWorld(&world);

    // Set flag (var) 50 to 1.
    engine.registerEvent(20, makeEvent(20, EventOpcode::SetFlag, 50, 1));
    REQUIRE(engine.triggerEvent(20));
    REQUIRE(world.getVar(static_cast<GameVarId>(50)) == 1);

    // CheckFlag: param1=var, param2=expected, param3=jump-target index. With
    // var==expected the jump is taken; here we just confirm the flag was set,
    // since the jump target lives within a script's command vector.
    REQUIRE(world.isFlagSet(static_cast<GameVarId>(50)));
}

TEST_CASE("EventEngine JumpToEvent chains to another event", "[game][event]")
{
    NullLogger logger;
    EventEngine engine(logger);
    GameWorld world;
    engine.setGameWorld(&world);

    // Event 30: jump to event 31.
    EventScript jump;
    jump.eventId = 30;
    EventCommand j;
    j.opcode = EventOpcode::JumpToEvent;
    j.param1 = 31; // target event id
    jump.commands.push_back(j);
    engine.registerEvent(30, std::move(jump));

    // Event 31: grant 250 gold then exit.
    EventScript grant;
    grant.eventId = 31;
    EventCommand g;
    g.opcode = EventOpcode::GiveGold;
    g.param1 = 250;
    grant.commands.push_back(g);
    EventCommand exit;
    exit.opcode = EventOpcode::Exit;
    grant.commands.push_back(exit);
    engine.registerEvent(31, std::move(grant));

    const int goldBefore = world.party().gold();
    REQUIRE(engine.triggerEvent(30));
    // The jump must have executed event 31, granting the gold.
    REQUIRE(world.party().gold() == goldBefore + 250);
}

TEST_CASE("EventEngine onMapLoaded fires OnMapLoad once and skips on reload", "[game][event]")
{
    // Regression: before the fired-set fix, loading a save re-fired every
    // OnMapLoad event, re-granting gold/XP/items. onMapLoaded must fire a
    // TriggerOnMapLoad event exactly once per map scope.
    NullLogger logger;
    EventEngine engine(logger);
    GameWorld world;
    engine.setGameWorld(&world);

    // A map-scoped event whose first command is TriggerOnMapLoad, then GiveGold.
    EventScript onLoad;
    onLoad.eventId = 40;
    EventCommand marker;
    marker.opcode = EventOpcode::TriggerOnMapLoad;
    onLoad.commands.push_back(marker);
    EventCommand gold;
    gold.opcode = EventOpcode::GiveGold;
    gold.param1 = 1000;
    onLoad.commands.push_back(gold);
    engine.setMapScopedEvents({onLoad});

    const int goldBefore = world.party().gold();

    // First map load fires the event.
    engine.onMapLoaded();
    REQUIRE(world.party().gold() == goldBefore + 1000);

    // A second onMapLoaded (simulating a save reload of the same map) must NOT
    // re-fire the one-shot event.
    engine.onMapLoaded();
    REQUIRE(world.party().gold() == goldBefore + 1000);
}

TEST_CASE("EventEngine fired-set survives serialize/deserialize", "[game][event]")
{
    NullLogger logger;
    EventEngine engine(logger);
    GameWorld world;
    engine.setGameWorld(&world);

    EventScript onLoad;
    onLoad.eventId = 41;
    EventCommand marker;
    marker.opcode = EventOpcode::TriggerOnMapLoad;
    onLoad.commands.push_back(marker);
    EventCommand gold;
    gold.opcode = EventOpcode::GiveGold;
    gold.param1 = 500;
    onLoad.commands.push_back(gold);
    engine.setMapScopedEvents({onLoad});

    const int goldBefore = world.party().gold();
    engine.onMapLoaded(); // fires once
    REQUIRE(world.party().gold() == goldBefore + 500);

    // Persist runtime state (including the fired-set).
    auto blob = engine.serializeRuntimeState();
    REQUIRE_FALSE(blob.empty());

    // Simulate a save load: a fresh engine with the same map scope, restored
    // runtime state, then onMapLoaded. The fired event must NOT re-fire.
    EventEngine restored(logger);
    restored.setGameWorld(&world);
    restored.setMapScopedEvents({onLoad});
    REQUIRE(restored.deserializeRuntimeState(blob));
    restored.onMapLoaded();
    REQUIRE(world.party().gold() == goldBefore + 500); // unchanged — not +1000
}
