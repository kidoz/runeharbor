// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::game
{

class GameWorld;
class CombatSystem;
class SpellSystem;
class Inventory;

// MM7 event command opcodes (from binary EVT files, matches actual bytecode values)
enum class EventOpcode : uint8_t
{
    NoOp = 0,
    Exit = 1,
    NpcDialog = 2,
    PlaySound = 3,
    SkipNext = 4,
    SkipNext2 = 5,
    Teleport = 6,
    GiveGold = 7,
    SetPlayerVar = 8,
    GiveItem = 9,
    SetFlag = 10,
    CheckFlag = 11,
    ChangeMap = 12,
    ModifyObject = 13,
    CheckCondition = 14,
    DoorControl = 15,
    AddStat = 16,
    RemoveItem = 17,
    SubtractStat = 18,
    ModifyNpc = 19,
    Unused20 = 20,
    ModifyNpcEx = 21,
    ShowBuilding = 22,
    ShowEffect = 23,
    PlayAnimation = 24,
    RandomGoto = 25,
    ShowText = 26,
    Unused27 = 27,
    Unused28 = 28,
    SetNpcPortrait = 29,
    SetNpcName = 30,
    TriggerTimerAbsolute = 31,
    GiveAward = 32,
    StatusMessage = 33,
    SpawnItem = 34,
    SetPlayerSelect = 35,
    JumpToEvent = 36,
    TriggerOnMapEnter = 37,
    TriggerTimerPeriodic = 38,
    SetGlobalVar = 39,
    SetGlobalVar2 = 40,
    CastSpell = 41,
    ModifyDecoration = 42,
    CheckSkill = 43,
    Unused44 = 44,
    Unused45 = 45,
    Unused46 = 46,
    SetMonsterTopic = 47,
    SetMonsterField = 48,
    SetMonsterHostile = 49,
    SetMonsterGroup = 50,
    CheckMapVar = 51,
    Unused52 = 52,
    TriggerOnMapLoad = 53, // Special marker used by map-load trigger table
    ReplaceMonster = 54,
    SetMonsterAi = 55,
    CheckTime = 56,
    GiveExperience = 57,
    TakeGold = 58,
    CureCondition = 59,
    SetHostileByIdx = 60,
};

// A single command in an event script
struct EventCommand
{
    EventOpcode opcode = EventOpcode::Exit;
    int param1 = 0;       // First integer parameter
    int param2 = 0;       // Second integer parameter
    int param3 = 0;       // Third integer parameter
    int param4 = 0;       // Fourth integer parameter
    int param5 = 0;       // Fifth integer parameter
    int param6 = 0;       // Sixth integer parameter
    std::string text;     // String parameter (for ShowText, Teleport map name, etc.)
    float fparam = 0;     // Float parameter (for positions)
    float fparam2 = 0;    // Second float parameter
    float fparam3 = 0;    // Third float parameter
    int64_t i64param = 0; // Extended payload (timer interval ticks)
};

// A complete event script (sequence of commands)
struct EventScript
{
    int eventId = 0;
    std::vector<EventCommand> commands;
};

// Event trigger types (what causes an event to fire)
enum class TriggerType : uint8_t
{
    OnEnter = 0, // Party enters area
    OnClick = 1, // Player clicks object
    OnTimer = 2, // Timer expires
    OnTalk = 3,  // Talk to NPC
    OnStep = 4,  // Step on floor trigger
};

// Callback for UI effects (text display, sound, etc.)
struct EventCallbacks
{
    std::function<void(const std::string& text)> onShowText;
    std::function<void(int dialogTextId)> onNpcDialog;
    std::function<void(int buildingId)> onShowBuilding;
    std::function<void(int soundId)> onPlaySound;
    std::function<void(const std::string& map, float x, float y, float z, float yaw)> onTeleport;
    std::function<void(const std::string& map, int exitDirection, int transitionParam)> onChangeMap;
    std::function<void(int itemId)> onGiveItem;
    std::function<bool(int itemId)> onRemoveItem;
    std::function<void(const EventCommand& command)> onMapCommand;
    // Fired when a global/quest variable is set (SetGlobalVar/SetGlobalVar2).
    // The host bridges quest-bit writes into the QuestLog (RE: setting a quest
    // bit acquires the quest; the 0->nonzero transition is "New Quest!").
    std::function<void(int varIndex, int field, int value)> onSetGlobalVar;
};

class EventEngine
{
  public:
    explicit EventEngine(util::ILogger& logger);

    // Set the game world reference for executing commands
    void setGameWorld(GameWorld* world) { gameWorld_ = world; }
    void setCombatSystem(CombatSystem* combatSystem) { combatSystem_ = combatSystem; }
    void setSpellSystem(SpellSystem* spellSystem) { spellSystem_ = spellSystem; }
    void setInventory(Inventory* inventory) { inventory_ = inventory; }

    // Set callbacks for UI-facing effects
    void setCallbacks(const EventCallbacks& callbacks) { callbacks_ = callbacks; }

    // Register an event script
    void registerEvent(int eventId, EventScript script);

    // Load event scripts from parsed data
    void loadEvents(const std::vector<EventScript>& scripts);

    // Mark scripts that belong to the current map scope.
    void setMapScopedEvents(const std::vector<EventScript>& scripts);

    // Trigger an event by ID.
    // contextFlag mirrors MM7 param_3 behavior (0 = deferred/non-interactive, 1 = interactive).
    bool triggerEvent(int eventId, int contextFlag = 1);

    // Trigger all events whose first command opcode matches `opcode`.
    // Returns number of events that were executed.
    int triggerEventsByFirstOpcode(uint8_t opcode, bool mapOnly = false);

    // Rebuild map trigger state (on-enter and timers) for currently scoped map events.
    void onMapLoaded();

    // Process scheduled timer triggers against current game clock.
    int updateRuntimeTriggers();

    // Check if an event exists
    bool hasEvent(int eventId) const;

    // Clear all registered events
    void clear();

    // Persist/restore runtime trigger state (timer schedule, last runtime tick).
    std::vector<uint8_t> serializeRuntimeState() const;
    bool deserializeRuntimeState(const std::vector<uint8_t>& data);

  private:
    size_t executeCommand(const EventScript& script, size_t index, bool& shouldExit);
    std::vector<int> resolvePlayerTargets(uint8_t mode) const;
    void applyStatDelta(int statId, int delta, uint8_t mode);
    const EventScript* resolveEventScript(int eventId) const;

    struct TimerTrigger
    {
        int eventId = 0;
        int64_t nextTick = 0;
        int64_t intervalTicks = 0;
        bool periodic = false;
        bool active = false;
    };

    util::ILogger& logger_;
    GameWorld* gameWorld_ = nullptr;
    CombatSystem* combatSystem_ = nullptr;
    SpellSystem* spellSystem_ = nullptr;
    Inventory* inventory_ = nullptr;
    EventCallbacks callbacks_;
    std::unordered_map<int, EventScript> events_;
    std::unordered_map<int, EventScript> mapScopedEvents_;
    std::vector<TimerTrigger> timerTriggers_;
    // One-shot map events (OnMapLoad / OnMapEnter) that have already fired for
    // the current map scope. Persisted across save/load so reloading a save
    // doesn't re-grant gold/XP/items from OnMapLoad events. Cleared on each
    // onMapLoaded() only when the map scope itself changes.
    std::unordered_set<int> firedOneShotEvents_;
    int64_t lastRuntimeTick_ = -1;
    int executionContext_ = 1;
    uint8_t playerSelectMode_ = 4; // 0..3 specific, 4 active, 5 all, 6 random
    mutable std::mt19937 rng_{0xE7715EEDu};
};

} // namespace runeharbor::game
