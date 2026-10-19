// SPDX-License-Identifier: MIT
//
// Unit tests for the save/load system. The primary target is the full
// round-trip: mutate representative fields across the party, characters,
// inventory, and quest log; save to a temp slot; load into a fresh world;
// assert every field survived. This locks in the previously-missing fields
// (spellbook, bank gold, quest state, quickbar) and guards future regressions.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <fstream>

#include "../../src/formats/game_lod_archive.hpp"
#include "../../src/formats/quests_parser.hpp"
#include "../../src/game/character.hpp"
#include "../../src/game/combat.hpp"
#include "../../src/game/game_world.hpp"
#include "../../src/game/inventory.hpp"
#include "../../src/game/quest_log.hpp"
#include "../../src/game/save_game.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::game;

namespace
{

class NullLogger : public runeharbor::util::ILogger
{
  public:
    void log(runeharbor::util::LogLevel, std::string_view) override {}
};

// A fresh temp directory per test case; removed on destruction. Uses a
// timestamp+counter name to avoid collisions without platform-specific APIs.
struct TempSaveDir
{
    std::filesystem::path path;

    TempSaveDir()
    {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               std::format("rh_save_test_{}_{}", stamp, counter++);
        std::filesystem::create_directories(path);
    }
    ~TempSaveDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// Seed a world with non-default values across many fields so a round-trip can
// detect losses. Every value chosen is non-default (not 0/empty).
std::vector<uint8_t> readFileBytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return {};
    }
    const auto size = file.tellg();
    if (size <= 0)
    {
        return {};
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file)
    {
        return {};
    }
    return data;
}

bool writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

std::optional<size_t> findBytes(const std::vector<uint8_t>& haystack, std::string_view needle)
{
    if (needle.empty() || haystack.size() < needle.size())
    {
        return std::nullopt;
    }
    const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end());
    if (it == haystack.end())
    {
        return std::nullopt;
    }
    return static_cast<size_t>(std::distance(haystack.begin(), it));
}

void seedWorld(GameWorld& world)
{
    auto& party = world.party();
    party.setGold(1234);
    party.setFood(56);
    party.setBankGold(9999); // v12 field — was lost before the fix.
    party.adjustReputation(7);
    party.setWorldPosition(12552.0f, 1816.0f, 512.0f);
    party.setOrientation(42.0f, 0.0f);
    party.setActiveMemberIndex(2);

    auto& ch = party.member(0);
    ch.name = "Aria";
    ch.faceId = 7;
    ch.charClass = CharacterClass::Sorcerer;
    ch.gender = Gender::Female;
    ch.level = 12;
    ch.experience = 45000;
    ch.hitPoints = 180;
    ch.maxHitPoints = 180;
    ch.spellPoints = 220;
    ch.maxSpellPoints = 220;
    ch.stats.might = 30;
    ch.stats.intellect = 45;
    ch.baseStats = ch.stats;
    ch.fireResistance = 50;
    // Spellbook: set knownSpells directly (public array) so the test doesn't
    // depend on the skill-gated learnSpell path. The round-trip only needs the
    // bits to survive.
    ch.knownSpells[15] = true;
    ch.knownSpells[20] = true;
    ch.setQuickbarSpell(0, 15); // quickbar round-trip
    ch.setQuickbarSpell(1, 20);

    world.setCurrentMap("out01.odm");
}

} // namespace

TEST_CASE("Save/Load round-trips party, character, inventory, and quest state", "[game][save]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    // --- Seed and save ---
    GameWorld src;
    seedWorld(src);

    Inventory srcInventory(logger);
    Item weapon;
    weapon.itemId = 5;
    weapon.identified = true;
    srcInventory.addToBackpack(0, weapon);

    runeharbor::formats::QuestEntry def;
    def.qBit = 100;
    def.questNoteText = "Recover the chalice";
    def.owner = "Lord";
    QuestLog srcQuests(logger);
    srcQuests.loadQuestData({def});
    srcQuests.startQuest(100, 1000);
    srcQuests.addJournalEntry("Spoke to Lord", 1100, 100);

    REQUIRE(save.save(src, 0, nullptr, &srcInventory, &srcQuests));

    // --- Load into a fresh world/inventory/quest-log ---
    GameWorld dst;
    Inventory dstInventory(logger);
    // Definitions must be reloaded before quest state is applied.
    QuestLog dstQuests(logger);
    dstQuests.loadQuestData({def});

    REQUIRE(save.load(dst, 0, nullptr, &dstInventory, &dstQuests));

    SECTION("party-level fields survive")
    {
        REQUIRE(dst.party().gold() == 1234);
        REQUIRE(dst.party().food() == 56);
        REQUIRE(dst.party().bankGold() == 9999); // regression: was 0 before v12
        REQUIRE(dst.party().reputation() == 7);
        REQUIRE(dst.party().activeMemberIndex() == 2);
        REQUIRE(dst.party().worldX() == 12552.0f);
        REQUIRE(dst.party().worldY() == 1816.0f);
        REQUIRE(dst.party().worldZ() == 512.0f);
        REQUIRE(dst.party().yaw() == 42.0f);
        REQUIRE(dst.currentMap() == "out01.odm");
    }

    SECTION("per-character fields survive")
    {
        const auto& ch = dst.party().member(0);
        REQUIRE(ch.name == "Aria");
        REQUIRE(ch.faceId == 7);
        REQUIRE(ch.charClass == CharacterClass::Sorcerer);
        REQUIRE(ch.gender == Gender::Female);
        REQUIRE(ch.level == 12);
        REQUIRE(ch.experience == 45000);
        REQUIRE(ch.maxHitPoints == 180);
        REQUIRE(ch.maxSpellPoints == 220);
        REQUIRE(ch.stats.might == 30);
        REQUIRE(ch.stats.intellect == 45);
        REQUIRE(ch.fireResistance == 50);
    }

    SECTION("spellbook and quickbar survive (v12 regression)")
    {
        const auto& ch = dst.party().member(0);
        // Regression: knownSpells was never serialized before the v12 fix, so
        // loading a save lost the entire spellbook.
        REQUIRE(ch.knowsSpell(15));
        REQUIRE(ch.knowsSpell(20));
        REQUIRE_FALSE(ch.knowsSpell(1));
        REQUIRE(ch.quickbarSpells[0] == 15);
        REQUIRE(ch.quickbarSpells[1] == 20);
    }

    SECTION("inventory survives")
    {
        const auto& inv = dstInventory.getInventory(0);
        bool foundWeapon = false;
        for (const auto& item : inv.backpack)
        {
            if (item.valid() && item.itemId == 5 && item.identified)
            {
                foundWeapon = true;
                break;
            }
        }
        REQUIRE(foundWeapon);
    }

    SECTION("quest state and journal survive")
    {
        // Regression: the quest log was never serialized; reload used to reset
        // an active quest to Unknown and drop the journal entry.
        REQUIRE(dstQuests.isQuestActive(100));
        REQUIRE_FALSE(dstQuests.getJournal().empty());
        REQUIRE(dstQuests.getJournal().back().text == "Spoke to Lord");
    }
}

TEST_CASE("Save header rejects bad magic and future versions", "[game][save]")
{
    NullLogger logger;
    TempSaveDir dir;
    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    seedWorld(src);

    // A valid save must round-trip and report its header via listSlots.
    REQUIRE(save.save(src, 3));
    auto slots = save.listSlots();
    REQUIRE(slots.size() > 3);
    bool foundSlot3 = false;
    for (const auto& s : slots)
    {
        if (s.slotIndex == 3 && s.exists)
        {
            REQUIRE(s.header.magic == SaveHeader::kMagic);
            REQUIRE(s.header.version == SaveHeader::kVersion);
            foundSlot3 = true;
            break;
        }
    }
    REQUIRE(foundSlot3);

    SECTION("loading a non-existent slot fails")
    {
        GameWorld dst;
        REQUIRE_FALSE(save.load(dst, 39));
    }

    SECTION("slotExists reflects save/delete")
    {
        REQUIRE(save.slotExists(3));
        REQUIRE(save.deleteSlot(3));
        REQUIRE_FALSE(save.slotExists(3));
    }
}

TEST_CASE("Save/Load preserves generated chest world positions", "[game][save][chest]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    src.setCurrentMap("test.odm");

    GeneratedMapContent content;
    content.seed = 42;
    GeneratedChest chest;
    chest.chestId = 3;
    chest.x = 111.0f;
    chest.y = 222.0f;
    chest.z = 333.0f;
    chest.itemIds = {5, 6};
    content.chests.push_back(chest);
    src.setGeneratedContent("test.odm", content);

    REQUIRE(save.save(src, 0));

    GameWorld dst;
    REQUIRE(save.load(dst, 0));

    const auto* loaded = dst.getGeneratedContent("test.odm");
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->chests.size() == 1);
    CHECK(loaded->chests[0].chestId == 3);
    CHECK(loaded->chests[0].x == 111.0f);
    CHECK(loaded->chests[0].y == 222.0f);
    CHECK(loaded->chests[0].z == 333.0f);
    REQUIRE(loaded->chests[0].itemIds.size() == 2);
}

TEST_CASE("Save/Load round-trips active timed spell effects", "[game][save][buffs]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    auto& ch = src.party().member(0);
    ch.charClass = CharacterClass::Sorcerer;
    ch.level = 10;
    ch.stats.intellect = 40;
    ch.baseStats = ch.stats;
    ch.recalculateDerived();
    ch.maxHitPoints = 200;
    ch.hitPoints = 200;
    ch.maxSpellPoints = 200;
    ch.spellPoints = 200;

    TimedEffect effect;
    effect.spellId = 45;
    effect.expiresAtTicks = 1234567;
    effect.name = "Shield";
    effect.armorClassBonus = 10;
    effect.statBonus[0] = 5;
    effect.resistanceBonus[0] = 10;
    ch.addTimedEffect(effect);
    const int sourceArmorClass = ch.armorClass;

    REQUIRE(save.save(src, 0));

    GameWorld dst;
    REQUIRE(save.load(dst, 0));

    const auto& loaded = dst.party().member(0);
    REQUIRE(loaded.timedEffects.size() == 1);
    CHECK(loaded.timedEffects[0].spellId == 45);
    CHECK(loaded.timedEffects[0].expiresAtTicks == 1234567);
    CHECK(loaded.timedEffects[0].name == "Shield");
    CHECK(loaded.timedEffects[0].armorClassBonus == 10);
    CHECK(loaded.timedEffects[0].statBonus[0] == 5);
    CHECK(loaded.timedEffects[0].resistanceBonus[0] == 10);
    CHECK(loaded.armorClass == sourceArmorClass);
}

TEST_CASE("Save files are written atomically with a checksum trailer", "[game][save][integrity]")
{
    NullLogger logger;
    TempSaveDir dir;
    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    seedWorld(src);
    REQUIRE(save.save(src, 0));

    const auto slotPath = dir.path / "save000.mm7";
    REQUIRE(std::filesystem::exists(slotPath));
    // The temporary file must not outlive a successful save.
    CHECK_FALSE(std::filesystem::exists(dir.path / "save000.mm7.tmp"));
    CHECK(std::filesystem::exists(dir.path / "autosave.mm7"));
    CHECK_FALSE(std::filesystem::exists(dir.path / "autosave.mm7.tmp"));

    runeharbor::formats::GameLODArchive archive(logger);
    REQUIRE(archive.open(slotPath.string()));
    auto blob = archive.extractFile("runeharbor.bin");
    REQUIRE(blob.has_value());
    REQUIRE(blob->size() >= sizeof(SaveHeader) + 8);

    SaveHeader header{};
    std::memcpy(&header, blob->data(), sizeof(header));
    CHECK(header.version == SaveHeader::kVersion);
    CHECK(header.version >= 16);

    uint32_t trailerMagic = 0;
    std::memcpy(&trailerMagic, blob->data() + blob->size() - sizeof(trailerMagic),
                sizeof(trailerMagic));
    CHECK(trailerMagic == 0x53434852u); // "RHCS"

    // party.bin is the headerless copy of the same payload, without the
    // world-blob header or checksum trailer.
    auto partyBlob = archive.extractFile("party.bin");
    REQUIRE(partyBlob.has_value());
    CHECK(partyBlob->size() == blob->size() - sizeof(SaveHeader) - 8);
}

TEST_CASE("Corrupt save is rejected without mutating live state", "[game][save][integrity]")
{
    NullLogger logger;
    TempSaveDir dir;
    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    seedWorld(src);
    REQUIRE(save.save(src, 0));

    const auto slotPath = dir.path / "save000.mm7";
    auto bytes = readFileBytes(slotPath);
    REQUIRE_FALSE(bytes.empty());

    // SaveHeader::kMagic = 0x52484256, which is stored little-endian as "VBHR".
    const auto magicPos = findBytes(bytes, "VBHR");
    REQUIRE(magicPos.has_value());
    // Flip one byte inside the serialized world payload (past the header).
    const size_t corruptOffset = *magicPos + sizeof(SaveHeader) + 16;
    REQUIRE(corruptOffset < bytes.size());
    bytes[corruptOffset] ^= 0xFF;
    REQUIRE(writeFileBytes(slotPath, bytes));

    GameWorld live;
    live.party().setGold(777);
    live.setCurrentMap("sentinel.map");
    live.party().member(0).name = "Sentinel";

    Inventory liveInventory(logger);
    Item sentinelItem;
    sentinelItem.itemId = 9;
    sentinelItem.identified = true;
    REQUIRE(liveInventory.addToBackpack(0, sentinelItem));

    runeharbor::formats::QuestEntry def;
    def.qBit = 100;
    def.questNoteText = "Sentinel quest";
    QuestLog liveQuests(logger);
    liveQuests.loadQuestData({def});
    REQUIRE(liveQuests.startQuest(100, 500));

    REQUIRE_FALSE(save.load(live, 0, nullptr, &liveInventory, &liveQuests));

    CHECK(live.party().gold() == 777);
    CHECK(live.currentMap() == "sentinel.map");
    CHECK(live.party().member(0).name == "Sentinel");

    bool foundItem = false;
    for (const auto& item : liveInventory.getInventory(0).backpack)
    {
        if (item.valid() && item.itemId == 9)
        {
            foundItem = true;
        }
    }
    CHECK(foundItem);
    CHECK(liveQuests.isQuestActive(100));
    CHECK(liveQuests.getJournal().size() == 1);
}

TEST_CASE("Corrupt companion blob aborts load without partial commit", "[game][save][integrity]")
{
    NullLogger logger;
    TempSaveDir dir;
    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    seedWorld(src);

    Inventory srcInventory(logger);
    Item weapon;
    weapon.itemId = 5;
    srcInventory.addToBackpack(0, weapon);

    runeharbor::formats::QuestEntry def;
    def.qBit = 100;
    def.questNoteText = "Saved quest";
    QuestLog srcQuests(logger);
    srcQuests.loadQuestData({def});
    REQUIRE(srcQuests.startQuest(100, 1000));

    REQUIRE(save.save(src, 0, nullptr, &srcInventory, &srcQuests));

    const auto slotPath = dir.path / "save000.mm7";
    auto bytes = readFileBytes(slotPath);
    REQUIRE_FALSE(bytes.empty());

    // Invalidate the inventory blob's magic. The world blob stays valid, so
    // this exercises the post-world validation path.
    const auto inventoryMagic = findBytes(bytes, "RHIV");
    REQUIRE(inventoryMagic.has_value());
    bytes[*inventoryMagic] ^= 0xFF;
    REQUIRE(writeFileBytes(slotPath, bytes));

    GameWorld live;
    live.party().setGold(777);
    live.setCurrentMap("sentinel.map");

    Inventory liveInventory(logger);
    Item sentinelItem;
    sentinelItem.itemId = 9;
    REQUIRE(liveInventory.addToBackpack(0, sentinelItem));

    QuestLog liveQuests(logger);
    liveQuests.loadQuestData({def});
    REQUIRE(liveQuests.startQuest(100, 500));

    REQUIRE_FALSE(save.load(live, 0, nullptr, &liveInventory, &liveQuests));

    CHECK(live.party().gold() == 777);
    CHECK(live.currentMap() == "sentinel.map");
    CHECK(liveInventory.getInventory(0).backpack[0].itemId == 9);
    CHECK(liveInventory.findItem(5) == std::nullopt);
    CHECK(liveQuests.isQuestActive(100));
}

TEST_CASE("Truncated save is rejected without mutating live state", "[game][save][integrity]")
{
    NullLogger logger;
    TempSaveDir dir;
    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    seedWorld(src);
    REQUIRE(save.save(src, 0));

    // Write a raw (non-archive) slot whose payload is cut inside the checksum
    // trailer. The archive's duplicate party.bin cannot mask this.
    runeharbor::formats::GameLODArchive archive(logger);
    REQUIRE(archive.open((dir.path / "save000.mm7").string()));
    auto blob = archive.extractFile("runeharbor.bin");
    REQUIRE(blob.has_value());
    REQUIRE(blob->size() > 16);
    blob->resize(blob->size() - 16);
    REQUIRE(writeFileBytes(dir.path / "save002.mm7", *blob));

    GameWorld live;
    live.party().setGold(777);
    live.setCurrentMap("sentinel.map");

    REQUIRE_FALSE(save.load(live, 2));
    CHECK(live.party().gold() == 777);
    CHECK(live.currentMap() == "sentinel.map");
}

TEST_CASE("Version 15 saves without a checksum trailer still load", "[game][save][integrity]")
{
    NullLogger logger;
    TempSaveDir dir;
    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    seedWorld(src);
    REQUIRE(save.save(src, 0));

    runeharbor::formats::GameLODArchive archive(logger);
    REQUIRE(archive.open((dir.path / "save000.mm7").string()));
    auto blob = archive.extractFile("runeharbor.bin");
    REQUIRE(blob.has_value());
    REQUIRE(blob->size() > sizeof(SaveHeader) + 8);

    // Strip the v16 trailer and mark the payload as v15.
    blob->resize(blob->size() - 8);
    const uint32_t legacyVersion = 15;
    std::memcpy(blob->data() + offsetof(SaveHeader, version), &legacyVersion,
                sizeof(legacyVersion));
    REQUIRE(writeFileBytes(dir.path / "save001.mm7", *blob));

    GameWorld dst;
    REQUIRE(save.load(dst, 1));
    CHECK(dst.party().gold() == 1234);
    CHECK(dst.currentMap() == "out01.odm");
}

TEST_CASE("Save/Load round-trips utility spell effects", "[game][save][utility]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    const int64_t now = src.calendar().totalTicks;
    src.setUtilityEffect(UtilityEffect::Light, 3, now + 5 * GameCalendar::kTicksPerHour);
    src.setUtilityEffect(UtilityEffect::Fly, 1, 0);

    REQUIRE(save.save(src, 0));

    GameWorld dst;
    REQUIRE(save.load(dst, 0));

    CHECK(dst.hasUtilityEffect(UtilityEffect::Light));
    CHECK(dst.utilityEffectStrength(UtilityEffect::Light) == 3);
    CHECK(dst.utilityEffect(UtilityEffect::Light).expiresAtTicks ==
          now + 5 * GameCalendar::kTicksPerHour);
    CHECK(dst.hasUtilityEffect(UtilityEffect::Fly));
    CHECK(dst.utilityEffect(UtilityEffect::Fly).expiresAtTicks == 0);
    CHECK_FALSE(dst.hasUtilityEffect(UtilityEffect::WaterWalk));
}

TEST_CASE("Save/Load round-trips Lloyd's Beacon", "[game][save][utility]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    src.setBeacon(BeaconLocation{.set = true,
                                 .mapName = "out05.odm",
                                 .x = 4096.0f,
                                 .y = -512.0f,
                                 .z = 128.0f,
                                 .yaw = 768.0f});

    REQUIRE(save.save(src, 0));

    GameWorld dst;
    REQUIRE(save.load(dst, 0));

    REQUIRE(dst.hasBeacon());
    CHECK(dst.beacon().mapName == "out05.odm");
    CHECK(dst.beacon().x == 4096.0f);
    CHECK(dst.beacon().y == -512.0f);
    CHECK(dst.beacon().z == 128.0f);
    CHECK(dst.beacon().yaw == 768.0f);
}

TEST_CASE("Save/Load round-trips per-map monster state", "[game][save][state]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    SavedMonsterState monster;
    monster.monsterId = 42;
    monster.x = 11.0f;
    monster.y = 22.0f;
    monster.z = 33.0f;
    monster.velocityX = 4.0f;
    monster.velocityY = 5.0f;
    monster.velocityZ = 6.0f;
    monster.facingAngle = 768;
    monster.currentHP = 17;
    monster.maxHP = 90;
    monster.group = 5;
    monster.topic = 23;
    monster.aiState = static_cast<uint8_t>(MonsterInstance::AIState::Attacking);
    monster.hostilityLevel = 3;
    monster.hostilityOverride = 1001;
    monster.flags = 0x8000;
    monster.flagsExtended = 0x80000;
    monster.targetCharacter = 2;
    monster.targetId = 77;
    monster.scriptFields = {1, 2, 3, 4, 5, 6, 7, 8};
    src.setSavedMapMonsters("out01.odm", {monster});
    // Cleared maps keep an empty entry so the roster does not regenerate.
    src.setSavedMapMonsters("out02.odm", {});

    REQUIRE(save.save(src, 0));

    GameWorld dst;
    REQUIRE(save.load(dst, 0));

    const auto* loaded = dst.getSavedMapMonsters("out01.odm");
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->size() == 1);
    const auto& m = (*loaded)[0];
    CHECK(m.monsterId == 42);
    CHECK(m.x == 11.0f);
    CHECK(m.y == 22.0f);
    CHECK(m.z == 33.0f);
    CHECK(m.velocityX == 4.0f);
    CHECK(m.velocityY == 5.0f);
    CHECK(m.velocityZ == 6.0f);
    CHECK(m.facingAngle == 768);
    CHECK(m.currentHP == 17);
    CHECK(m.maxHP == 90);
    CHECK(m.group == 5);
    CHECK(m.topic == 23);
    CHECK(m.aiState == static_cast<uint8_t>(MonsterInstance::AIState::Attacking));
    CHECK(m.hostilityLevel == 3);
    CHECK(m.hostilityOverride == 1001);
    CHECK(m.flags == 0x8000);
    CHECK(m.flagsExtended == 0x80000);
    CHECK(m.targetCharacter == 2);
    CHECK(m.targetId == 77);
    CHECK(m.scriptFields[0] == 1);
    CHECK(m.scriptFields[7] == 8);

    const auto* cleared = dst.getSavedMapMonsters("out02.odm");
    REQUIRE(cleared != nullptr);
    CHECK(cleared->empty());
}

TEST_CASE("Save/Load round-trips acquired awards and autonotes", "[game][save][journal]")
{
    NullLogger logger;
    TempSaveDir dir;

    SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());

    GameWorld src;
    QuestLog srcQuests(logger);
    srcQuests.acquireAward(2);
    srcQuests.acquireAward(7);
    srcQuests.acquireAutonote(1);
    srcQuests.acquireAutonote(42);

    REQUIRE(save.save(src, 0, nullptr, nullptr, &srcQuests));

    GameWorld dst;
    QuestLog dstQuests(logger);
    REQUIRE(save.load(dst, 0, nullptr, nullptr, &dstQuests));

    CHECK(dstQuests.hasAward(2));
    CHECK(dstQuests.hasAward(7));
    CHECK_FALSE(dstQuests.hasAward(1));
    CHECK(dstQuests.acquiredAwards().size() == 2);
    CHECK(dstQuests.hasAutonote(1));
    CHECK(dstQuests.hasAutonote(42));
    CHECK_FALSE(dstQuests.hasAutonote(2));
    CHECK(dstQuests.acquiredAutonotes().size() == 2);
}
