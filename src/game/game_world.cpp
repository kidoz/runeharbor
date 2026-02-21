// SPDX-License-Identifier: MIT
#include "game_world.hpp"

#include <algorithm>
#include <utility>

namespace runeharbor::game
{

GameWorld::GameWorld() = default;

void GameWorld::advanceTime(int64_t minutes)
{
    calendar_.advanceMinutes(minutes);
}

void GameWorld::setCurrentMap(const std::string& name)
{
    currentMap_ = name;
    party_.setCurrentMap(name);
    markVisited(name);
    clearLastEventInteraction();
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

void GameWorld::clearTransitions()
{
    transitions_.clear();
}

bool GameWorld::hasGeneratedContent(const std::string& mapName) const
{
    return generatedContent_.contains(mapName);
}

void GameWorld::setGeneratedContent(const std::string& mapName, GeneratedMapContent content)
{
    generatedContent_[mapName] = std::move(content);
}

const GeneratedMapContent* GameWorld::getGeneratedContent(const std::string& mapName) const
{
    auto it = generatedContent_.find(mapName);
    return it != generatedContent_.end() ? &it->second : nullptr;
}

bool GameWorld::hasSavedMapState(const std::string& mapName) const
{
    return savedMapStates_.contains(mapName);
}

void GameWorld::setSavedMapState(const std::string& mapName, SavedMapState state)
{
    savedMapStates_[mapName] = std::move(state);
}

const SavedMapState* GameWorld::getSavedMapState(const std::string& mapName) const
{
    auto it = savedMapStates_.find(mapName);
    return it != savedMapStates_.end() ? &it->second : nullptr;
}

void GameWorld::addSpawnedMapItem(const std::string& mapName, SpawnedMapItem item)
{
    if (mapName.empty())
    {
        return;
    }
    spawnedMapItems_[mapName].push_back(std::move(item));
}

bool GameWorld::hasSpawnedMapItems(const std::string& mapName) const
{
    auto it = spawnedMapItems_.find(mapName);
    return it != spawnedMapItems_.end() && !it->second.empty();
}

const std::vector<SpawnedMapItem>* GameWorld::getSpawnedMapItems(const std::string& mapName) const
{
    auto it = spawnedMapItems_.find(mapName);
    if (it == spawnedMapItems_.end())
    {
        return nullptr;
    }
    return &it->second;
}

void GameWorld::setSpawnedMapItems(const std::string& mapName, std::vector<SpawnedMapItem> items)
{
    if (mapName.empty())
    {
        return;
    }
    if (items.empty())
    {
        spawnedMapItems_.erase(mapName);
        return;
    }
    spawnedMapItems_[mapName] = std::move(items);
}

void GameWorld::setLastEventInteraction(EventInteractionContext context)
{
    if (context.eventId <= 0)
    {
        clearLastEventInteraction();
        return;
    }

    if (context.objectIndex < 0)
    {
        context.type = EventInteractionType::Unknown;
    }
    lastEventInteraction_ = context;
}

void GameWorld::clearLastEventInteraction()
{
    lastEventInteraction_ = {};
}

void GameWorld::setDeferredBuildingEvent(int buildingId)
{
    setVar(kDeferredBuildingVar, std::max(0, buildingId));
}

int GameWorld::deferredBuildingEvent() const
{
    return getVar(kDeferredBuildingVar);
}

int GameWorld::consumeDeferredBuildingEvent()
{
    const int id = deferredBuildingEvent();
    if (id != 0)
    {
        setVar(kDeferredBuildingVar, 0);
    }
    return id;
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
    generatedContent_.clear();
    savedMapStates_.clear();
    spawnedMapItems_.clear();
    lastEventInteraction_ = {};
    runtimeConfig_ = RuntimeConfig{};
}

} // namespace runeharbor::game
