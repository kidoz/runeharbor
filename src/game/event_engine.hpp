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

// MM7 event command opcodes (from binary EVT files)
enum class EventOpcode : uint8_t
{
    End = 0,           // End of event script
    Nop = 1,           // No operation
    SetVariable = 2,   // Set a game variable
    CheckVariable = 3, // Branch if variable matches
    GiveItem = 4,      // Give item to party
    TakeItem = 5,      // Remove item from party
    GiveGold = 6,      // Add gold
    TakeGold = 7,      // Remove gold
    GiveFood = 8,      // Add food
    TakeFood = 9,      // Remove food
    GiveXP = 10,       // Give experience to party
    ShowText = 11,     // Display text message
    ShowNPC = 12,      // Show NPC dialogue
    Teleport = 13,     // Move party to map/position
    PlaySound = 14,    // Play a sound effect
    SetTimer = 15,     // Schedule a timed event
    SetFace = 16,      // Change NPC face
    CastSpell = 17,    // Cast a spell effect
    AdvanceTime = 18,  // Fast-forward game time
    SetReputation = 19 // Modify party reputation
};

// A single command in an event script
struct EventCommand
{
    EventOpcode opcode = EventOpcode::Nop;
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
