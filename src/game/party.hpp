// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "character.hpp"

namespace runeharbor::game
{

// Party alignment (affects quest availability and NPC reactions)
enum class Alignment : uint8_t
{
    Good = 0,
    Neutral,
    Evil,
};

static constexpr int kPartySize = 4;

class Party
{
  public:
    Party();

    // Character access
    Character& member(int index);
    const Character& member(int index) const;
    Character& operator[](int index) { return member(index); }
    const Character& operator[](int index) const { return member(index); }

    // Party resources
    int gold() const { return gold_; }
    void setGold(int amount) { gold_ = amount; }
    void addGold(int amount) { gold_ += amount; }
    bool spendGold(int amount);

    int food() const { return food_; }
    void setFood(int amount) { food_ = amount; }
    void addFood(int amount) { food_ += amount; }
    bool consumeFood(int amount);

    // Alignment & reputation
    Alignment alignment() const { return alignment_; }
    void setAlignment(Alignment a) { alignment_ = a; }
    int reputation() const { return reputation_; }
    void adjustReputation(int delta) { reputation_ += delta; }

    // Party-level queries
    bool isPartyAlive() const;
    int aliveCount() const;
    int consciousCount() const;

    // Active player slot (used by event system mode 4). -1 means "no active member".
    int activeMemberIndex() const { return activeMemberIndex_; }
    void setActiveMemberIndex(int index);

    // Position in world
    float worldX() const { return worldX_; }
    float worldY() const { return worldY_; }
    float worldZ() const { return worldZ_; }
    void setWorldPosition(float x, float y, float z);

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    void setOrientation(float yaw, float pitch);

    // Time tracking
    uint64_t gameTime() const { return gameTime_; }
    void advanceTime(uint64_t ticks) { gameTime_ += ticks; }
    void setGameTime(uint64_t ticks) { gameTime_ = ticks; }

    // Current map
    const std::string& currentMap() const { return currentMap_; }
    void setCurrentMap(const std::string& name) { currentMap_ = name; }

    // Initialize a default party (4 characters with MM7 defaults)
    void initDefault();

    // Recalculate all derived stats for all members
    void recalculateAll();

  private:
    std::array<Character, kPartySize> members_;
    int gold_ = 200;
    int food_ = 7;
    Alignment alignment_ = Alignment::Neutral;
    int reputation_ = 0;

    float worldX_ = 0.0f;
    float worldY_ = 0.0f;
    float worldZ_ = 0.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    uint64_t gameTime_ = 0;
    std::string currentMap_;
    int activeMemberIndex_ = 0;
};

} // namespace runeharbor::game
