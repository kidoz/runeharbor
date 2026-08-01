// SPDX-License-Identifier: MIT
#include "event_engine.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "combat.hpp"
#include "game_world.hpp"
#include "inventory.hpp"
#include "spells.hpp"

namespace runeharbor::game
{

namespace
{
constexpr uint32_t kRuntimeStateMagic = 0x45565254u; // "EVRT"
//   1 — lastRuntimeTick + timerTriggers
//   2 — + firedOneShotEvents (OnMapLoad/OnMapEnter already fired this map scope)
constexpr uint32_t kRuntimeStateVersion = 2;

void writeU32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

void writeI32(std::vector<uint8_t>& out, int32_t value)
{
    writeU32(out, static_cast<uint32_t>(value));
}

void writeI64(std::vector<uint8_t>& out, int64_t value)
{
    uint64_t raw = static_cast<uint64_t>(value);
    for (int i = 0; i < 8; i++)
    {
        out.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xFFu));
    }
}

bool readU32(const uint8_t*& ptr, const uint8_t* end, uint32_t& value)
{
    if (ptr + 4 > end)
    {
        return false;
    }
    value = static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
            (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    return true;
}

bool readI32(const uint8_t*& ptr, const uint8_t* end, int32_t& value)
{
    uint32_t raw = 0;
    if (!readU32(ptr, end, raw))
    {
        return false;
    }
    value = static_cast<int32_t>(raw);
    return true;
}

bool readI64(const uint8_t*& ptr, const uint8_t* end, int64_t& value)
{
    if (ptr + 8 > end)
    {
        return false;
    }

    uint64_t raw = 0;
    for (int i = 0; i < 8; i++)
    {
        raw |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    }
    ptr += 8;
    value = static_cast<int64_t>(raw);
    return true;
}

GameVarId qbitStorageKey(int varIndex, int field)
{
    const uint16_t clampedIndex = static_cast<uint16_t>(std::clamp(varIndex, 0, 0x0FFF));
    const uint16_t clampedField = static_cast<uint16_t>(std::clamp(field, 0, 7));
    return static_cast<GameVarId>(0x8000u | static_cast<uint16_t>(clampedIndex << 3) |
                                  clampedField);
}

GameVarId markerVarKey(int base, int id)
{
    const int value = base + std::clamp(id, 0, 0x0FFF);
    return static_cast<GameVarId>(std::clamp(value, 0, 0xFFFF));
}

MonsterInstance::AIState toAiState(int value)
{
    switch (value)
    {
    case 0:
        return MonsterInstance::AIState::Standing;
    case 1:
        return MonsterInstance::AIState::Wandering;
    case 2:
        return MonsterInstance::AIState::Guarding;
    case 3:
        return MonsterInstance::AIState::Fidgeting;
    case 4:
        return MonsterInstance::AIState::Fleeing;
    case 5:
        return MonsterInstance::AIState::Dead;
    case 6:
        return MonsterInstance::AIState::Pursuing;
    case 7:
        return MonsterInstance::AIState::Attacking;
    case 8:
        return MonsterInstance::AIState::AttackingRanged;
    case 9:
        return MonsterInstance::AIState::AttackingMelee2;
    case 11:
        return MonsterInstance::AIState::Stunned;
    case 12:
        return MonsterInstance::AIState::CastingSpell1;
    case 13:
        return MonsterInstance::AIState::CastingSpell2;
    case 17:
        return MonsterInstance::AIState::Paralyzed;
    case 18:
        return MonsterInstance::AIState::CastingSpell3;
    case 19:
        return MonsterInstance::AIState::Stoned;
    default:
        return MonsterInstance::AIState::Standing;
    }
}

int eventSpellIdFromSchoolAndIndex(int school, int spellIndex)
{
    // Event bytecode uses school + spell-in-school index (0..10 in RE notes;
    // some scripts may encode 1..11), so we accept both forms.
    const int normalizedIndex =
        (spellIndex >= 1 && spellIndex <= 11) ? (spellIndex - 1) : std::clamp(spellIndex, 0, 10);

    int baseSpellId = 0;
    switch (school)
    {
    case 0:
        baseSpellId = 1; // Fire
        break;
    case 1:
        baseSpellId = 12; // Air
        break;
    case 2:
        baseSpellId = 23; // Water
        break;
    case 3:
        baseSpellId = 34; // Earth
        break;
    case 6:
        baseSpellId = 45; // Spirit
        break;
    case 7:
        baseSpellId = 56; // Mind
        break;
    case 8:
        baseSpellId = 67; // Body
        break;
    case 9:
        baseSpellId = 78; // Light
        break;
    case 10:
        baseSpellId = 89; // Dark
        break;
    default:
        return 0;
    }

    return baseSpellId + normalizedIndex;
}

bool removeFirstItemByType(Inventory& inventory, int rawItemType)
{
    if (rawItemType < 0 || rawItemType >= static_cast<int>(EquipType::Count))
    {
        return false;
    }

    const auto type = static_cast<EquipType>(rawItemType);
    for (int c = 0; c < kPartySize; c++)
    {
        const auto& inv = inventory.getInventory(c);
        for (const auto& item : inv.equipped)
        {
            if (item.valid() && inventory.getEquipType(item.itemId) == type)
            {
                return inventory.removeItem(item.itemId);
            }
        }
        for (const auto& item : inv.backpack)
        {
            if (item.valid() && inventory.getEquipType(item.itemId) == type)
            {
                return inventory.removeItem(item.itemId);
            }
        }
    }

    return false;
}

size_t findFirstScriptCommandIndex(const EventScript& script)
{
    for (size_t i = 0; i < script.commands.size(); i++)
    {
        if (script.commands[i].opcode != EventOpcode::NoOp)
        {
            return i;
        }
    }
    return script.commands.size();
}

const EventCommand* firstScriptCommand(const EventScript& script)
{
    const size_t index = findFirstScriptCommandIndex(script);
    if (index >= script.commands.size())
    {
        return nullptr;
    }
    return &script.commands[index];
}
} // namespace

EventEngine::EventEngine(util::ILogger& logger) : logger_(logger) {}

void EventEngine::registerEvent(int eventId, EventScript script)
{
    events_[eventId] = std::move(script);
}

void EventEngine::loadEvents(const std::vector<EventScript>& scripts)
{
    for (const auto& script : scripts)
    {
        events_[script.eventId] = script;
    }
    logger_.info("Loaded " + std::to_string(scripts.size()) + " event scripts");
}

void EventEngine::setMapScopedEvents(const std::vector<EventScript>& scripts)
{
    mapScopedEvents_.clear();
    // A new map scope means one-shot fired-state from the previous map no longer
    // applies. (When loading a save, deserializeRuntimeState repopulates this
    // set AFTER setMapScopedEvents, before onMapLoaded runs.)
    firedOneShotEvents_.clear();
    for (const auto& script : scripts)
    {
        mapScopedEvents_[script.eventId] = script;
    }
}

bool EventEngine::triggerEvent(int eventId, int contextFlag)
{
    const EventScript* script = resolveEventScript(eventId);
    if (!script)
    {
        return false;
    }

    logger_.debug("Executing event #" + std::to_string(eventId));
    executionContext_ = (contextFlag == 0) ? 0 : 1;

    if (gameWorld_)
    {
        const int active = gameWorld_->party().activeMemberIndex();
        // MM7 semantics: active slot value 0 means "no active player selected",
        // defaulting event target mode to random player (mode 6).
        playerSelectMode_ = (active > 0 && active < kPartySize) ? 4 : 6;
    }
    else
    {
        playerSelectMode_ = 4;
    }
    bool shouldExit = false;
    size_t index = findFirstScriptCommandIndex(*script);
    if (index >= script->commands.size())
    {
        return true;
    }
    int guardCounter = 0;

    while (!shouldExit && index < script->commands.size())
    {
        if (++guardCounter > 10000)
        {
            logger_.warning("Event execution guard triggered for event #" +
                            std::to_string(eventId));
            break;
        }
        index = executeCommand(*script, index, shouldExit);
    }

    return true;
}

int EventEngine::triggerEventsByFirstOpcode(uint8_t opcode, bool mapOnly)
{
    std::vector<int> matchingMap;
    matchingMap.reserve(mapScopedEvents_.size());
    for (const auto& [eventId, script] : mapScopedEvents_)
    {
        const EventCommand* first = firstScriptCommand(script);
        if (!first)
        {
            continue;
        }
        const uint8_t firstOpcode = static_cast<uint8_t>(first->opcode);
        if (firstOpcode == opcode)
        {
            matchingMap.push_back(eventId);
        }
    }

    std::sort(matchingMap.begin(), matchingMap.end());

    if (mapOnly)
    {
        int triggered = 0;
        for (int eventId : matchingMap)
        {
            if (triggerEvent(eventId, 0))
            {
                triggered++;
            }
        }
        return triggered;
    }

    std::vector<int> matchingGlobal;
    matchingGlobal.reserve(events_.size());
    for (const auto& [eventId, script] : events_)
    {
        if (mapScopedEvents_.contains(eventId))
        {
            // Event ID is shadowed by map scope in active-map context.
            continue;
        }
        if (script.commands.empty())
        {
            continue;
        }
        const EventCommand* first = firstScriptCommand(script);
        if (!first)
        {
            continue;
        }
        const uint8_t firstOpcode = static_cast<uint8_t>(first->opcode);
        if (firstOpcode == opcode)
        {
            matchingGlobal.push_back(eventId);
        }
    }

    std::sort(matchingGlobal.begin(), matchingGlobal.end());

    int triggered = 0;
    for (int eventId : matchingMap)
    {
        if (triggerEvent(eventId, 0))
        {
            triggered++;
        }
    }
    for (int eventId : matchingGlobal)
    {
        if (triggerEvent(eventId, 0))
        {
            triggered++;
        }
    }

    return triggered;
}

void EventEngine::onMapLoaded()
{
    timerTriggers_.clear();
    if (!gameWorld_)
    {
        return;
    }

    lastRuntimeTick_ = gameWorld_->calendar().totalTicks;

    std::vector<int> onMapLoadEvents;
    std::vector<int> onEnterEvents;
    std::vector<int> ambientEvents;
    std::vector<int> immediateTimerEvents;

    // RE divergence (docs/event-engine.md §"OnMapLoad"): MM7 identifies
    // OnMapLoad/OnMapEnter triggers via a SEPARATE per-map trigger table
    // (FUN_00443fb8), where a trigger-type byte ('5' = 0x35 = 53 for OnMapLoad)
    // marks each entry. RuneHarbor instead classifies by the FIRST command's
    // opcode in each script. This works iff real .evt OnMapLoad scripts begin
    // with a command whose opcode byte equals 53 — which needs verification
    // against actual .evt data (open question). If real scripts don't follow
    // that convention, map-load triggers are silently misclassified here.
    for (const auto& [eventId, script] : mapScopedEvents_)
    {
        const EventCommand* first = firstScriptCommand(script);
        if (!first)
        {
            continue;
        }

        switch (first->opcode)
        {
        case EventOpcode::TriggerOnMapLoad:
            onMapLoadEvents.push_back(eventId);
            break;

        case EventOpcode::PlaySound:
            ambientEvents.push_back(eventId);
            break;

        case EventOpcode::TriggerOnMapEnter:
            onEnterEvents.push_back(eventId);
            break;

        case EventOpcode::TriggerTimerAbsolute:
        case EventOpcode::TriggerTimerPeriodic:
        {
            TimerTrigger trigger;
            trigger.eventId = eventId;
            trigger.intervalTicks = std::max<int64_t>(0, first->i64param);
            trigger.periodic = (first->opcode == EventOpcode::TriggerTimerPeriodic);
            if (trigger.intervalTicks <= 0)
            {
                immediateTimerEvents.push_back(eventId);
                break;
            }
            // MM7-style timer commands encode a calendar interval that is added
            // to the current game clock; absolute timers are one-shot, periodic
            // timers reschedule after each firing.
            trigger.nextTick = lastRuntimeTick_ + trigger.intervalTicks;
            trigger.active = true;
            timerTriggers_.push_back(trigger);
            break;
        }

        default:
            break;
        }
    }

    std::sort(ambientEvents.begin(), ambientEvents.end());
    for (int eventId : ambientEvents)
    {
        if (!callbacks_.onPlaySound)
        {
            continue;
        }
        const EventScript* script = resolveEventScript(eventId);
        if (!script || script->commands.empty())
        {
            continue;
        }
        callbacks_.onPlaySound(script->commands.front().param1);
    }

    std::sort(onEnterEvents.begin(), onEnterEvents.end());
    for (int eventId : onEnterEvents)
    {
        // One-shot: skip if already fired for this map scope (e.g. on save load).
        if (firedOneShotEvents_.count(eventId))
        {
            continue;
        }
        firedOneShotEvents_.insert(eventId);
        (void)triggerEvent(eventId, 0);
    }

    std::sort(immediateTimerEvents.begin(), immediateTimerEvents.end());
    for (int eventId : immediateTimerEvents)
    {
        (void)triggerEvent(eventId, 0);
    }

    std::sort(onMapLoadEvents.begin(), onMapLoadEvents.end());
    for (int eventId : onMapLoadEvents)
    {
        // One-shot: skip if already fired for this map scope (e.g. on save load).
        if (firedOneShotEvents_.count(eventId))
        {
            continue;
        }
        firedOneShotEvents_.insert(eventId);
        (void)triggerEvent(eventId, 0);
    }
    if (onMapLoadEvents.empty() && resolveEventScript(1))
    {
        (void)triggerEvent(1, 0);
    }
}

int EventEngine::updateRuntimeTriggers()
{
    if (!gameWorld_)
    {
        return 0;
    }

    const int64_t now = gameWorld_->calendar().totalTicks;
    if (lastRuntimeTick_ < 0)
    {
        lastRuntimeTick_ = now;
        return 0;
    }
    if (now < lastRuntimeTick_)
    {
        lastRuntimeTick_ = now;
        return 0;
    }

    int fired = 0;
    for (auto& trigger : timerTriggers_)
    {
        if (!trigger.active || now < trigger.nextTick)
        {
            continue;
        }

        if (triggerEvent(trigger.eventId, 0))
        {
            fired++;
        }

        if (!trigger.periodic || trigger.intervalTicks <= 0)
        {
            trigger.active = false;
            continue;
        }

        do
        {
            trigger.nextTick += trigger.intervalTicks;
        } while (trigger.nextTick <= now);
    }

    lastRuntimeTick_ = now;
    return fired;
}

bool EventEngine::hasEvent(int eventId) const
{
    return mapScopedEvents_.contains(eventId) || events_.contains(eventId);
}

void EventEngine::clear()
{
    events_.clear();
    mapScopedEvents_.clear();
    timerTriggers_.clear();
    firedOneShotEvents_.clear();
    lastRuntimeTick_ = -1;
}

std::vector<uint8_t> EventEngine::serializeRuntimeState() const
{
    std::vector<uint8_t> out;
    out.reserve(32 + timerTriggers_.size() * 32);

    writeU32(out, kRuntimeStateMagic);
    writeU32(out, kRuntimeStateVersion);
    writeI64(out, lastRuntimeTick_);
    writeU32(out, static_cast<uint32_t>(timerTriggers_.size()));

    for (const auto& trigger : timerTriggers_)
    {
        writeI32(out, trigger.eventId);
        writeI64(out, trigger.nextTick);
        writeI64(out, trigger.intervalTicks);
        out.push_back(trigger.periodic ? 1u : 0u);
        out.push_back(trigger.active ? 1u : 0u);
    }

    // v2: fired one-shot map events (so a save load skips already-fired
    // OnMapLoad/OnMapEnter events instead of re-granting rewards).
    writeU32(out, static_cast<uint32_t>(firedOneShotEvents_.size()));
    for (int eventId : firedOneShotEvents_)
    {
        writeI32(out, eventId);
    }

    return out;
}

bool EventEngine::deserializeRuntimeState(const std::vector<uint8_t>& data)
{
    if (data.empty())
    {
        timerTriggers_.clear();
        lastRuntimeTick_ = -1;
        return true;
    }

    const uint8_t* ptr = data.data();
    const uint8_t* end = data.data() + data.size();
    uint32_t magic = 0;
    uint32_t version = 0;
    int64_t savedRuntimeTick = -1;
    uint32_t triggerCount = 0;

    if (!readU32(ptr, end, magic) || !readU32(ptr, end, version) ||
        !readI64(ptr, end, savedRuntimeTick) || !readU32(ptr, end, triggerCount))
    {
        return false;
    }

    if (magic != kRuntimeStateMagic)
    {
        return false;
    }
    // Accept v1 (no fired-set) and v2 (with fired-set); anything else is rejected.
    if (version < 1 || version > kRuntimeStateVersion)
    {
        return false;
    }

    std::vector<TimerTrigger> restored;
    restored.reserve(triggerCount);
    for (uint32_t i = 0; i < triggerCount; i++)
    {
        TimerTrigger trigger;
        if (!readI32(ptr, end, trigger.eventId) || !readI64(ptr, end, trigger.nextTick) ||
            !readI64(ptr, end, trigger.intervalTicks))
        {
            return false;
        }
        if (ptr + 2 > end)
        {
            return false;
        }
        trigger.periodic = (ptr[0] != 0);
        trigger.active = (ptr[1] != 0);
        ptr += 2;

        // Ignore stale timer entries pointing to events that are not present in
        // current global/map scope. This keeps save restores resilient across
        // map/event table changes.
        if (!resolveEventScript(trigger.eventId))
        {
            continue;
        }
        restored.push_back(trigger);
    }

    timerTriggers_ = std::move(restored);
    lastRuntimeTick_ = savedRuntimeTick;

    // v2: fired one-shot map events. Missing in v1 → empty set (those saves will
    // re-fire OnMapLoad events once on first load, which is the pre-fix behavior).
    firedOneShotEvents_.clear();
    if (version >= 2)
    {
        uint32_t firedCount = 0;
        if (!readU32(ptr, end, firedCount))
        {
            return false;
        }
        for (uint32_t i = 0; i < firedCount; i++)
        {
            int32_t eventId = 0;
            if (!readI32(ptr, end, eventId))
            {
                return false;
            }
            firedOneShotEvents_.insert(eventId);
        }
    }

    return true;
}

size_t EventEngine::executeCommand(const EventScript& script, size_t index, bool& shouldExit)
{
    if (index >= script.commands.size())
    {
        return script.commands.size();
    }

    const auto& cmd = script.commands[index];
    const size_t nextIndex = index + 1;
    const auto rawOpcode = static_cast<uint8_t>(cmd.opcode);

    // MM7 opcode table has intentional holes that behave as no-ops.
    if (rawOpcode == 0 || rawOpcode == 20 || rawOpcode == 27 || rawOpcode == 28 ||
        rawOpcode == 44 || rawOpcode == 45 || rawOpcode == 46 || rawOpcode == 52)
    {
        return nextIndex;
    }

    switch (cmd.opcode)
    {
    case EventOpcode::Exit:
        shouldExit = true;
        return script.commands.size();

    case EventOpcode::SkipNext:
    case EventOpcode::SkipNext2:
        return std::min(script.commands.size(), index + 2);

    case EventOpcode::JumpToEvent:
        if (cmd.param1 >= 0 && static_cast<size_t>(cmd.param1) < script.commands.size())
        {
            return static_cast<size_t>(cmd.param1);
        }
        if (cmd.param1 > 0 && cmd.param1 != script.eventId && resolveEventScript(cmd.param1))
        {
            (void)triggerEvent(cmd.param1, executionContext_);
            shouldExit = true;
            return script.commands.size();
        }
        return nextIndex;

    case EventOpcode::RandomGoto:
    {
        std::vector<size_t> targets;
        for (int raw : {cmd.param1, cmd.param2, cmd.param3, cmd.param4, cmd.param5, cmd.param6})
        {
            if (raw > 0 && static_cast<size_t>(raw) < script.commands.size())
            {
                targets.push_back(static_cast<size_t>(raw));
            }
        }
        if (targets.empty())
        {
            return nextIndex;
        }
        std::uniform_int_distribution<size_t> pick(0, targets.size() - 1);
        return targets[pick(rng_)];
    }

    case EventOpcode::SetPlayerSelect:
        playerSelectMode_ = static_cast<uint8_t>(std::clamp(cmd.param1, 0, 6));
        return nextIndex;

    case EventOpcode::NpcDialog:
        if (callbacks_.onNpcDialog)
        {
            callbacks_.onNpcDialog(cmd.param1);
        }
        if (callbacks_.onShowText && !cmd.text.empty())
        {
            callbacks_.onShowText(cmd.text);
        }
        return nextIndex;

    case EventOpcode::ShowBuilding:
        if (executionContext_ == 0)
        {
            if (gameWorld_)
            {
                // Mirrors DAT_005c3444 deferred building/event id slot.
                gameWorld_->setDeferredBuildingEvent(cmd.param1);
            }
        }
        else if (callbacks_.onShowBuilding)
        {
            callbacks_.onShowBuilding(cmd.param1);
        }
        return nextIndex;

    case EventOpcode::SetNpcPortrait:
        if (gameWorld_)
        {
            gameWorld_->setVar(markerVarKey(0x7400, 0), cmd.param1);
        }
        return nextIndex;

    case EventOpcode::SetNpcName:
        if (callbacks_.onShowText && !cmd.text.empty())
        {
            callbacks_.onShowText(cmd.text);
        }
        if (gameWorld_)
        {
            gameWorld_->setVar(markerVarKey(0x7401, 0), cmd.param1);
        }
        return nextIndex;

    case EventOpcode::GiveAward:
        if (gameWorld_)
        {
            gameWorld_->setVar(markerVarKey(0x7500, cmd.param1), 1);
        }
        return nextIndex;

    case EventOpcode::ShowEffect:
    case EventOpcode::PlayAnimation:
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::ModifyNpc:
    case EventOpcode::ModifyNpcEx:
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::AddStat:
        applyStatDelta(cmd.param1, std::abs(cmd.param2), playerSelectMode_);
        return nextIndex;

    case EventOpcode::SubtractStat:
        applyStatDelta(cmd.param1, -std::abs(cmd.param2), playerSelectMode_);
        return nextIndex;

    case EventOpcode::SetPlayerVar:
        if (gameWorld_)
        {
            uint8_t mode = playerSelectMode_;
            if (cmd.param5 == 1 && cmd.param4 >= 0 && cmd.param4 <= 6)
            {
                mode = static_cast<uint8_t>(cmd.param4);
            }

            auto targets = resolvePlayerTargets(mode);
            for (int characterIndex : targets)
            {
                if (characterIndex < 0 || characterIndex >= kPartySize)
                {
                    continue;
                }
                auto& ch = gameWorld_->party().member(characterIndex);
                if (cmd.param1 >= 0 && cmd.param1 < Stats::kCount)
                {
                    ch.baseStats.byIndex(cmd.param1) = std::clamp(cmd.param2, 1, 255);
                    ch.recalculateDerived();
                    continue;
                }

                switch (cmd.param1)
                {
                case 7: // HP
                    ch.setHitPoints(cmd.param2);
                    break;
                case 8: // SP
                    ch.spellPoints = std::clamp(cmd.param2, 0, std::max(0, ch.maxSpellPoints));
                    break;
                case 9: // AC
                    ch.armorClass = std::max(0, cmd.param2);
                    break;
                case 10: // Level
                    ch.level = std::clamp(cmd.param2, 1, 200);
                    ch.recalculateDerived();
                    break;
                case 11: // Experience
                    ch.experience = std::max(0, cmd.param2);
                    break;
                default:
                    gameWorld_->setVar(static_cast<GameVarId>(std::clamp(cmd.param1, 0, 65535)),
                                       cmd.param2);
                    break;
                }
            }
        }
        return nextIndex;

    case EventOpcode::SetFlag:
        if (gameWorld_)
        {
            gameWorld_->setVar(static_cast<GameVarId>(cmd.param1), cmd.param2);
            logger_.debug("SetFlag " + std::to_string(cmd.param1) + " = " +
                          std::to_string(cmd.param2));
        }
        return nextIndex;

    case EventOpcode::CheckFlag:
        if (gameWorld_)
        {
            int val = gameWorld_->getVar(static_cast<GameVarId>(cmd.param1));
            if (val != cmd.param2)
            {
                logger_.debug("CheckFlag " + std::to_string(cmd.param1) + " != " +
                              std::to_string(cmd.param2) + " (actual=" + std::to_string(val) + ")");
                if (cmd.param3 > 0 && static_cast<size_t>(cmd.param3) < script.commands.size())
                {
                    return static_cast<size_t>(cmd.param3);
                }
            }
        }
        return nextIndex;

    case EventOpcode::CheckCondition:
        if (gameWorld_)
        {
            const auto targets = resolvePlayerTargets(playerSelectMode_);
            bool passed = false;
            bool allPassed = true;
            bool hadTarget = false;
            for (int idxTarget : targets)
            {
                if (idxTarget < 0 || idxTarget >= kPartySize)
                {
                    continue;
                }
                hadTarget = true;
                const auto& ch = gameWorld_->party().member(idxTarget);
                bool onePassed = false;

                if (cmd.param4 == 0)
                {
                    auto cond = static_cast<ConditionIndex>(
                        std::clamp(cmd.param1, 0, static_cast<int>(ConditionIndex::Count) - 1));
                    onePassed = ch.hasCondition(cond);
                }
                else
                {
                    int currentValue = 0;
                    if (cmd.param1 >= 0 && cmd.param1 < Stats::kCount)
                    {
                        currentValue = ch.stats.byIndex(cmd.param1);
                    }
                    else
                    {
                        switch (cmd.param1)
                        {
                        case 7:
                            currentValue = ch.hitPoints;
                            break;
                        case 8:
                            currentValue = ch.spellPoints;
                            break;
                        case 9:
                            currentValue = ch.armorClass;
                            break;
                        case 10:
                            currentValue = ch.level;
                            break;
                        case 11:
                            currentValue = ch.experience;
                            break;
                        default:
                            currentValue = gameWorld_->getVar(
                                static_cast<GameVarId>(std::clamp(cmd.param1, 0, 65535)));
                            break;
                        }
                    }
                    onePassed = (currentValue >= cmd.param2);
                }

                passed = passed || onePassed;
                allPassed = allPassed && onePassed;
            }

            if (!hadTarget)
            {
                passed = false;
                allPassed = false;
            }

            const bool shouldJump = (playerSelectMode_ == 5) ? allPassed : passed;
            if (shouldJump && cmd.param3 > 0 &&
                static_cast<size_t>(cmd.param3) < script.commands.size())
            {
                return static_cast<size_t>(cmd.param3);
            }
        }
        return nextIndex;

    case EventOpcode::CheckTime:
        if (gameWorld_)
        {
            int nowHour = gameWorld_->calendar().hour();
            int minHour = std::clamp(cmd.param1, 0, 23);
            int maxHour = std::clamp(cmd.param2, 0, 23);
            bool inRange = (minHour <= maxHour) ? (nowHour >= minHour && nowHour <= maxHour)
                                                : (nowHour >= minHour || nowHour <= maxHour);
            if (inRange && cmd.param3 > 0 &&
                static_cast<size_t>(cmd.param3) < script.commands.size())
            {
                return static_cast<size_t>(cmd.param3);
            }
        }
        return nextIndex;

    case EventOpcode::CheckSkill:
        if (gameWorld_)
        {
            const auto targets = resolvePlayerTargets(playerSelectMode_);
            const int skillId = std::clamp(cmd.param1, 0, static_cast<int>(SkillId::Count) - 1);
            const int requiredLevel = std::max(0, cmd.param2);
            const int requiredMastery = std::clamp(cmd.param4, 0, 4);
            bool any = false;
            bool all = true;
            bool hadTarget = false;
            for (int idxTarget : targets)
            {
                if (idxTarget < 0 || idxTarget >= kPartySize)
                {
                    continue;
                }
                hadTarget = true;
                const auto& skill =
                    gameWorld_->party().member(idxTarget).skillLevels[static_cast<size_t>(skillId)];
                const bool levelOk = skill.effective() >= requiredLevel;
                const int masteryRank = static_cast<int>(skill.mastery);
                const bool masteryOk =
                    (requiredMastery == 0) ? true : (masteryRank >= requiredMastery);
                const bool passed = levelOk && masteryOk;
                any = any || passed;
                all = all && passed;
            }

            if (!hadTarget)
            {
                any = false;
                all = false;
            }

            const bool shouldJump = (playerSelectMode_ == 5) ? all : any;
            if (shouldJump && cmd.param3 > 0 &&
                static_cast<size_t>(cmd.param3) < script.commands.size())
            {
                return static_cast<size_t>(cmd.param3);
            }
        }
        return nextIndex;

    case EventOpcode::CheckMapVar:
        if (gameWorld_)
        {
            int val = gameWorld_->getVar(static_cast<GameVarId>(cmd.param1));
            if (val == cmd.param2)
            {
                if (cmd.param3 > 0 && static_cast<size_t>(cmd.param3) < script.commands.size())
                {
                    return static_cast<size_t>(cmd.param3);
                }
            }
        }
        return nextIndex;

    case EventOpcode::GiveGold:
        if (gameWorld_)
        {
            gameWorld_->party().addGold(cmd.param1);
            logger_.debug("GiveGold " + std::to_string(cmd.param1));
        }
        return nextIndex;

    case EventOpcode::TakeGold:
        if (gameWorld_)
        {
            gameWorld_->party().spendGold(cmd.param1);
            logger_.debug("TakeGold " + std::to_string(cmd.param1));
        }
        return nextIndex;

    case EventOpcode::GiveExperience:
        if (gameWorld_)
        {
            const int xp = std::max(0, cmd.param1);
            const auto targets = resolvePlayerTargets(playerSelectMode_);
            for (int idxTarget : targets)
            {
                if (idxTarget < 0 || idxTarget >= kPartySize)
                {
                    continue;
                }

                auto& ch = gameWorld_->party().member(idxTarget);
                if (ch.isConscious())
                {
                    ch.experience += xp;
                }
            }
            logger_.debug("GiveExperience " + std::to_string(cmd.param1));
        }
        return nextIndex;

    case EventOpcode::GiveItem:
    {
        bool given = false;
        if (inventory_ && cmd.param1 > 0)
        {
            uint8_t mode = playerSelectMode_;
            if (cmd.param4 >= 0 && cmd.param4 <= 6)
            {
                mode = static_cast<uint8_t>(cmd.param4);
            }

            const auto targets = resolvePlayerTargets(mode);
            Item item;
            item.itemId = cmd.param1;

            for (int idxTarget : targets)
            {
                if (idxTarget < 0 || idxTarget >= kPartySize)
                {
                    continue;
                }
                if (inventory_->addToBackpack(idxTarget, item))
                {
                    given = true;
                    break;
                }
            }

            if (!given)
            {
                given = inventory_->giveItem(item);
            }
        }

        if (!given && callbacks_.onGiveItem && cmd.param1 > 0)
        {
            callbacks_.onGiveItem(cmd.param1);
        }
        return nextIndex;
    }

    case EventOpcode::RemoveItem:
    {
        bool removed = false;
        if (inventory_)
        {
            if (cmd.param1 > 0)
            {
                removed = inventory_->removeItem(cmd.param1);
            }
            if (!removed && cmd.param4 >= 0)
            {
                removed = removeFirstItemByType(*inventory_, cmd.param4);
            }
        }
        if (!removed && callbacks_.onRemoveItem && cmd.param1 > 0)
        {
            removed = callbacks_.onRemoveItem(cmd.param1);
        }
        return nextIndex;
    }

    case EventOpcode::ShowText:
        if (callbacks_.onShowText && !cmd.text.empty())
        {
            callbacks_.onShowText(cmd.text);
        }
        logger_.debug("ShowText: " + cmd.text.substr(0, 60));
        if (cmd.text.empty() && cmd.param4 > 0 &&
            static_cast<size_t>(cmd.param4) < script.commands.size())
        {
            return static_cast<size_t>(cmd.param4);
        }
        return nextIndex;

    case EventOpcode::StatusMessage:
        if (callbacks_.onShowText && !cmd.text.empty())
        {
            callbacks_.onShowText(cmd.text);
        }
        shouldExit = true;
        return script.commands.size();

    case EventOpcode::Teleport:
        if (callbacks_.onTeleport)
        {
            callbacks_.onTeleport(cmd.text, cmd.fparam, cmd.fparam2, cmd.fparam3,
                                  static_cast<float>(cmd.param1));
        }
        logger_.debug("Teleport -> " + cmd.text);
        return nextIndex;

    case EventOpcode::ChangeMap:
        if (callbacks_.onChangeMap)
        {
            callbacks_.onChangeMap(cmd.text, cmd.param1, cmd.param2);
        }
        else if (callbacks_.onTeleport)
        {
            callbacks_.onTeleport(cmd.text, cmd.fparam, cmd.fparam2, cmd.fparam3,
                                  static_cast<float>(cmd.param1));
        }
        logger_.debug("ChangeMap -> " + cmd.text);
        return nextIndex;

    case EventOpcode::PlaySound:
        if (callbacks_.onPlaySound)
        {
            callbacks_.onPlaySound(cmd.param1);
        }
        return nextIndex;

    case EventOpcode::SetGlobalVar:
        if (gameWorld_)
        {
            const int field = std::clamp(cmd.param4, 0, 5);
            gameWorld_->setVar(qbitStorageKey(cmd.param1, field), cmd.param2);
            // Backward-compatible mirror used by existing scripts/tests.
            if (field == 0)
            {
                gameWorld_->setVar(static_cast<GameVarId>(cmd.param1), cmd.param2);
            }

            // MM7 special-case milestone: var 8 / field 0 / value 78 refreshes NPC dialog flow.
            if (cmd.param1 == 8 && field == 0 && cmd.param2 == 78 && callbacks_.onNpcDialog)
            {
                callbacks_.onNpcDialog(0xAA);
            }
            logger_.debug("SetGlobalVar " + std::to_string(cmd.param1) + " = " +
                          std::to_string(cmd.param2));
        }
        if (callbacks_.onSetGlobalVar)
        {
            callbacks_.onSetGlobalVar(cmd.param1, std::clamp(cmd.param4, 0, 5), cmd.param2);
        }
        return nextIndex;

    case EventOpcode::SetGlobalVar2:
        if (gameWorld_)
        {
            gameWorld_->setVar(qbitStorageKey(cmd.param1, 6), cmd.param2);
            // Backward-compatible mirror used by existing scripts/tests.
            gameWorld_->setVar(static_cast<GameVarId>(cmd.param1), cmd.param2);
            logger_.debug("SetGlobalVar2 " + std::to_string(cmd.param1) + " = " +
                          std::to_string(cmd.param2));
        }
        if (callbacks_.onSetGlobalVar)
        {
            callbacks_.onSetGlobalVar(cmd.param1, 6, cmd.param2);
        }
        return nextIndex;

    case EventOpcode::CastSpell:
        if (spellSystem_ && gameWorld_)
        {
            const int spellId = eventSpellIdFromSchoolAndIndex(cmd.param1, cmd.param2);
            if (spellId > 0)
            {
                const auto targets = resolvePlayerTargets(playerSelectMode_);
                for (int idxTarget : targets)
                {
                    if (idxTarget < 0 || idxTarget >= kPartySize)
                    {
                        continue;
                    }
                    (void)spellSystem_->castScriptSpell(spellId, cmd.param3, idxTarget);
                }
            }
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        logger_.debug("CastSpell school=" + std::to_string(cmd.param1) + " spell=" +
                      std::to_string(cmd.param2) + " power=" + std::to_string(cmd.param3));
        return nextIndex;

    case EventOpcode::CureCondition:
        if (gameWorld_)
        {
            const uint8_t mode = static_cast<uint8_t>(
                std::clamp(cmd.param1, 0, 6)); // opcode may pass explicit target selector
            const auto targets = resolvePlayerTargets(mode);
            const auto condition = static_cast<ConditionIndex>(
                std::clamp(cmd.param2, 0, static_cast<int>(ConditionIndex::Count) - 1));

            for (int idxTarget : targets)
            {
                if (idxTarget < 0 || idxTarget >= kPartySize)
                {
                    continue;
                }
                gameWorld_->party().member(idxTarget).clearCondition(condition);
            }
        }
        return nextIndex;

    case EventOpcode::TriggerTimerAbsolute:
    case EventOpcode::TriggerTimerPeriodic:
        if (index == 0)
        {
            // Leading timer opcodes are map-trigger markers handled by onMapLoaded().
            return nextIndex;
        }
        if (gameWorld_)
        {
            const int targetEventId = (cmd.param1 > 0) ? cmd.param1 : script.eventId;
            const int64_t nowTicks = gameWorld_->calendar().totalTicks;
            if (lastRuntimeTick_ < 0)
            {
                lastRuntimeTick_ = nowTicks;
            }

            TimerTrigger trigger;
            trigger.eventId = targetEventId;
            trigger.periodic = (cmd.opcode == EventOpcode::TriggerTimerPeriodic);
            trigger.intervalTicks = std::max<int64_t>(0, cmd.i64param);
            if (trigger.periodic)
            {
                if (trigger.intervalTicks <= 0)
                {
                    // MM7 behavior: zero-interval periodic timer fires once.
                    if (targetEventId != script.eventId && resolveEventScript(targetEventId))
                    {
                        (void)triggerEvent(targetEventId, executionContext_);
                    }
                    trigger.active = false;
                }
                else
                {
                    trigger.nextTick = nowTicks + trigger.intervalTicks;
                    trigger.active = true;
                }
            }
            else
            {
                if (trigger.intervalTicks <= 0)
                {
                    if (targetEventId != script.eventId && resolveEventScript(targetEventId))
                    {
                        (void)triggerEvent(targetEventId, executionContext_);
                    }
                    trigger.active = false;
                }
                else
                {
                    trigger.nextTick = nowTicks + trigger.intervalTicks;
                    trigger.active = true;
                }
            }

            if (trigger.active)
            {
                timerTriggers_.push_back(trigger);
            }
        }
        return nextIndex;

    case EventOpcode::SetMonsterTopic:
        if (combatSystem_)
        {
            combatSystem_->setMonsterTopic(cmd.param1, static_cast<uint16_t>(cmd.param2));
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::SetMonsterField:
        if (combatSystem_)
        {
            combatSystem_->setMonsterField(cmd.param1, cmd.param2, cmd.param3);
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::SetMonsterHostile:
        if (combatSystem_)
        {
            combatSystem_->setMonsterHostileByGroup(cmd.param1, cmd.param2 != 0);
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::SetMonsterGroup:
        if (combatSystem_)
        {
            combatSystem_->setMonsterAiByGroup(cmd.param1, toAiState(cmd.param2));
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::ReplaceMonster:
        if (combatSystem_)
        {
            combatSystem_->replaceMonsterType(cmd.param1, cmd.param2);
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::SetMonsterAi:
        if (combatSystem_)
        {
            combatSystem_->setMonsterAiByType(cmd.param1, toAiState(cmd.param2));
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::SetHostileByIdx:
        if (combatSystem_)
        {
            combatSystem_->setMonsterHostileByIndex(cmd.param1, cmd.param2 != 0);
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::SpawnItem:
        if (gameWorld_)
        {
            SpawnedMapItem item;
            item.itemType = std::max(0, cmd.param1);
            item.count = std::max(1, cmd.param2);
            item.x = cmd.fparam;
            item.y = cmd.fparam2;
            item.z = cmd.fparam3;
            item.createdAtTicks = gameWorld_->calendar().totalTicks;
            gameWorld_->addSpawnedMapItem(gameWorld_->currentMap(), std::move(item));
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::DoorControl:
    case EventOpcode::ModifyObject:
    case EventOpcode::ModifyDecoration:
    case EventOpcode::TriggerOnMapEnter:
    case EventOpcode::TriggerOnMapLoad:
        if (cmd.opcode == EventOpcode::TriggerOnMapEnter ||
            cmd.opcode == EventOpcode::TriggerOnMapLoad)
        {
            return nextIndex;
        }
        if (callbacks_.onMapCommand)
        {
            callbacks_.onMapCommand(cmd);
        }
        return nextIndex;

    case EventOpcode::Unused20:
    case EventOpcode::Unused27:
    case EventOpcode::Unused28:
    case EventOpcode::Unused44:
    case EventOpcode::Unused45:
    case EventOpcode::Unused46:
    case EventOpcode::Unused52:
        return nextIndex;

    default:
        logger_.warning("Unhandled event opcode: " + std::to_string(static_cast<int>(cmd.opcode)));
        return nextIndex;
    }
}

std::vector<int> EventEngine::resolvePlayerTargets(uint8_t mode) const
{
    std::vector<int> targets;

    if (!gameWorld_)
    {
        return targets;
    }

    switch (mode)
    {
    case 0:
    case 1:
    case 2:
    case 3:
        targets.push_back(static_cast<int>(mode));
        break;
    case 5:
        targets = {0, 1, 2, 3};
        break;
    case 6:
    {
        std::uniform_int_distribution<int> pick(0, kPartySize - 1);
        targets.push_back(pick(rng_));
        break;
    }
    case 4:
    default:
    {
        int active = gameWorld_->party().activeMemberIndex();
        if (active >= 0 && active < kPartySize && gameWorld_->party().member(active).isConscious())
        {
            targets.push_back(active);
            break;
        }

        // Fallback: first conscious member, then slot 0 as final guard.
        for (int i = 0; i < kPartySize; i++)
        {
            if (gameWorld_->party().member(i).isConscious())
            {
                targets.push_back(i);
                return targets;
            }
        }
        targets.push_back(0);
        break;
    }
    }

    return targets;
}

void EventEngine::applyStatDelta(int statId, int delta, uint8_t mode)
{
    if (!gameWorld_)
    {
        return;
    }

    for (int characterIndex : resolvePlayerTargets(mode))
    {
        if (characterIndex < 0 || characterIndex >= kPartySize)
        {
            continue;
        }

        auto& ch = gameWorld_->party().member(characterIndex);
        if (statId >= 0 && statId < Stats::kCount)
        {
            int value = ch.baseStats.byIndex(statId) + delta;
            ch.baseStats.byIndex(statId) = std::clamp(value, 1, 255);
            ch.recalculateDerived();
            continue;
        }

        switch (statId)
        {
        case 7: // HP
            ch.setHitPoints(ch.hitPoints + delta);
            break;
        case 8: // SP
            ch.spellPoints = std::clamp(ch.spellPoints + delta, 0, std::max(0, ch.maxSpellPoints));
            break;
        case 9: // AC
            ch.armorClass = std::max(0, ch.armorClass + delta);
            break;
        case 10: // Level
            ch.level = std::clamp(ch.level + delta, 1, 200);
            ch.recalculateDerived();
            break;
        case 11: // Experience
            ch.experience = std::max(0, ch.experience + delta);
            break;
        default:
            // Fallback: treat unknown stat IDs as script variables.
            {
                const GameVarId varId = static_cast<GameVarId>(std::clamp(statId, 0, 65535));
                gameWorld_->setVar(varId, gameWorld_->getVar(varId) + delta);
            }
            break;
        }
    }
}

const EventScript* EventEngine::resolveEventScript(int eventId) const
{
    if (auto it = mapScopedEvents_.find(eventId); it != mapScopedEvents_.end())
    {
        return &it->second;
    }
    if (auto it = events_.find(eventId); it != events_.end())
    {
        return &it->second;
    }
    return nullptr;
}

} // namespace runeharbor::game
