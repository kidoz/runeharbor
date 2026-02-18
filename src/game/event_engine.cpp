// SPDX-License-Identifier: MIT
#include "event_engine.hpp"

#include "game_world.hpp"

namespace runeharbor::game
{

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

bool EventEngine::triggerEvent(int eventId)
{
    auto it = events_.find(eventId);
    if (it == events_.end())
    {
        return false;
    }

    logger_.debug("Executing event #" + std::to_string(eventId));

    for (const auto& cmd : it->second.commands)
    {
        if (cmd.opcode == EventOpcode::Exit)
        {
            break;
        }
        executeCommand(cmd);
    }

    return true;
}

bool EventEngine::hasEvent(int eventId) const
{
    return events_.contains(eventId);
}

void EventEngine::clear()
{
    events_.clear();
}

void EventEngine::executeCommand(const EventCommand& cmd)
{
    switch (cmd.opcode)
    {
    case EventOpcode::SetFlag:
        if (gameWorld_)
        {
            gameWorld_->setVar(static_cast<GameVarId>(cmd.param1), cmd.param2);
            logger_.debug("SetFlag " + std::to_string(cmd.param1) + " = " +
                          std::to_string(cmd.param2));
        }
        break;

    case EventOpcode::CheckFlag:
        if (gameWorld_)
        {
            int val = gameWorld_->getVar(static_cast<GameVarId>(cmd.param1));
            if (val != cmd.param2)
            {
                logger_.debug("CheckFlag " + std::to_string(cmd.param1) + " != " +
                              std::to_string(cmd.param2) + " (actual=" + std::to_string(val) + ")");
            }
        }
        break;

    case EventOpcode::GiveGold:
        if (gameWorld_)
        {
            gameWorld_->party().addGold(cmd.param1);
            logger_.debug("GiveGold " + std::to_string(cmd.param1));
        }
        break;

    case EventOpcode::TakeGold:
        if (gameWorld_)
        {
            gameWorld_->party().spendGold(cmd.param1);
            logger_.debug("TakeGold " + std::to_string(cmd.param1));
        }
        break;

    case EventOpcode::GiveExperience:
        if (gameWorld_)
        {
            // Distribute XP to all conscious party members
            int xpEach = cmd.param1 / std::max(1, gameWorld_->party().consciousCount());
            for (int i = 0; i < kPartySize; i++)
            {
                auto& ch = gameWorld_->party().member(i);
                if (ch.isConscious())
                {
                    ch.experience += xpEach;
                }
            }
            logger_.debug("GiveExperience " + std::to_string(cmd.param1));
        }
        break;

    case EventOpcode::ShowText:
        if (callbacks_.onShowText)
        {
            callbacks_.onShowText(cmd.text);
        }
        logger_.debug("ShowText: " + cmd.text.substr(0, 60));
        break;

    case EventOpcode::Teleport:
        if (callbacks_.onTeleport)
        {
            callbacks_.onTeleport(cmd.text, cmd.fparam, cmd.fparam2, cmd.fparam3,
                                  static_cast<float>(cmd.param1));
        }
        logger_.debug("Teleport -> " + cmd.text);
        break;

    case EventOpcode::PlaySound:
        if (callbacks_.onPlaySound)
        {
            callbacks_.onPlaySound(cmd.param1);
        }
        break;

    case EventOpcode::SetGlobalVar:
        if (gameWorld_)
        {
            gameWorld_->setVar(static_cast<GameVarId>(cmd.param1), cmd.param2);
            logger_.debug("SetGlobalVar " + std::to_string(cmd.param1) + " = " +
                          std::to_string(cmd.param2));
        }
        break;

    case EventOpcode::CastSpell:
        logger_.debug("CastSpell " + std::to_string(cmd.param1));
        break;

    case EventOpcode::Exit:
        break;

    default:
        logger_.warning("Unhandled event opcode: " + std::to_string(static_cast<int>(cmd.opcode)));
        break;
    }
}

} // namespace runeharbor::game
