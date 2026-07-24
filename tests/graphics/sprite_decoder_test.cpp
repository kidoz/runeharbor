// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>

#include "../../src/graphics/sprite_decoder.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor;
using namespace runeharbor::graphics;
using namespace runeharbor::util;

namespace
{

class TestLogger : public ILogger
{
  public:
    void log(LogLevel level, std::string_view message) override
    {
        lastLevel = level;
        lastMessage = std::string(message);
    }

    LogLevel lastLevel = LogLevel::Info;
    std::string lastMessage;
};

void appendLe16(std::vector<uint8_t>& data, uint16_t value)
{
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

} // namespace

TEST_CASE("SpriteDecoder decodes line metadata without aligned loads", "[sprite_decoder]")
{
    TestLogger logger;
    Palette palette;
    palette.setColor(1, Palette::Color(10, 20, 30));
    palette.setColor(2, Palette::Color(40, 50, 60));

    std::vector<uint8_t> data;
    appendLe16(data, 2); // width
    appendLe16(data, 2); // height
    appendLe16(data, 0); // centerX
    appendLe16(data, 0); // centerY
    appendLe16(data, 0); // paletteId

    appendLe16(data, 0); // line 0 left pixel
    appendLe16(data, 1); // line 0 right pixel, inclusive
    appendLe16(data, 0); // line 0 pixel data offset
    appendLe16(data, 0); // reserved

    appendLe16(data, 0xFFFF); // line 1 empty marker
    appendLe16(data, 0xFFFF);
    appendLe16(data, 0);
    appendLe16(data, 0);

    data.push_back(1);
    data.push_back(2);

    auto image = SpriteDecoder::decode(data, palette, logger);
    REQUIRE(image);
    REQUIRE(image->getWidth() == 2);
    REQUIRE(image->getHeight() == 2);

    const auto& rgba = image->getRGBAData();
    REQUIRE(rgba[0] == 10);
    REQUIRE(rgba[1] == 20);
    REQUIRE(rgba[2] == 30);
    REQUIRE(rgba[3] == 255);

    REQUIRE(rgba[4] == 40);
    REQUIRE(rgba[5] == 50);
    REQUIRE(rgba[6] == 60);
    REQUIRE(rgba[7] == 255);

    REQUIRE(rgba[8] == 0);
    REQUIRE(rgba[9] == 0);
    REQUIRE(rgba[10] == 0);
    REQUIRE(rgba[11] == 0);
}
