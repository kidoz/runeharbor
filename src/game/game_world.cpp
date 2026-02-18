// SPDX-License-Identifier: MIT
#include "game_world.hpp"

#include <algorithm>

namespace runeharbor::game
{

GameWorld::GameWorld() = default;

void GameWorld::advanceTime(uint64_t minutes)
{
    calendar_.advance(minutes);
}

void GameWorld::setCurrentMap(const std::string& name)
{
    currentMap_ = name;
    party_.setCurrentMap(name);
    markVisited(name);
}

bool GameWorld::isIndoorMap() const
{
    // Indoor maps use .blv extension, outdoor use .odm
    if (currentMap_.size() >= 4)
    {
        std::string ext = currentMap_.substr(currentMap_.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return ext == ".blv";
    }
    return false;
}

bool GameWorld::isOutdoorMap() const
{
    if (currentMap_.size() >= 4)
    {
        std::string ext = currentMap_.substr(currentMap_.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return ext == ".odm";
    }
    return false;
}

int GameWorld::getVar(GameVarId id) const
{
    auto it = gameVars_.find(id);
    return it != gameVars_.end() ? it->second : 0;
}

void GameWorld::setVar(GameVarId id, int value)
{
    if (value == 0)
    {
        gameVars_.erase(id);
    }
    else
    {
        gameVars_[id] = value;
    }
}

bool GameWorld::hasVisited(const std::string& mapName) const
{
    return visitedMaps_.contains(mapName);
}

void GameWorld::markVisited(const std::string& mapName)
{
    visitedMaps_.insert(mapName);
}

void GameWorld::addTransition(int triggerId, const MapTransition& transition)
{
    transitions_[triggerId] = transition;
}

const MapTransition* GameWorld::getTransition(int triggerId) const
{
    auto it = transitions_.find(triggerId);
    return it != transitions_.end() ? &it->second : nullptr;
}

bool GameWorld::isGameOver() const
{
    return !party_.isPartyAlive();
}

void GameWorld::reset()
{
    party_ = Party();
    calendar_ = GameCalendar();
    currentMap_.clear();
    gameVars_.clear();
    visitedMaps_.clear();
    transitions_.clear();
}

} // namespace runeharbor::game
