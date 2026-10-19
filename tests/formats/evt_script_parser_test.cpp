// SPDX-License-Identifier: MIT
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/evt_script_parser.hpp"
#include "../../src/util/console_logger.hpp"

using namespace runeharbor;
using namespace runeharbor::formats;
using namespace runeharbor::game;

TEST_CASE("EvtScriptParser decodes GiveAward kind byte", "[formats][evt]")
{
    util::ConsoleLogger logger;
    EvtScriptParser parser(logger);

    // Command: [size=9][eventId=7 (u16)][sub=0][opcode=0x20]
    //          [id=1 (u32)][kind=1 (u8)]
    const std::vector<uint8_t> evtData = {
        9, 7, 0, 0, 0x20, 1, 0, 0, 0, 1,
    };

    const auto scripts = parser.parseEventData(evtData, {});
    REQUIRE(scripts.size() == 1);
    REQUIRE(scripts[0].eventId == 7);
    REQUIRE(scripts[0].commands.size() == 1);

    const auto& command = scripts[0].commands[0];
    CHECK(command.opcode == EventOpcode::GiveAward);
    CHECK(command.param1 == 1);
    CHECK(command.param2 == 1);
}
