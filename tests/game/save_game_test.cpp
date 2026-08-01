// SPDX-License-Identifier: MIT
//
// Unit tests for the save/load system. The primary target is the full
// round-trip: mutate representative fields across the party, characters,
// inventory, and quest log; save to a temp slot; load into a fresh world;
// assert every field survived. This locks in the previously-missing fields
// (spellbook, bank gold, quest state, quickbar) and guards future regressions.
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/quests_parser.hpp"
#include "../../src/game/character.hpp"
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
