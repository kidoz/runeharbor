// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "../formats/blv_map.hpp"
#include "../formats/mapstats_parser.hpp"
#include "../formats/odm_map.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::game
{

struct GeneratedMonster
{
    int monsterId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int group = 0;
};

struct GeneratedChest
{
    int chestId = 0;
    std::vector<int> itemIds;
};

struct GeneratedMapContent
{
    uint64_t seed = 0;
    int64_t generatedAtTicks = 0;
    int32_t respawnDays = 0;
    std::vector<GeneratedMonster> monsters;
    std::vector<GeneratedChest> chests;
};

struct GenerationConfig
{
    int mapDifficulty = 1; // 1..10
    int maxMonsters = 96;
    int maxChests = 32;
};

struct TreasureItemWeight
{
    int itemId = 0;
    int baseLevel = 0;
    std::array<int, 6> levelWeights = {};
};

struct MonsterPlacementEntry
{
    int monsterId = 0;
    int weight = 0;
};

struct MonsterPlacementRule
{
    std::string mapName;
    int minDifficulty = 1;
    int maxDifficulty = 10;
    std::vector<MonsterPlacementEntry> entries;
};

class ContentGenerator
{
  public:
    explicit ContentGenerator(util::ILogger& logger);

    void setWorldSeed(uint64_t seed) { worldSeed_ = seed; }
    uint64_t worldSeed() const { return worldSeed_; }
    void setTreasureItemWeights(std::vector<TreasureItemWeight> weights);
    void setMonsterPlacementRules(std::vector<MonsterPlacementRule> rules);

    GeneratedMapContent generateForMap(const std::string& mapName,
                                       const std::vector<formats::BLVSpawnPoint>& indoorSpawns,
                                       const std::vector<formats::ODMSpawnPoint>& outdoorSpawns,
                                       const GenerationConfig& config) const;

    static int estimateDifficultyFromMapStats(const formats::MapStatsEntry& stats);

  private:
    int pickMonsterIdFromSpawn(std::string_view mapName, uint16_t objectIndex, int mapDifficulty,
                               uint64_t salt) const;
    int pickTreasureItemId(int treasureLevel, std::mt19937& rng) const;
    std::vector<int> generateChestItems(int mapDifficulty, uint64_t salt) const;

    util::ILogger& logger_;
    uint64_t worldSeed_ = 0x4D4D3701u;
    std::vector<TreasureItemWeight> treasureItemWeights_;
    std::vector<MonsterPlacementRule> monsterPlacementRules_;
};

} // namespace runeharbor::game
