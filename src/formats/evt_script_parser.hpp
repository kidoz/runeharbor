// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../game/event_engine.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

class EvtScriptParser
{
  public:
    explicit EvtScriptParser(util::ILogger& logger);

    // Parse null-separated map string table (.str). Index 0 is always empty so event string IDs are
    // naturally 1-based.
    std::vector<std::string> parseStringTable(const std::vector<uint8_t>& strData) const;

    // Parse binary event bytecode (.evt) into high-level scripts.
    std::vector<game::EventScript>
    parseEventData(const std::vector<uint8_t>& evtData,
                   const std::vector<std::string>& strings = {}) const;

  private:
    static uint16_t readU16(const std::vector<uint8_t>& data, size_t offset);
    static int32_t readI32(const std::vector<uint8_t>& data, size_t offset);

    static std::string readCString(const std::vector<uint8_t>& bytes, size_t offset);
    static std::string trimLeadingSpaces(std::string text);

    game::EventCommand decodeCommand(uint8_t opcodeByte, const std::vector<uint8_t>& params,
                                     const std::vector<std::string>& strings) const;

    util::ILogger& logger_;
};

} // namespace runeharbor::formats
