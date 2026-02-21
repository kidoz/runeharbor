// SPDX-License-Identifier: MIT
#include "generation.hpp"

#include <algorithm>
#include <random>
#include <utility>

#include <cctype>

namespace runeharbor::game
{

namespace
{
std::string toLower(std::string_view s)
{
    std::string result;
    result.reserve(s.size());
    for (char c : s)
    {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

std::string mapBaseName(std::string_view s)
{
    std::string lower = toLower(s);
    const size_t dot = lower.find_last_of('.');
    if (dot == std::string::npos)
    {
        return lower;
    }
    return lower.substr(0, dot);
}

uint64_t stableMix(uint64_t a, uint64_t b)
{
    uint64_t x = a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
} // namespace

ContentGenerator::ContentGenerator(util::ILogger& logger) : logger_(logger) {}

void ContentGenerator::setTreasureItemWeights(std::vector<TreasureItemWeight> weights)
{
    treasureItemWeights_ = std::move(weights);
}

void ContentGenerator::setMonsterPlacementRules(std::vector<MonsterPlacementRule> rules)
{
    monsterPlacementRules_ = std::move(rules);
}

GeneratedMapContent ContentGenerator::generateForMap(
    const std::string& mapName, const std::vector<formats::BLVSpawnPoint>& indoorSpawns,
    const std::vector<formats::ODMSpawnPoint>& outdoorSpawns, const GenerationConfig& config) const
{
    GeneratedMapContent content;
    uint64_t mapHash = static_cast<uint64_t>(std::hash<std::string>{}(mapName));
    content.seed = stableMix(worldSeed_, mapHash);

    std::mt19937 rng(static_cast<uint32_t>(content.seed ^ (content.seed >> 32)));
    std::uniform_int_distribution<int> roll100(1, 100);
    std::uniform_real_distribution<float> jitter(-96.0f, 96.0f);

    const int difficulty = std::clamp(config.mapDifficulty, 1, 10);
    const int maxMonsters = std::max(0, config.maxMonsters);
    const int maxChests = std::max(0, config.maxChests);

    auto addSpawn = [&](float x, float y, float z, uint16_t objectType, uint16_t objectIndex,
                        int group, uint64_t salt)
    {
        if (static_cast<int>(content.monsters.size()) >= maxMonsters)
        {
            return;
        }

        // Keep generation deterministic but avoid over-populating every marker.
        const int chance = std::clamp(30 + difficulty * 6, 20, 95);
        if (roll100(rng) > chance)
        {
            return;
        }

        GeneratedMonster monster;
        monster.monsterId = pickMonsterIdFromSpawn(mapName, objectIndex, difficulty, salt);
        monster.x = x + jitter(rng);
        monster.y = y;
        monster.z = z + jitter(rng);
        monster.group = group;
        content.monsters.push_back(monster);

        // Heuristic chest generation from special spawn/object types.
        if (static_cast<int>(content.chests.size()) < maxChests &&
            (objectType == 0x16u || objectType == 0x17u || objectType >= 500u))
        {
            GeneratedChest chest;
            chest.chestId = static_cast<int>(content.chests.size());
            chest.itemIds = generateChestItems(difficulty, salt ^ 0xC8E57ULL);
            content.chests.push_back(std::move(chest));
        }
    };

    for (size_t i = 0; i < indoorSpawns.size(); i++)
    {
        const auto& s = indoorSpawns[i];
        addSpawn(static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z),
                 s.objectType, s.objectIndex, static_cast<int>(s.group),
                 stableMix(content.seed, static_cast<uint64_t>(i + 1)));
    }

    for (size_t i = 0; i < outdoorSpawns.size(); i++)
    {
        const auto& s = outdoorSpawns[i];
        addSpawn(static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z),
                 s.objectType, s.objectIndex, static_cast<int>(s.group),
                 stableMix(content.seed ^ 0x0D4D4D4DULL, static_cast<uint64_t>(i + 1)));
    }

    // Ensure at least one chest exists on first generation in higher difficulty maps.
    if (content.chests.empty() && difficulty >= 4 && maxChests > 0)
    {
        GeneratedChest chest;
        chest.chestId = 0;
        chest.itemIds = generateChestItems(difficulty, content.seed ^ 0xABCD1234ULL);
        content.chests.push_back(std::move(chest));
    }

    logger_.info("Generated content for map '" + mapName +
                 "': " + std::to_string(content.monsters.size()) + " monsters, " +
                 std::to_string(content.chests.size()) + " chests");

    return content;
}

int ContentGenerator::pickMonsterIdFromSpawn(std::string_view mapName, uint16_t objectIndex,
                                             int mapDifficulty, uint64_t salt) const
{
    if (objectIndex > 0)
    {
        return static_cast<int>(objectIndex);
    }

    if (!monsterPlacementRules_.empty())
    {
        const std::string mapLower = toLower(mapName);
        const std::string mapBase = mapBaseName(mapName);

        struct WeightedMonster
        {
            int monsterId = 0;
            int weight = 0;
        };
        std::vector<WeightedMonster> weighted;

        for (const auto& rule : monsterPlacementRules_)
        {
            const bool mapMatch = rule.mapName.empty() || rule.mapName == "*" ||
                                  rule.mapName == "all" || toLower(rule.mapName) == mapLower ||
                                  mapBaseName(rule.mapName) == mapBase;
            if (!mapMatch)
            {
                continue;
            }
            if (mapDifficulty < rule.minDifficulty || mapDifficulty > rule.maxDifficulty)
            {
                continue;
            }

            for (const auto& entry : rule.entries)
            {
                if (entry.monsterId <= 0 || entry.weight <= 0)
                {
                    continue;
                }
                weighted.push_back({entry.monsterId, entry.weight});
            }
        }

        int totalWeight = 0;
        for (const auto& entry : weighted)
        {
            totalWeight += entry.weight;
        }

        if (totalWeight > 0)
        {
            std::mt19937 rng(static_cast<uint32_t>(salt ^ (salt >> 32)));
            std::uniform_int_distribution<int> pick(1, totalWeight);
            int roll = pick(rng);
            for (const auto& entry : weighted)
            {
                roll -= entry.weight;
                if (roll <= 0)
                {
                    return entry.monsterId;
                }
            }
        }
    }

    std::mt19937 rng(static_cast<uint32_t>(salt ^ (salt >> 32)));
    int tierBase = (std::clamp(mapDifficulty, 1, 10) - 1) * 10;
    std::uniform_int_distribution<int> pick(1 + tierBase, 10 + tierBase);
    return pick(rng);
}

int ContentGenerator::pickTreasureItemId(int treasureLevel, std::mt19937& rng) const
{
    if (treasureItemWeights_.empty())
    {
        return 0;
    }

    const int levelIdx = std::clamp(treasureLevel, 1, 6) - 1;
    int totalWeight = 0;
    for (const auto& item : treasureItemWeights_)
    {
        totalWeight += std::max(0, item.levelWeights[static_cast<size_t>(levelIdx)]);
    }

    if (totalWeight <= 0)
    {
        return 0;
    }

    std::uniform_int_distribution<int> pick(1, totalWeight);
    int roll = pick(rng);
    for (const auto& item : treasureItemWeights_)
    {
        roll -= std::max(0, item.levelWeights[static_cast<size_t>(levelIdx)]);
        if (roll <= 0)
        {
            return item.itemId;
        }
    }

    return 0;
}

std::vector<int> ContentGenerator::generateChestItems(int mapDifficulty, uint64_t salt) const
{
    std::mt19937 rng(static_cast<uint32_t>(salt ^ (salt >> 33)));
    const int difficulty = std::clamp(mapDifficulty, 1, 10);
    const int treasureLevel = std::clamp(1 + (difficulty - 1) / 2, 1, 6);
    std::uniform_int_distribution<int> countRoll(1, std::min(5, 1 + difficulty / 2));
    std::uniform_int_distribution<int> common(1, 120);
    std::uniform_int_distribution<int> rare(121, 220);
    std::uniform_int_distribution<int> epic(221, 320);
    std::uniform_int_distribution<int> chance(1, 100);

    int count = std::max(1, countRoll(rng));
    std::vector<int> result;
    result.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; i++)
    {
        if (const int weighted = pickTreasureItemId(treasureLevel, rng); weighted > 0)
        {
            result.push_back(weighted);
        }
        else
        {
            int roll = chance(rng) + difficulty * 3;
            if (roll > 115)
            {
                result.push_back(epic(rng));
            }
            else if (roll > 75)
            {
                result.push_back(rare(rng));
            }
            else
            {
                result.push_back(common(rng));
            }
        }
    }

    return result;
}

int ContentGenerator::estimateDifficultyFromMapStats(const formats::MapStatsEntry& stats)
{
    int score = 0;

    // Security/lock/trap profile
    score += std::clamp(stats.lock, 0, 10);
    score += std::clamp(stats.trap * 2, 0, 20);
    score += std::clamp(stats.steal / 2, 0, 10);

    // Encounter and monster pressure
    score += std::clamp(stats.enc / 10, 0, 10);
    score += std::clamp(stats.m1 / 10, 0, 10);
    score += std::clamp(stats.m2 / 10, 0, 10);

    // Economy pressure (treasure/respawn/perm can indirectly indicate challenge pacing)
    score += std::clamp(stats.tres / 25, 0, 4);
    score += std::clamp(stats.per / 5, 0, 4);
    score += std::clamp(stats.perm / 5, 0, 4);

    // Convert to 1..10 range.
    int difficulty = 1 + score / 7;
    return std::clamp(difficulty, 1, 10);
}

} // namespace runeharbor::game
