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
    processConditions();
}

void GameWorld::resetConditionClock()
{
    lastConditionTick_ = calendar_.totalTicks;
}

void GameWorld::setUtilityEffect(UtilityEffect effect, int strength, int64_t expiresAtTicks)
{
    const size_t index = static_cast<size_t>(effect);
    if (index >= utilityEffects_.size())
    {
        return;
    }
    utilityEffects_[index].strength = std::max(0, strength);
    utilityEffects_[index].expiresAtTicks = expiresAtTicks;
}

bool GameWorld::hasUtilityEffect(UtilityEffect effect) const
{
    const size_t index = static_cast<size_t>(effect);
    if (index >= utilityEffects_.size())
    {
        return false;
    }
    return utilityEffects_[index].active(calendar_.totalTicks);
}

int GameWorld::utilityEffectStrength(UtilityEffect effect) const
{
    return hasUtilityEffect(effect) ? utilityEffects_[static_cast<size_t>(effect)].strength : 0;
}

const UtilityEffectState& GameWorld::utilityEffect(UtilityEffect effect) const
{
    static const UtilityEffectState kEmpty{};
    const size_t index = static_cast<size_t>(effect);
    return index < utilityEffects_.size() ? utilityEffects_[index] : kEmpty;
}

void GameWorld::clearExpiredUtilityEffects()
{
    const int64_t now = calendar_.totalTicks;
    for (auto& effect : utilityEffects_)
    {
        if (effect.strength > 0 && effect.expiresAtTicks != 0 && now >= effect.expiresAtTicks)
        {
            effect = {};
        }
    }
}

float GameWorld::lightNightReduction() const
{
    const int strength = utilityEffectStrength(UtilityEffect::Light);
    return std::min(0.9f, static_cast<float>(strength) * 0.25f);
}

float GameWorld::effectiveNightBlend() const
{
    return calendar_.nightBlend() * (1.0f - lightNightReduction());
}

void GameWorld::setBeacon(BeaconLocation location)
{
    if (location.mapName.empty())
    {
        beacon_ = {};
        return;
    }
    location.set = true;
    beacon_ = std::move(location);
}

std::optional<TravelRequest> GameWorld::takeTravelRequest()
{
    std::optional<TravelRequest> request = std::move(pendingTravel_);
    pendingTravel_.reset();
    return request;
}

void GameWorld::processConditions()
{
    const int64_t now = calendar_.totalTicks;
    clearExpiredUtilityEffects();
    if (lastConditionTick_ <= 0)
    {
        // First run after a new game or load: establish the baseline without
        // retroactively applying damage for all elapsed game time.
        lastConditionTick_ = now;
        return;
    }

    // Buff expiry is cheap and should take effect on the frame it is due, not
    // only on the next hourly condition tick.
    for (int i = 0; i < kPartySize; ++i)
    {
        party_.member(i).clearExpiredEffects(now);
    }

    if (now <= lastConditionTick_)
    {
        return;
    }

    const int64_t elapsedTicks = now - lastConditionTick_;
    const int64_t wholeHours = elapsedTicks / GameCalendar::kTicksPerHour;
    if (wholeHours <= 0)
    {
        return;
    }
    lastConditionTick_ += wholeHours * GameCalendar::kTicksPerHour;

    const auto expireAfter = [&](Character& ch, ConditionIndex condition, int64_t hours)
    {
        const int64_t timestamp = ch.conditionTimestamps[static_cast<size_t>(condition)];
        if (timestamp > 1 && now - timestamp >= hours * GameCalendar::kTicksPerHour)
        {
            ch.clearCondition(condition);
        }
    };

    for (int i = 0; i < kPartySize; ++i)
    {
        auto& ch = party_.member(i);
        if (!ch.isAlive())
        {
            continue;
        }

        // Timed conditions expire after their game-time duration.
        expireAfter(ch, ConditionIndex::Weak, 8);
        expireAfter(ch, ConditionIndex::Afraid, 8);
        expireAfter(ch, ConditionIndex::Insane, 8);
        expireAfter(ch, ConditionIndex::Drunk, 8);
        expireAfter(ch, ConditionIndex::Cursed, 24);
        expireAfter(ch, ConditionIndex::Asleep, 8);
        expireAfter(ch, ConditionIndex::Paralyzed, 1);

        // Poison and disease persist until cured and deal damage each hour.
        int damagePerHour = 0;
        if (ch.hasCondition(ConditionIndex::Poison3))
        {
            damagePerHour += 3;
        }
        else if (ch.hasCondition(ConditionIndex::Poison2))
        {
            damagePerHour += 2;
        }
        else if (ch.hasCondition(ConditionIndex::Poison1))
        {
            damagePerHour += 1;
        }

        if (ch.hasCondition(ConditionIndex::Disease3))
        {
            damagePerHour += 3;
        }
        else if (ch.hasCondition(ConditionIndex::Disease2))
        {
            damagePerHour += 2;
        }
        else if (ch.hasCondition(ConditionIndex::Disease1))
        {
            damagePerHour += 1;
        }

        if (damagePerHour > 0)
        {
            ch.takeDamage(static_cast<int>(damagePerHour * wholeHours));
        }
    }
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

bool GameWorld::hasSavedMapMonsters(const std::string& mapName) const
{
    return savedMapMonsters_.contains(mapName);
}

void GameWorld::setSavedMapMonsters(const std::string& mapName,
                                    std::vector<SavedMonsterState> monsters)
{
    if (mapName.empty())
    {
        return;
    }
    // Keep empty lists: they mark a visited map whose roster is cleared.
    savedMapMonsters_[mapName] = std::move(monsters);
}

const std::vector<SavedMonsterState>*
GameWorld::getSavedMapMonsters(const std::string& mapName) const
{
    auto it = savedMapMonsters_.find(mapName);
    return it != savedMapMonsters_.end() ? &it->second : nullptr;
}

void GameWorld::clearSavedMapMonsters(const std::string& mapName)
{
    savedMapMonsters_.erase(mapName);
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
    savedMapMonsters_.clear();
    lastEventInteraction_ = {};
    runtimeConfig_ = RuntimeConfig{};
    utilityEffects_ = {};
    beacon_ = {};
    pendingTravel_.reset();
    lastConditionTick_ = calendar_.totalTicks;
}

} // namespace runeharbor::game
