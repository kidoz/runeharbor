// SPDX-License-Identifier: MIT
//
// Unit tests for usable items (potions/scrolls/books/message-scrolls) — the
// consume path documented in docs/usable-items.md. Verifies the effect
// application and consume-on-use behavior of Inventory::useItem.
#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/items_parser.hpp"
#include "../../src/game/character.hpp"
#include "../../src/game/inventory.hpp"
#include "../../src/game/party.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::game;
using namespace runeharbor::formats;

namespace
{

class NullLogger : public runeharbor::util::ILogger
{
  public:
    void log(runeharbor::util::LogLevel, std::string_view) override {}
};

ItemEntry makePotion(int id)
{
    ItemEntry e;
    e.id = id;
    e.name = "Potion";
    e.equipStat = "potion";
    e.value = 30;
    return e;
}

ItemEntry makeScroll(int id)
{
    ItemEntry e;
    e.id = id;
    e.name = "Scroll";
    e.equipStat = "sscroll";
    return e;
}

ItemEntry makeBook(int id)
{
    ItemEntry e;
    e.id = id;
    e.name = "Spellbook";
    e.equipStat = "book";
    return e;
}

ItemEntry makeMessageScroll(int id)
{
    ItemEntry e;
    e.id = id;
    e.name = "Message Scroll";
    e.equipStat = "mscroll";
    return e;
}

ItemEntry makeWeapon(int id)
{
    ItemEntry e;
    e.id = id;
    e.name = "Sword";
    e.equipStat = "weapon1h";
    return e;
}

Item makeItemWithCharge(int id, int charge)
{
    Item item;
    item.itemId = id;
    item.chargeCount = charge;
    item.identified = true;
    return item;
}

} // namespace

TEST_CASE("Heal potion restores HP and is consumed", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makePotion(220)});

    Party party;
    Character& c = party.member(0);
    c.hitPoints = 10;
    c.maxHitPoints = 50;
    c.spellPoints = 0;
    c.maxSpellPoints = 0;
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(220, 5)); // heal potion, power 5 -> +15 HP
    auto result = inv.useItem(0, 0, 0);

    REQUIRE(result.used);
    REQUIRE(result.consumed);
    REQUIRE(result.hpHealed == 15);
    REQUIRE(c.hitPoints == 25);
    REQUIRE_FALSE(inv.getInventory(0).backpack[0].valid()); // consumed
}

TEST_CASE("Cure potion clears poison", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makePotion(221)}); // 221 = cure poison

    Party party;
    Character& c = party.member(0);
    c.setCondition(ConditionIndex::Poison1);
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(221, 1));
    auto result = inv.useItem(0, 0, 0);

    REQUIRE(result.used);
    REQUIRE(result.consumed);
    REQUIRE_FALSE(c.hasCondition(ConditionIndex::Poison1));
}

TEST_CASE("Mana potion restores spell points", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makePotion(222)}); // 222 = mana

    Party party;
    Character& c = party.member(0);
    c.spellPoints = 0;
    c.maxSpellPoints = 50;
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(222, 5)); // +15 SP
    auto result = inv.useItem(0, 0, 0);

    REQUIRE(result.used);
    REQUIRE(result.consumed);
    REQUIRE(result.spRestored == 15);
    REQUIRE(c.spellPoints == 15);
}

TEST_CASE("Message scroll is used but NOT consumed", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeMessageScroll(700)});

    Party party;
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(700, 0));
    auto result = inv.useItem(0, 0, 0);

    REQUIRE(result.used);
    REQUIRE_FALSE(result.consumed);
    REQUIRE(inv.getInventory(0).backpack[0].valid()); // still there
}

TEST_CASE("Book learns a spell when the school is known", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeBook(400)}); // book index 0 -> spell id 1

    Party party;
    Character& c = party.member(0);
    c.skillLevels[static_cast<size_t>(SkillId::Fire)] =
        SkillValue{1, SkillMastery::Normal}; // knows Fire
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(400, 0));
    auto result = inv.useItem(0, 0, 0);

    REQUIRE(result.used);
    REQUIRE(result.consumed);
    REQUIRE(result.spellLearned == 1);
    REQUIRE(c.knowsSpell(1));
    REQUIRE_FALSE(inv.getInventory(0).backpack[0].valid()); // consumed
}

TEST_CASE("Book is refused when the school skill is missing", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeBook(400)});

    Party party;
    // member 0 has no Fire skill
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(400, 0));
    auto result = inv.useItem(0, 0, 0);

    REQUIRE_FALSE(result.used);
    REQUIRE_FALSE(result.consumed);
    REQUIRE_FALSE(party.member(0).knowsSpell(1));
    REQUIRE(inv.getInventory(0).backpack[0].valid()); // still there
}

TEST_CASE("Non-usable item (weapon) is refused", "[game][usable]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeWeapon(1)});

    Party party;
    inv.setParty(&party);

    inv.addToBackpack(0, makeItemWithCharge(1, 0));
    auto result = inv.useItem(0, 0, 0);

    REQUIRE_FALSE(result.used);
    REQUIRE_FALSE(result.consumed);
    // The weapon remains — useItem doesn't touch equippable items.
    REQUIRE(inv.getInventory(0).backpack[0].valid());
}

TEST_CASE("learnSpell refuses an unknown spell id", "[character][usable]")
{
    Character c;
    c.skillLevels[static_cast<size_t>(SkillId::Fire)] = SkillValue{1, SkillMastery::Normal};
    REQUIRE_FALSE(c.learnSpell(0, SkillId::Fire));   // id out of range
    REQUIRE_FALSE(c.learnSpell(100, SkillId::Fire)); // id out of range
    REQUIRE(c.learnSpell(1, SkillId::Fire));         // valid + school known
    REQUIRE_FALSE(c.learnSpell(1, SkillId::Fire));   // already known
}
