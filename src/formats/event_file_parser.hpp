// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct EventEntry
{
    int mainId = 0;
    int subId = 0;
    std::string type;
    int mapId = 0;
    int pictureId = 0;
    std::string name;
    std::string proprietorName;
    std::string title;
    int field9_int = 0;
    int field10_int = 0;
    int field11_int = 0;
    int field12_int = 0;
    float field13_float = 0.0f;
    int field14_int = 0;
    std::string field15_str;
    int field16_int = 0;
    std::string field17_str;
    std::string field18_str;
    int field19_int = 0;
    int field20_int = 0;
    int field21_int = 0;
    int field22_int = 0;
    std::string field23_str;
    std::string field24_str;
    int field25_int = 0;
    int field26_int = 0;
    int field27_int = 0;
    int field28_int = 0;
    int field29_int = 0;
    int field30_int = 0;
};

class EventFileParser
{
  public:
    explicit EventFileParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<EventEntry>& getEvents() const { return events; }

  private:
    util::ILogger& logger;
    std::vector<EventEntry> events;
};

} // namespace runeharbor::formats
