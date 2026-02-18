// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::game
{

class GameWorld;

// MM7 event command opcodes (from binary EVT files, matches actual bytecode values)
enum class EventOpcode : uint8_t
{
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
    ModifyNpcEx = 21,
    ShowBuilding = 22,
    ShowEffect = 23,
    PlayAnimation = 24,
    RandomGoto = 25,
    ShowText = 26,
    SetNpcPortrait = 29,
    SetNpcName = 30,
    GiveAward = 32,
    StatusMessage = 33,
    SpawnItem = 34,
    SetPlayerSelect = 35,
    JumpToEvent = 36,
    SetGlobalVar = 39,
    SetGlobalVar2 = 40,
    CastSpell = 41,
    ModifyDecoration = 42,
    CheckSkill = 43,
    SetMonsterTopic = 47,
    SetMonsterField = 48,
    SetMonsterHostile = 49,
    SetMonsterGroup = 50,
    CheckMapVar = 51,
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
    int param1 = 0;    // First integer parameter
    int param2 = 0;    // Second integer parameter
    int param3 = 0;    // Third integer parameter
    std::string text;  // String parameter (for ShowText, Teleport map name, etc.)
    float fparam = 0;  // Float parameter (for positions)
    float fparam2 = 0; // Second float parameter
    float fparam3 = 0; // Third float parameter
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
    std::function<void(int soundId)> onPlaySound;
    std::function<void(const std::string& map, float x, float y, float z, float yaw)> onTeleport;
};

class EventEngine
{
  public:
    explicit EventEngine(util::ILogger& logger);

    // Set the game world reference for executing commands
    void setGameWorld(GameWorld* world) { gameWorld_ = world; }

    // Set callbacks for UI-facing effects
    void setCallbacks(const EventCallbacks& callbacks) { callbacks_ = callbacks; }

    // Register an event script
    void registerEvent(int eventId, EventScript script);

    // Load event scripts from parsed data
    void loadEvents(const std::vector<EventScript>& scripts);

    // Trigger an event by ID
    bool triggerEvent(int eventId);

    // Check if an event exists
    bool hasEvent(int eventId) const;

    // Clear all registered events
    void clear();

  private:
    void executeCommand(const EventCommand& cmd);

    util::ILogger& logger_;
    GameWorld* gameWorld_ = nullptr;
    EventCallbacks callbacks_;
    std::unordered_map<int, EventScript> events_;
};

} // namespace runeharbor::game
