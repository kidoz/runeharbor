// SPDX-License-Identifier: MIT
#include "evt_script_parser.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <unordered_map>

#include <cctype>

namespace runeharbor::formats
{

EvtScriptParser::EvtScriptParser(util::ILogger& logger) : logger_(logger) {}

std::vector<std::string>
EvtScriptParser::parseStringTable(const std::vector<uint8_t>& strData) const
{
    std::vector<std::string> strings;
    strings.emplace_back(); // index 0 unused (MM-style 1-based string IDs)

    size_t start = 0;
    while (start < strData.size())
    {
        size_t end = start;
        while (end < strData.size() && strData[end] != 0)
        {
            end++;
        }

        if (end > start)
        {
            std::string text(reinterpret_cast<const char*>(strData.data() + start), end - start);
            strings.push_back(trimLeadingSpaces(text));
        }
        else
        {
            strings.emplace_back();
        }

        start = end + 1;
    }

    return strings;
}

std::vector<game::EventScript>
EvtScriptParser::parseEventData(const std::vector<uint8_t>& evtData,
                                const std::vector<std::string>& strings) const
{
    if (evtData.empty())
    {
        return {};
    }

    struct IndexedCommand
    {
        uint8_t subId = 0;
        game::EventCommand command;
    };

    std::unordered_map<int, std::vector<IndexedCommand>> grouped;

    size_t offset = 0;
    while (offset < evtData.size())
    {
        if (offset + 5 > evtData.size())
        {
            logger_.warning("EVT parse stopped at truncated command header");
            break;
        }

        const uint8_t commandSize = evtData[offset];
        if (commandSize < 4)
        {
            logger_.warning(
                std::format("Invalid EVT command size {} at offset {}", commandSize, offset));
            break;
        }

        const size_t totalSize = static_cast<size_t>(commandSize) + 1;
        if (offset + totalSize > evtData.size())
        {
            logger_.warning("EVT parse stopped at truncated command body");
            break;
        }

        const int eventId = static_cast<int>(readU16(evtData, offset + 1));
        const uint8_t subId = evtData[offset + 3];
        const uint8_t opcodeByte = evtData[offset + 4];

        std::vector<uint8_t> params;
        const size_t paramSize = static_cast<size_t>(commandSize) - 4;
        if (paramSize > 0)
        {
            params.insert(params.end(), evtData.begin() + static_cast<std::ptrdiff_t>(offset + 5),
                          evtData.begin() + static_cast<std::ptrdiff_t>(offset + 5 + paramSize));
        }

        game::EventCommand cmd = decodeCommand(opcodeByte, params, strings);
        grouped[eventId].push_back({subId, std::move(cmd)});

        offset += totalSize;
    }

    std::vector<game::EventScript> scripts;
    scripts.reserve(grouped.size());

    for (auto& [eventId, commands] : grouped)
    {
        std::sort(commands.begin(), commands.end(),
                  [](const IndexedCommand& a, const IndexedCommand& b)
                  { return a.subId < b.subId; });

        uint8_t maxSub = 0;
        for (const auto& indexed : commands)
        {
            maxSub = std::max(maxSub, indexed.subId);
        }

        game::EventScript script;
        script.eventId = eventId;
        script.commands.resize(static_cast<size_t>(maxSub) + 1);
        for (auto& cmd : script.commands)
        {
            cmd.opcode = game::EventOpcode::NoOp;
        }

        for (const auto& indexed : commands)
        {
            script.commands[static_cast<size_t>(indexed.subId)] = indexed.command;
        }

        scripts.push_back(std::move(script));
    }

    std::sort(scripts.begin(), scripts.end(),
              [](const game::EventScript& a, const game::EventScript& b)
              { return a.eventId < b.eventId; });

    return scripts;
}

uint16_t EvtScriptParser::readU16(const std::vector<uint8_t>& data, size_t offset)
{
    return static_cast<uint16_t>(data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8));
}

int32_t EvtScriptParser::readI32(const std::vector<uint8_t>& data, size_t offset)
{
    uint32_t value = static_cast<uint32_t>(data[offset]) |
                     (static_cast<uint32_t>(data[offset + 1]) << 8) |
                     (static_cast<uint32_t>(data[offset + 2]) << 16) |
                     (static_cast<uint32_t>(data[offset + 3]) << 24);
    return static_cast<int32_t>(value);
}

std::string EvtScriptParser::readCString(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset >= bytes.size())
    {
        return {};
    }

    size_t end = offset;
    while (end < bytes.size() && bytes[end] != 0)
    {
        end++;
    }

    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), end - offset);
}

std::string EvtScriptParser::trimLeadingSpaces(std::string text)
{
    size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
    {
        i++;
    }
    return text.substr(i);
}

game::EventCommand EvtScriptParser::decodeCommand(uint8_t opcodeByte,
                                                  const std::vector<uint8_t>& params,
                                                  const std::vector<std::string>& strings) const
{
    game::EventCommand cmd;
    cmd.opcode = static_cast<game::EventOpcode>(opcodeByte);

    if (params.size() >= 4)
    {
        cmd.param1 = readI32(params, 0);
    }
    if (params.size() >= 8)
    {
        cmd.param2 = readI32(params, 4);
    }
    if (params.size() >= 12)
    {
        cmd.param3 = readI32(params, 8);
    }

    auto resolveStringByIndex = [&](int index) -> std::string
    {
        if (index <= 0 || static_cast<size_t>(index) >= strings.size())
        {
            return {};
        }
        return strings[static_cast<size_t>(index)];
    };

    switch (cmd.opcode)
    {
    case game::EventOpcode::ShowText:
    {
        cmd.text = resolveStringByIndex(cmd.param1);
        if (cmd.text.empty())
        {
            cmd.text = resolveStringByIndex(cmd.param2);
        }
        if (cmd.text.empty())
        {
            cmd.text = resolveStringByIndex(cmd.param3);
        }
        if (params.size() >= 13)
        {
            cmd.param4 = params[12]; // jump target
        }
        break;
    }

    case game::EventOpcode::NpcDialog:
    case game::EventOpcode::StatusMessage:
        cmd.text = resolveStringByIndex(cmd.param1);
        break;

    case game::EventOpcode::SetNpcName:
        cmd.text = resolveStringByIndex(cmd.param1);
        break;

    case game::EventOpcode::ModifyNpc:
    case game::EventOpcode::ModifyNpcEx:
        // +7:type(u8), followed by u32 parameter block.
        if (!params.empty())
        {
            cmd.param1 = params[0]; // NPC modifier type
        }
        if (params.size() >= 5)
        {
            cmd.param2 = readI32(params, 1);
        }
        if (params.size() >= 9)
        {
            cmd.param3 = readI32(params, 5);
        }
        if (params.size() >= 13)
        {
            cmd.param4 = readI32(params, 9);
        }
        if (params.size() >= 17)
        {
            cmd.param5 = readI32(params, 13);
        }
        if (params.size() >= 21)
        {
            cmd.param6 = readI32(params, 17);
        }
        if (cmd.opcode == game::EventOpcode::ModifyNpcEx && params.size() >= 25)
        {
            cmd.i64param = static_cast<int64_t>(readI32(params, 21));
        }
        break;

    case game::EventOpcode::SetPlayerSelect:
    case game::EventOpcode::JumpToEvent:
        if (!params.empty())
        {
            cmd.param1 = params[0];
        }
        break;

    case game::EventOpcode::SetFlag:
        // +5:flag type (u8), +6:flag value (u8)
        if (params.size() >= 1)
        {
            cmd.param1 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param2 = params[1];
        }
        break;

    case game::EventOpcode::CheckFlag:
        // Compact layout used in EVT scripts: +5:flag(u8), +6:value(u8), +7:jump(u8)
        if (params.size() >= 1)
        {
            cmd.param1 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param2 = params[1];
        }
        if (params.size() >= 3)
        {
            cmd.param3 = params[2];
        }
        // Fallback for wider variants.
        if (params.size() >= 12)
        {
            cmd.param1 = readI32(params, 0);
            cmd.param2 = readI32(params, 4);
            cmd.param3 = readI32(params, 8);
        }
        break;

    case game::EventOpcode::SetPlayerVar:
        // +5:player selector (u8), +6:variable/stat id (u8)
        // Original opcode semantics are broader; we preserve selector in param4
        // and default value to 1 when no explicit payload is present.
        if (params.size() >= 2)
        {
            cmd.param4 = params[0];
            cmd.param5 = 1; // selector was explicitly encoded in EVT payload
            cmd.param1 = params[1];
            cmd.param2 = 1;
        }
        if (params.size() >= 6)
        {
            cmd.param2 = readI32(params, 2);
        }
        break;

    case game::EventOpcode::AddStat:
    case game::EventOpcode::SubtractStat:
        // +5:stat type(u16) +7:value(u32)
        if (params.size() >= 2)
        {
            cmd.param1 = static_cast<int>(readU16(params, 0));
        }
        if (params.size() >= 6)
        {
            cmd.param2 = readI32(params, 2);
        }
        break;

    case game::EventOpcode::GiveGold:
    case game::EventOpcode::TakeGold:
    case game::EventOpcode::GiveExperience:
        // Original handlers read payload from command offset +0xD (params[8]),
        // while some scripts use a compact u32 at params[0].
        if (params.size() >= 12)
        {
            cmd.param1 = readI32(params, 8);
        }
        else if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0);
        }
        else if (!params.empty())
        {
            cmd.param1 = params[0];
        }
        break;

    case game::EventOpcode::SetGlobalVar:
        // +5:var index (u32), +9:subfield (u8), +10:value (u32)
        if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0);
        }
        if (params.size() >= 5)
        {
            cmd.param4 = params[4];
        }
        if (params.size() >= 9)
        {
            cmd.param2 = readI32(params, 5);
        }
        break;

    case game::EventOpcode::SetGlobalVar2:
        // +5:var index (u32), +9:value (u32)
        if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0);
        }
        if (params.size() >= 8)
        {
            cmd.param2 = readI32(params, 4);
        }
        break;

    case game::EventOpcode::CastSpell:
        // +5:school(u8) +6:spell(u8) +7:power(u32)
        if (params.size() >= 1)
        {
            cmd.param1 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param2 = params[1];
        }
        if (params.size() >= 6)
        {
            cmd.param3 = readI32(params, 2);
        }
        break;

    case game::EventOpcode::DoorControl:
        // MM7 scripts often use compact [target, action] dwords here.
        // Some command variants expose a control byte at +0xD (params[8]).
        cmd.param1 = -1;
        cmd.param2 = 1;
        if (params.size() >= 8)
        {
            cmd.param1 = readI32(params, 0);
            cmd.param2 = readI32(params, 4);
        }
        else if (params.size() >= 4)
        {
            cmd.param2 = readI32(params, 0);
        }
        else if (!params.empty())
        {
            cmd.param2 = params[0];
        }
        if (params.size() >= 9)
        {
            cmd.param2 = params[8];
        }
        break;

    case game::EventOpcode::ModifyObject:
        // Opcode reference notes data payload at command +10 (params[5]).
        // We preserve optional explicit target id from params[0..3] when present.
        cmd.param1 = -1;
        cmd.param2 = 1;
        if (params.size() >= 8)
        {
            cmd.param1 = readI32(params, 0);
        }
        if (params.size() >= 6)
        {
            cmd.param2 = params[5];
        }
        else if (params.size() >= 4)
        {
            cmd.param2 = readI32(params, 0);
        }
        else if (!params.empty())
        {
            cmd.param2 = params[0];
        }
        break;

    case game::EventOpcode::ModifyDecoration:
        // +5:action(u32). Some synthetic commands also encode explicit target id.
        cmd.param1 = -1;
        cmd.param2 = 1;
        if (params.size() >= 4)
        {
            cmd.param2 = readI32(params, 0);
        }
        else if (!params.empty())
        {
            cmd.param2 = params[0];
        }
        if (params.size() >= 8)
        {
            cmd.param1 = readI32(params, 4);
        }
        break;

    case game::EventOpcode::ShowEffect:
    case game::EventOpcode::PlayAnimation:
        // RE layout references +0xD byte in command body; keep a compact fallback too.
        if (params.size() >= 9)
        {
            cmd.param1 = params[8];
        }
        else if (!params.empty())
        {
            cmd.param1 = params[0];
        }
        break;

    case game::EventOpcode::RandomGoto:
        if (params.size() >= 1)
            cmd.param1 = params[0];
        if (params.size() >= 2)
            cmd.param2 = params[1];
        if (params.size() >= 3)
            cmd.param3 = params[2];
        if (params.size() >= 4)
            cmd.param4 = params[3];
        if (params.size() >= 5)
            cmd.param5 = params[4];
        if (params.size() >= 6)
            cmd.param6 = params[5];
        break;

    case game::EventOpcode::CheckSkill:
        if (params.size() >= 1)
        {
            cmd.param1 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param4 = params[1]; // required mastery
        }
        if (params.size() >= 6)
        {
            cmd.param2 = readI32(params, 2); // required skill level
        }
        if (params.size() >= 7)
        {
            cmd.param3 = params[6]; // jump target
        }
        break;

    case game::EventOpcode::CheckCondition:
        // +5:condition(u16) +7:value(u32) +0xB:jump(u8)
        if (params.size() >= 2)
        {
            const uint16_t packed = readU16(params, 0);
            cmd.param1 = static_cast<int>(packed & 0x00FF);        // condition type
            cmd.param4 = static_cast<int>((packed >> 8) & 0x00FF); // subtype
        }
        if (params.size() >= 6)
        {
            cmd.param2 = readI32(params, 2); // comparison value
        }
        if (params.size() >= 7)
        {
            cmd.param3 = params[6]; // jump target on success
        }
        break;

    case game::EventOpcode::Teleport:
        // +5:X +9:Y +D:Z +11:yaw ... +1F:map
        if (params.size() >= 16)
        {
            cmd.fparam = static_cast<float>(readI32(params, 0));
            cmd.fparam2 = static_cast<float>(readI32(params, 4));
            cmd.fparam3 = static_cast<float>(readI32(params, 8));
            cmd.param1 = readI32(params, 12);
        }
        if (params.size() >= 24)
        {
            cmd.param2 = readI32(params, 16); // pitch
            cmd.param3 = readI32(params, 20); // viewZ
        }
        if (params.size() > 26)
        {
            cmd.text = readCString(params, 26);
        }
        break;

    case game::EventOpcode::ChangeMap:
        if (params.size() >= 1)
        {
            cmd.param1 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param2 = params[1];
        }
        if (params.size() > 2)
        {
            cmd.text = readCString(params, 2);
            while (!cmd.text.empty() && std::isspace(static_cast<unsigned char>(cmd.text.back())))
            {
                cmd.text.pop_back();
            }
        }
        break;

    case game::EventOpcode::GiveItem:
        // +5:player selector (u8), +6:sub-param (u8), +7:item data (u32)
        if (params.size() >= 1)
        {
            cmd.param4 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param5 = params[1];
        }
        // Keep item id in param1 for downstream inventory callbacks.
        if (params.size() >= 6)
        {
            cmd.param1 = readI32(params, 2);
        }
        else if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0);
        }
        break;

    case game::EventOpcode::RemoveItem:
        // +5:item type (u16), +7:item id (u32)
        if (params.size() >= 2)
        {
            cmd.param4 = readU16(params, 0); // item type/category
        }
        if (params.size() >= 6)
        {
            cmd.param1 = readI32(params, 2); // item id
        }
        break;

    case game::EventOpcode::CureCondition:
        if (params.size() >= 2)
        {
            cmd.param1 = params[0];
            cmd.param2 = params[1];
        }
        break;

    case game::EventOpcode::SpawnItem:
        if (params.size() >= 20)
        {
            cmd.fparam = static_cast<float>(readI32(params, 8));
            cmd.fparam2 = static_cast<float>(readI32(params, 12));
            cmd.fparam3 = static_cast<float>(readI32(params, 16));
        }
        if (params.size() >= 26)
        {
            cmd.param1 = params[24]; // item type
            cmd.param2 = params[25]; // count
        }
        break;

    case game::EventOpcode::SetMonsterTopic:
        if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0); // monster index
        }
        if (params.size() >= 6)
        {
            cmd.param2 = params[4] | (static_cast<int>(params[5]) << 8); // topic
        }
        break;

    case game::EventOpcode::SetMonsterField:
        if (params.size() >= 4)
        {
            // 24-bit monster id + 8-bit field index.
            cmd.param1 = static_cast<int>(params[0]) | (static_cast<int>(params[1]) << 8) |
                         (static_cast<int>(params[2]) << 16);
            cmd.param2 = params[3];
        }
        if (params.size() >= 8)
        {
            cmd.param3 = readI32(params, 4);
        }
        break;

    case game::EventOpcode::SetMonsterHostile:
    case game::EventOpcode::SetHostileByIdx:
        // +5: target id/group (u32), +0xD: hostility byte
        if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0);
        }
        if (params.size() >= 9)
        {
            cmd.param2 = params[8];
        }
        else
        {
            // Keep historical behavior for compact synthetic commands.
            cmd.param2 = 1;
        }
        break;

    case game::EventOpcode::CheckMapVar:
        // +5:var(u32) ... +10:value(u8) +0xB:jump(u8)
        if (params.size() >= 4)
        {
            cmd.param1 = readI32(params, 0);
        }
        if (params.size() >= 6)
        {
            cmd.param2 = params[5];
        }
        if (params.size() >= 7)
        {
            cmd.param3 = params[6];
        }
        break;

    case game::EventOpcode::CheckTime:
        // Range layout: +5=minHour +6=maxHour +7=jumpTarget.
        if (params.size() >= 3)
        {
            cmd.param1 = params[0];
            cmd.param2 = params[1];
            cmd.param3 = params[2];
            break;
        }
        // Compact fallback: hour==value branch.
        if (params.size() >= 1)
        {
            cmd.param1 = params[0];
            cmd.param2 = params[0];
        }
        if (params.size() >= 2)
        {
            cmd.param3 = params[1];
        }
        break;

    case game::EventOpcode::TriggerTimerAbsolute:
    case game::EventOpcode::TriggerTimerPeriodic:
        // Timer payload does not carry an event-id target in MM7 layout.
        // Keep parser defaults from generic i32 reads from affecting runtime scheduling.
        cmd.param1 = 0;
        cmd.param2 = 0;
        cmd.param3 = 0;
        // +5:years +6:months +7:weeks +8:days +9:hours +A:minutes +B:seconds(u16)
        if (params.size() >= 8)
        {
            const int64_t years = params[0];
            const int64_t months = params[1];
            const int64_t weeks = params[2];
            const int64_t days = params[3];
            const int64_t hours = params[4];
            const int64_t minutes = params[5];
            const int64_t seconds = static_cast<int64_t>(readU16(params, 6));

            constexpr int64_t kSecondsPerMinute = 60;
            constexpr int64_t kSecondsPerHour = 60 * 60;
            constexpr int64_t kSecondsPerDay = 24 * 60 * 60;
            constexpr int64_t kSecondsPerWeek = 7 * kSecondsPerDay;
            constexpr int64_t kSecondsPerMonth = 4 * kSecondsPerWeek;
            constexpr int64_t kSecondsPerYear = 12 * kSecondsPerMonth;

            const int64_t totalSeconds = years * kSecondsPerYear + months * kSecondsPerMonth +
                                         weeks * kSecondsPerWeek + days * kSecondsPerDay +
                                         hours * kSecondsPerHour + minutes * kSecondsPerMinute +
                                         seconds;
            cmd.i64param = totalSeconds * 128;
        }
        break;

    default:
        break;
    }

    return cmd;
}

} // namespace runeharbor::formats
