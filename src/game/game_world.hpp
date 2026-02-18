// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "party.hpp"

namespace runeharbor::game
{

// MM7 game calendar: 1 game minute = 2 real seconds at default speed
// Time stored as total minutes since game start
struct GameCalendar
{
    uint64_t totalMinutes = 0;

    // Starting date: 1st of January, year 1168 (MM7 default)
    static constexpr int kStartYear = 1168;
    static constexpr int kMinutesPerHour = 60;
    static constexpr int kHoursPerDay = 24;
    static constexpr int kDaysPerMonth = 28;
    static constexpr int kMonthsPerYear = 12;
    static constexpr int kMinutesPerDay = kMinutesPerHour * kHoursPerDay;
    static constexpr int kMinutesPerMonth = kMinutesPerDay * kDaysPerMonth;
    static constexpr int kMinutesPerYear = kMinutesPerMonth * kMonthsPerYear;

    int minute() const { return static_cast<int>(totalMinutes % kMinutesPerHour); }
    int hour() const { return static_cast<int>((totalMinutes / kMinutesPerHour) % kHoursPerDay); }
    int day() const
    {
        return static_cast<int>((totalMinutes / kMinutesPerDay) % kDaysPerMonth) + 1;
    }
    int month() const
    {
        return static_cast<int>((totalMinutes / kMinutesPerMonth) % kMonthsPerYear) + 1;
    }
    int year() const { return kStartYear + static_cast<int>(totalMinutes / kMinutesPerYear); }

    // Time of day queries
    bool isNight() const
    {
        int h = hour();
        return h < 6 || h >= 20;
    }
    bool isDay() const { return !isNight(); }

    void advance(uint64_t minutes) { totalMinutes += minutes; }
};

// Map transition: where to go when stepping on a trigger
struct MapTransition
{
    std::string targetMap;
    float targetX = 0;
    float targetY = 0;
    float targetZ = 0;
    float targetYaw = 0;
};

// Global game variable (quest flags, event triggers, etc.)
// MM7 uses numbered variables 0-500+ as general-purpose flags
using GameVarId = uint16_t;

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
    void advanceTime(uint64_t minutes);

    // Current map
    const std::string& currentMap() const { return currentMap_; }
    void setCurrentMap(const std::string& name);
    bool isIndoorMap() const;
    bool isOutdoorMap() const;

    // Game variables (quest flags, event state)
    int getVar(GameVarId id) const;
    void setVar(GameVarId id, int value);
    bool isFlagSet(GameVarId id) const { return getVar(id) != 0; }
    void setFlag(GameVarId id) { setVar(id, 1); }
    void clearFlag(GameVarId id) { setVar(id, 0); }

    // Visited locations tracking
    bool hasVisited(const std::string& mapName) const;
    void markVisited(const std::string& mapName);

    // Map transitions
    void addTransition(int triggerId, const MapTransition& transition);
    const MapTransition* getTransition(int triggerId) const;

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
};

} // namespace runeharbor::game
