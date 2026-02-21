// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "generation.hpp"
#include "party.hpp"

namespace runeharbor::game
{

// MM7 game calendar: time stored as totalTicks (128 ticks per second)
struct GameCalendar
{
    int64_t totalTicks = 0;

    // Starting date: 1st of January, year 1168 (MM7 default)
    static constexpr int kStartYear = 1168;
    static constexpr int64_t kTicksPerSecond = 128;
    static constexpr int64_t kTicksPerMinute = 128 * 60; // 7,680
    static constexpr int64_t kTicksPerHour = 128 * 3600; // 460,800
    static constexpr int64_t kTicksPerDay = 128 * 86400; // 11,059,200
    static constexpr int kDaysPerWeek = 7;
    static constexpr int kDaysPerMonth = 28;
    static constexpr int kMonthsPerYear = 12;
    static constexpr int64_t kTicksPerMonth = kTicksPerDay * kDaysPerMonth;
    static constexpr int64_t kTicksPerYear = kTicksPerMonth * kMonthsPerYear;
    static constexpr int kDawnStartHour = 5;
    static constexpr int kDawnEndHour = 6;
    static constexpr int kDuskStartHour = 20;
    static constexpr int kDuskEndHour = 21;

    int dayOfWeek() const
    {
        int64_t totalDays = totalTicks / kTicksPerDay;
        int dow = static_cast<int>(totalDays % kDaysPerWeek);
        if (dow < 0)
        {
            dow += kDaysPerWeek;
        }
        return dow;
    }

    int minute() const { return static_cast<int>((totalTicks / kTicksPerMinute) % 60); }
    int hour() const { return static_cast<int>((totalTicks / kTicksPerHour) % 24); }
    int day() const { return static_cast<int>((totalTicks / kTicksPerDay) % kDaysPerMonth) + 1; }
    int month() const
    {
        return static_cast<int>((totalTicks / kTicksPerMonth) % kMonthsPerYear) + 1;
    }
    int year() const { return kStartYear + static_cast<int>(totalTicks / kTicksPerYear); }

    float hourOfDay() const
    {
        int64_t dayTicks = totalTicks % kTicksPerDay;
        if (dayTicks < 0)
        {
            dayTicks += kTicksPerDay;
        }
        return static_cast<float>(dayTicks) / static_cast<float>(kTicksPerHour);
    }

    // 0.0 = full day palette, 1.0 = full night palette.
    float nightBlend() const
    {
        const float hourValue = hourOfDay();
        if (hourValue < static_cast<float>(kDawnStartHour))
        {
            return 1.0f;
        }
        if (hourValue < static_cast<float>(kDawnEndHour))
        {
            return static_cast<float>(kDawnEndHour) - hourValue;
        }
        if (hourValue < static_cast<float>(kDuskStartHour))
        {
            return 0.0f;
        }
        if (hourValue < static_cast<float>(kDuskEndHour))
        {
            return hourValue - static_cast<float>(kDuskStartHour);
        }
        return 1.0f;
    }

    // Time of day queries
    bool isNight() const
    {
        int h = hour();
        return h < 6 || h >= 20;
    }
    bool isDay() const { return !isNight(); }

    void advanceTicks(int64_t ticks) { totalTicks += ticks; }
    void advanceMinutes(int64_t minutes) { totalTicks += minutes * kTicksPerMinute; }
};

// Map transition: where to go when stepping on a trigger
struct MapTransition
{
    std::string targetMap;
    std::string targetDisplayName;
    float targetX = 0;
    float targetY = 0;
    float targetZ = 0;
    float targetYaw = 0;
    bool hasArrivalOverride = false;
};

// Dynamic runtime state preserved per visited map.
struct SavedMapState
{
    std::vector<uint32_t> indoorFaceAttributes;
    std::vector<uint8_t> indoorDecorationHidden;
    std::vector<std::vector<uint32_t>> outdoorBuildingFaceAttributes;
};

// Runtime items spawned by EVT/map logic.
struct SpawnedMapItem
{
    int itemType = 0;
    int count = 1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int64_t createdAtTicks = 0;
};

enum class EventInteractionType : uint8_t
{
    Unknown = 0,
    IndoorFace = 1,
    IndoorDecoration = 2,
    OutdoorBuildingFace = 3,
};

struct EventInteractionContext
{
    int eventId = 0;
    EventInteractionType type = EventInteractionType::Unknown;
    int objectIndex = -1;
};

// Global game variable (quest flags, event triggers, etc.)
// MM7 uses numbered variables 0-500+ as general-purpose flags
using GameVarId = uint16_t;

inline constexpr GameVarId kDeferredBuildingVar = static_cast<GameVarId>(0x7700);

struct RuntimeConfig
{
    bool noMonsters = false;
    bool noDamage = false;
    bool noDecorations = false;
    bool noSky = false;
    bool noWavyWater = false;
    bool noMist = false;
    int walkSpeed = 384;
    int partyHeight = 192;
    int partyEyeLevel = 160;
    int gridBand1 = 10;
    int gridBand2 = 15;
    int gridBand3 = 25;
    int terrainGamma = 0;
    int buildingGamma = 0;
    int distShade = 2048;
    int distShadeMist = 4096;
    int distMist = 8192;
    std::array<uint8_t, 3> skyDayTop = {81, 121, 236};
    std::array<uint8_t, 3> skyDayBottom = {153, 193, 237};
    std::array<uint8_t, 3> skyNightTop = {0, 0, 0};
    std::array<uint8_t, 3> skyNightBottom = {11, 41, 129};
};

class GameWorld
{
  public:
    GameWorld();

    // Party
    Party& party() { return party_; }
    const Party& party() const { return party_; }

    // Calendar
    GameCalendar& calendar() { return calendar_; }
    const GameCalendar& calendar() const { return calendar_; }
    void advanceTime(int64_t minutes);

    // Current map
    const std::string& currentMap() const { return currentMap_; }
    void setCurrentMap(const std::string& name);
    bool isIndoorMap() const;
    bool isOutdoorMap() const;

    // Game variables (quest flags, event state)
    int getVar(GameVarId id) const;
    void setVar(GameVarId id, int value);
    const std::unordered_map<GameVarId, int>& vars() const { return gameVars_; }
    bool isFlagSet(GameVarId id) const { return getVar(id) != 0; }
    void setFlag(GameVarId id) { setVar(id, 1); }
    void clearFlag(GameVarId id) { setVar(id, 0); }

    // Visited locations tracking
    bool hasVisited(const std::string& mapName) const;
    void markVisited(const std::string& mapName);
    const std::unordered_set<std::string>& visitedMaps() const { return visitedMaps_; }

    // Map transitions
    void addTransition(int triggerId, const MapTransition& transition);
    const MapTransition* getTransition(int triggerId) const;
    void clearTransitions();
    const std::unordered_map<int, MapTransition>& transitions() const { return transitions_; }

    // Generated runtime content (map-local monsters/chests)
    bool hasGeneratedContent(const std::string& mapName) const;
    void setGeneratedContent(const std::string& mapName, GeneratedMapContent content);
    const GeneratedMapContent* getGeneratedContent(const std::string& mapName) const;
    const std::unordered_map<std::string, GeneratedMapContent>& generatedContentEntries() const
    {
        return generatedContent_;
    }

    // Preserved runtime map state (doors/face flags and similar dynamic edits).
    bool hasSavedMapState(const std::string& mapName) const;
    void setSavedMapState(const std::string& mapName, SavedMapState state);
    const SavedMapState* getSavedMapState(const std::string& mapName) const;
    const std::unordered_map<std::string, SavedMapState>& savedMapStates() const
    {
        return savedMapStates_;
    }

    // Runtime spawned map items.
    void addSpawnedMapItem(const std::string& mapName, SpawnedMapItem item);
    bool hasSpawnedMapItems(const std::string& mapName) const;
    const std::vector<SpawnedMapItem>* getSpawnedMapItems(const std::string& mapName) const;
    void setSpawnedMapItems(const std::string& mapName, std::vector<SpawnedMapItem> items);
    const std::unordered_map<std::string, std::vector<SpawnedMapItem>>& spawnedMapItems() const
    {
        return spawnedMapItems_;
    }

    // Last object interaction context (used by event opcodes that target clicked map objects).
    void setLastEventInteraction(EventInteractionContext context);
    const EventInteractionContext& lastEventInteraction() const { return lastEventInteraction_; }
    void clearLastEventInteraction();

    // Deferred building/event marker used by non-interactive EVT_SHOW_BUILDING processing.
    void setDeferredBuildingEvent(int buildingId);
    int deferredBuildingEvent() const;
    int consumeDeferredBuildingEvent();

    // Game state
    const RuntimeConfig& runtimeConfig() const { return runtimeConfig_; }
    void setRuntimeConfig(const RuntimeConfig& config) { runtimeConfig_ = config; }

    // Game state
    bool isGameOver() const;
    void reset();

  private:
    Party party_;
    GameCalendar calendar_;
    std::string currentMap_;

    std::unordered_map<GameVarId, int> gameVars_;
    std::unordered_set<std::string> visitedMaps_;
    std::unordered_map<int, MapTransition> transitions_;
    std::unordered_map<std::string, GeneratedMapContent> generatedContent_;
    std::unordered_map<std::string, SavedMapState> savedMapStates_;
    std::unordered_map<std::string, std::vector<SpawnedMapItem>> spawnedMapItems_;
    EventInteractionContext lastEventInteraction_;
    RuntimeConfig runtimeConfig_;
};

} // namespace runeharbor::game
