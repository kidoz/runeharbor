// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>

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

ItemEntry makeSword()
{
    ItemEntry e;
    e.id = 1;
    e.name = "Short Sword";
    e.equipStat = "weapon1h";
    e.mod1 = "1d6";
    e.mod2 = 2;
    e.value = 100;
    e.material = 0;
    return e;
}

ItemEntry makeShield()
{
    ItemEntry e;
    e.id = 2;
    e.name = "Buckler";
    e.equipStat = "shield";
    e.value = 50;
    e.material = 0;
    return e;
}

ItemEntry makeRing()
{
    ItemEntry e;
    e.id = 3;
    e.name = "Gold Ring";
    e.equipStat = "ring";
    e.value = 200;
    e.material = 0;
    return e;
}

ItemEntry makePotion()
{
    ItemEntry e;
    e.id = 4;
    e.name = "Healing Potion";
    e.equipStat = "potion";
    e.value = 30;
    e.material = 0;
    return e;
}

Item makeItem(int id)
{
    Item item;
    item.itemId = id;
    return item;
}

} // namespace

TEST_CASE("Inventory backpack operations", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword(), makeShield(), makeRing(), makePotion()});

    SECTION("add item to backpack")
    {
        REQUIRE(inv.addToBackpack(0, makeItem(1)));
        REQUIRE(inv.getInventory(0).backpack[0].itemId == 1);
    }

    SECTION("backpack fills up")
    {
        for (int i = 0; i < kBackpackSlots; i++)
        {
            REQUIRE(inv.addToBackpack(0, makeItem(1)));
        }
        REQUIRE_FALSE(inv.addToBackpack(0, makeItem(1))); // full
    }

    SECTION("remove from backpack")
    {
        inv.addToBackpack(0, makeItem(1));
        REQUIRE(inv.removeFromBackpack(0, 0));
        REQUIRE_FALSE(inv.getInventory(0).backpack[0].valid());
    }

    SECTION("takeFromBackpack returns item and clears slot")
    {
        inv.addToBackpack(0, makeItem(1));
        auto taken = inv.takeFromBackpack(0, 0);
        REQUIRE(taken.has_value());
        REQUIRE(taken->itemId == 1);
        REQUIRE_FALSE(inv.getInventory(0).backpack[0].valid());
    }

    SECTION("takeFromBackpack returns nullopt for empty slot")
    {
        REQUIRE_FALSE(inv.takeFromBackpack(0, 0).has_value());
    }

    SECTION("invalid backpack slot returns false")
    {
        REQUIRE_FALSE(inv.removeFromBackpack(0, -1));
        REQUIRE_FALSE(inv.removeFromBackpack(0, kBackpackSlots));
    }

    SECTION("free backpack slots count")
    {
        REQUIRE(inv.getInventory(0).freeBackpackSlots() == kBackpackSlots);
        inv.addToBackpack(0, makeItem(1));
        REQUIRE(inv.getInventory(0).freeBackpackSlots() == kBackpackSlots - 1);
    }
}

TEST_CASE("Inventory equip/unequip", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword(), makeShield(), makeRing(), makePotion()});

    SECTION("equip sword from backpack to main hand")
    {
        inv.addToBackpack(0, makeItem(1));
        REQUIRE(inv.equip(0, 0));
        REQUIRE(inv.getInventory(0).hasEquipped(EquipSlot::MainHand));
        REQUIRE_FALSE(inv.getInventory(0).backpack[0].valid()); // cleared from backpack
    }

    SECTION("equip shield goes to off hand")
    {
        inv.addToBackpack(0, makeItem(2));
        REQUIRE(inv.equip(0, 0));
        REQUIRE(inv.getInventory(0).hasEquipped(EquipSlot::OffHand));
    }

    SECTION("unequip moves item to backpack")
    {
        inv.addToBackpack(0, makeItem(1));
        inv.equip(0, 0);
        REQUIRE(inv.unequip(0, EquipSlot::MainHand));
        REQUIRE_FALSE(inv.getInventory(0).hasEquipped(EquipSlot::MainHand));
        // Item should be in backpack now
        bool found = false;
        for (int i = 0; i < kBackpackSlots; i++)
        {
            if (inv.getInventory(0).backpack[static_cast<size_t>(i)].itemId == 1)
            {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("unequip fails with full backpack")
    {
        inv.addToBackpack(0, makeItem(1));
        inv.equip(0, 0);
        // Fill backpack
        for (int i = 0; i < kBackpackSlots; i++)
        {
            inv.addToBackpack(0, makeItem(4));
        }
        REQUIRE_FALSE(inv.unequip(0, EquipSlot::MainHand));
    }

    SECTION("cannot equip empty backpack slot")
    {
        REQUIRE_FALSE(inv.equip(0, 0));
    }

    SECTION("equipping swaps when slot occupied")
    {
        inv.addToBackpack(0, makeItem(1)); // slot 0
        inv.addToBackpack(0, makeItem(1)); // slot 1 (another sword)
        inv.getInventory(0).backpack[1].itemId = 1;

        inv.equip(0, 0); // equip first sword
        REQUIRE(inv.getInventory(0).hasEquipped(EquipSlot::MainHand));

        inv.equip(0, 1); // equip second sword, first goes back to backpack
        REQUIRE(inv.getInventory(0).hasEquipped(EquipSlot::MainHand));
    }
}

TEST_CASE("Inventory ring slot logic", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeRing()});

    SECTION("rings fill sequential slots")
    {
        inv.addToBackpack(0, makeItem(3));
        inv.equip(0, 0);
        REQUIRE(inv.getInventory(0).hasEquipped(EquipSlot::Ring1));

        inv.addToBackpack(0, makeItem(3));
        inv.equip(0, 0);
        REQUIRE(inv.getInventory(0).hasEquipped(EquipSlot::Ring2));
    }
}

TEST_CASE("Inventory canEquip", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword(), makePotion()});

    SECTION("weapon is equippable")
    {
        REQUIRE(inv.canEquip(0, 1));
    }

    SECTION("potion is not equippable")
    {
        REQUIRE_FALSE(inv.canEquip(0, 4));
    }

    SECTION("unknown item is not equippable")
    {
        REQUIRE_FALSE(inv.canEquip(0, 999));
    }

    SECTION("invalid character index")
    {
        REQUIRE_FALSE(inv.canEquip(-1, 1));
    }
}

TEST_CASE("Inventory identify and repair", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword()});

    SECTION("identify equipped item")
    {
        inv.addToBackpack(0, makeItem(1));
        inv.equip(0, 0);
        REQUIRE(inv.identifyItem(0, EquipSlot::MainHand));
        REQUIRE(inv.getInventory(0).equipped[static_cast<size_t>(EquipSlot::MainHand)].identified);
        // Already identified
        REQUIRE_FALSE(inv.identifyItem(0, EquipSlot::MainHand));
    }

    SECTION("identify backpack item")
    {
        inv.addToBackpack(0, makeItem(1));
        REQUIRE(inv.identifyBackpackItem(0, 0));
        REQUIRE(inv.getInventory(0).backpack[0].identified);
        REQUIRE_FALSE(inv.identifyBackpackItem(0, 0)); // already done
    }

    SECTION("repair broken item")
    {
        inv.addToBackpack(0, makeItem(1));
        inv.equip(0, 0);
        auto& eq = inv.getInventory(0).equipped[static_cast<size_t>(EquipSlot::MainHand)];
        eq.broken = true;
        REQUIRE(inv.repairItem(0, EquipSlot::MainHand));
        REQUIRE_FALSE(eq.broken);
    }

    SECTION("repair non-broken item returns false")
    {
        inv.addToBackpack(0, makeItem(1));
        inv.equip(0, 0);
        REQUIRE_FALSE(inv.repairItem(0, EquipSlot::MainHand));
    }
}

TEST_CASE("Inventory findItem and removeItem", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword(), makeShield()});

    SECTION("findItem locates item in backpack")
    {
        inv.addToBackpack(1, makeItem(1));
        auto loc = inv.findItem(1);
        REQUIRE(loc.has_value());
        REQUIRE(loc->characterIndex == 1);
        REQUIRE_FALSE(loc->equipped);
    }

    SECTION("findItem locates equipped item")
    {
        inv.addToBackpack(0, makeItem(1));
        inv.equip(0, 0);
        auto loc = inv.findItem(1);
        REQUIRE(loc.has_value());
        REQUIRE(loc->equipped);
    }

    SECTION("findItem returns nullopt if not found")
    {
        REQUIRE_FALSE(inv.findItem(999).has_value());
    }

    SECTION("removeItem removes from backpack")
    {
        inv.addToBackpack(0, makeItem(1));
        REQUIRE(inv.removeItem(1));
        REQUIRE_FALSE(inv.findItem(1).has_value());
    }

    SECTION("removeItem returns false if not found")
    {
        REQUIRE_FALSE(inv.removeItem(999));
    }
}

TEST_CASE("Inventory giveItem", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword()});

    SECTION("gives to first character with space")
    {
        REQUIRE(inv.giveItem(makeItem(1)));
        REQUIRE(inv.getInventory(0).backpack[0].itemId == 1);
    }

    SECTION("overflows to next character")
    {
        // Fill character 0's backpack
        for (int i = 0; i < kBackpackSlots; i++)
        {
            inv.addToBackpack(0, makeItem(1));
        }
        REQUIRE(inv.giveItem(makeItem(1)));
        REQUIRE(inv.getInventory(1).backpack[0].itemId == 1);
    }

    SECTION("fails when all backpacks full")
    {
        for (int c = 0; c < 4; c++)
        {
            for (int i = 0; i < kBackpackSlots; i++)
            {
                inv.addToBackpack(c, makeItem(1));
            }
        }
        REQUIRE_FALSE(inv.giveItem(makeItem(1)));
    }
}

TEST_CASE("Inventory totalValue", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSword(), makeShield()}); // sword=100, shield=50

    inv.addToBackpack(0, makeItem(1));
    inv.addToBackpack(0, makeItem(2));
    REQUIRE(inv.totalValue(0) == 150);
}

TEST_CASE("Inventory equip type categorization", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);

    SECTION("numeric equip stat codes")
    {
        ItemEntry e;
        e.id = 10;
        e.equipStat = "3"; // Armor
        inv.loadItemData({e});
        REQUIRE(inv.getEquipType(10) == EquipType::Armor);
    }

    SECTION("string equip stat names")
    {
        ItemEntry e;
        e.id = 11;
        e.equipStat = "Helm";
        inv.loadItemData({e});
        REQUIRE(inv.getEquipType(11) == EquipType::Helmet);
    }

    SECTION("unknown item returns None")
    {
        REQUIRE(inv.getEquipType(999) == EquipType::None);
    }
}

// ---------------------------------------------------------------------------
// Equip skill gating (FUN_004926F8) — see docs/re/31-inventory-equipment.md
// ---------------------------------------------------------------------------

namespace
{

ItemEntry makeSkilledSword()
{
    // A sword whose skillGroup requires the Sword skill (as real items.txt does).
    ItemEntry e;
    e.id = 10;
    e.name = "Knight's Blade";
    e.equipStat = "weapon1h";
    e.skillGroup = "Sword";
    e.value = 100;
    return e;
}

ItemEntry makePlateArmor()
{
    ItemEntry e;
    e.id = 11;
    e.name = "Full Plate";
    e.equipStat = "armor";
    e.skillGroup = "Plate";
    e.value = 200;
    return e;
}

ItemEntry makeMiscRing()
{
    // Rings/amulets/misc have skillGroup "Misc" -> no skill required.
    ItemEntry e;
    e.id = 12;
    e.name = "Gold Ring";
    e.equipStat = "ring";
    e.skillGroup = "Misc";
    e.value = 50;
    return e;
}

} // namespace

TEST_CASE("Equip skill gate: refused when the required skill is unlearned", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSkilledSword(), makePlateArmor(), makeMiscRing()});

    Party party;
    // Member 0 has no skills learned.
    inv.setParty(&party);

    SECTION("sword refused without Sword skill")
    {
        REQUIRE_FALSE(inv.canEquip(0, 10));
    }
    SECTION("plate refused without Plate skill")
    {
        REQUIRE_FALSE(inv.canEquip(0, 11));
    }
    SECTION("ring allowed regardless of skills")
    {
        REQUIRE(inv.canEquip(0, 12));
    }
}

TEST_CASE("Equip skill gate: allowed when the required skill is learned", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSkilledSword(), makePlateArmor()});

    Party party;
    Character& c = party.member(0);
    c.skillLevels[static_cast<size_t>(SkillId::Sword)] = SkillValue{1, SkillMastery::Normal};
    c.skillLevels[static_cast<size_t>(SkillId::Plate)] = SkillValue{1, SkillMastery::Normal};
    inv.setParty(&party);

    REQUIRE(inv.canEquip(0, 10)); // sword, Sword learned
    REQUIRE(inv.canEquip(0, 11)); // plate, Plate learned

    // equip() also enforces the gate.
    inv.addToBackpack(0, Item{10});
    REQUIRE(inv.equip(0, 0));
    REQUIRE(inv.getInventory(0).equipped[static_cast<size_t>(EquipSlot::MainHand)].itemId == 10);
}

TEST_CASE("Equip skill gate: equip() refuses an unskilled item", "[game][inventory]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSkilledSword()});

    Party party; // no skills learned
    inv.setParty(&party);

    inv.addToBackpack(0, Item{10});
    REQUIRE_FALSE(inv.equip(0, 0)); // refused
    REQUIRE_FALSE(inv.getInventory(0).equipped[static_cast<size_t>(EquipSlot::MainHand)].valid());
    REQUIRE(inv.getInventory(0).backpack[0].valid()); // still in backpack
}

TEST_CASE("Equip skill gate: skipped (allowed) when no party is wired", "[game][inventory]")
{
    // Back-compat: when setParty is not called, the gate is skipped so existing
    // behavior (slot validity only) is preserved.
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeSkilledSword()});
    // No setParty() call.
    REQUIRE(inv.canEquip(0, 10));
}
