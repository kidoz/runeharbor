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
        if (cmd.opcode == EventOpcode::End)
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
    case EventOpcode::SetVariable:
        if (gameWorld_)
        {
            gameWorld_->setVar(static_cast<GameVarId>(cmd.param1), cmd.param2);
            logger_.debug("SetVar " + std::to_string(cmd.param1) + " = " +
                          std::to_string(cmd.param2));
        }
        break;

    case EventOpcode::CheckVariable:
        if (gameWorld_)
        {
            int val = gameWorld_->getVar(static_cast<GameVarId>(cmd.param1));
            if (val != cmd.param2)
            {
                // In a full implementation, this would skip to a branch target.
                // For now, just log the check.
                logger_.debug("CheckVar " + std::to_string(cmd.param1) + " != " +
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

    case EventOpcode::GiveFood:
        if (gameWorld_)
        {
            gameWorld_->party().addFood(cmd.param1);
            logger_.debug("GiveFood " + std::to_string(cmd.param1));
        }
        break;

    case EventOpcode::TakeFood:
        if (gameWorld_)
        {
            gameWorld_->party().consumeFood(cmd.param1);
            logger_.debug("TakeFood " + std::to_string(cmd.param1));
        }
        break;

    case EventOpcode::GiveXP:
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
            logger_.debug("GiveXP " + std::to_string(cmd.param1));
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

    case EventOpcode::AdvanceTime:
        if (gameWorld_)
        {
            gameWorld_->advanceTime(static_cast<uint64_t>(cmd.param1));
            logger_.debug("AdvanceTime " + std::to_string(cmd.param1) + " minutes");
        }
        break;

    case EventOpcode::SetReputation:
        if (gameWorld_)
        {
            gameWorld_->party().adjustReputation(cmd.param1);
            logger_.debug("SetReputation delta=" + std::to_string(cmd.param1));
        }
        break;

    case EventOpcode::Nop:
    case EventOpcode::End:
        break;

    default:
        logger_.warning("Unhandled event opcode: " + std::to_string(static_cast<int>(cmd.opcode)));
        break;
    }
}

} // namespace runeharbor::game
