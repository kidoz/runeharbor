// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "../formats/items_parser.hpp"
#include "../util/ilogger.hpp"
#include "character.hpp"

namespace runeharbor::game
{

// Item equip type (matches MM7's 21 equip types from items.txt)
enum class EquipType : uint8_t
{
    Weapon1H = 0,
    Weapon2H = 1,
    Missile = 2,
    Armor = 3,
    Shield = 4,
    Helmet = 5,
    Belt = 6,
    Cloak = 7,
    Gauntlets = 8,
    Boots = 9,
    Ring = 10,
    Amulet = 11,
    Wand = 12,
    Reagent = 13,
    Potion = 14,
    SpellScroll = 15,
    Book = 16,
    MessageScroll = 17,
    Deed = 18,
    GoldItem = 19,
    None = 20,
    Count = 21
};

// Item material/quality tier
enum class ItemMaterial : uint8_t
{
    Common = 0,
    Artifact = 1,
    Relic = 2,
    Special = 3,
};

// A concrete item instance in the game world
struct Item
{
    int itemId = 0;           // Reference to ItemEntry
    int enchantId = 0;        // Standard enchantment (0=none)
    int specialEnchantId = 0; // Special enchantment (0=none)
    int chargeCount = 0;      // Remaining charges (wands, etc.)
    bool identified = false;  // Has the item been identified?
    bool broken = false;      // Is the item broken?
    bool temporaryEnchant = false;

    bool valid() const { return itemId > 0; }
};

// Backpack: grid-based inventory (MM7 uses 14 slots per character)
static constexpr int kBackpackSlots = 14;

// Per-character inventory
struct CharacterInventory
{
    std::array<Item, static_cast<size_t>(EquipSlot::Count)> equipped = {};
    std::array<Item, kBackpackSlots> backpack = {};

    bool hasEquipped(EquipSlot slot) const { return equipped[static_cast<size_t>(slot)].valid(); }

    int freeBackpackSlots() const
    {
        int count = 0;
        for (const auto& item : backpack)
        {
            if (!item.valid())
                count++;
        }
        return count;
    }
};

// The inventory system manages items across the party
class Inventory
{
  public:
    explicit Inventory(util::ILogger& logger);

    // Load item definitions from parsed data
    void loadItemData(const std::vector<formats::ItemEntry>& items);

    // Item data lookup
    const formats::ItemEntry* getItemDef(int itemId) const;
    EquipType getEquipType(int itemId) const;
    EquipSlot getEquipSlot(int itemId) const;
    int getItemWeight(int itemId) const;

    // Per-character inventory access
    CharacterInventory& getInventory(int characterIndex);
    const CharacterInventory& getInventory(int characterIndex) const;

    // Equip/unequip
    bool canEquip(int characterIndex, int itemId) const;
    bool equip(int characterIndex, int backpackSlot);
    bool unequip(int characterIndex, EquipSlot slot);

    // Backpack operations
    bool addToBackpack(int characterIndex, const Item& item);
    bool removeFromBackpack(int characterIndex, int backpackSlot);
    std::optional<Item> takeFromBackpack(int characterIndex, int backpackSlot);

    // Item identification
    bool identifyItem(int characterIndex, EquipSlot slot);
    bool identifyBackpackItem(int characterIndex, int backpackSlot);

    // Item repair
    bool repairItem(int characterIndex, EquipSlot slot);

    // Weight calculation
    int totalWeight(int characterIndex) const;

    // Gold value of all items
    int totalValue(int characterIndex) const;

    // Find first matching item across party
    struct ItemLocation
    {
        int characterIndex = -1;
        bool equipped = false;
        int slotIndex = -1;
    };
    std::optional<ItemLocation> findItem(int itemId) const;

    // Remove first matching item from party (for quest/event "TakeItem")
    bool removeItem(int itemId);

    // Give item to first character with space
    bool giveItem(const Item& item);

  private:
    EquipType categorizeItem(const formats::ItemEntry& entry) const;
    EquipSlot mapEquipSlot(const formats::ItemEntry& entry) const;

    util::ILogger& logger_;
    std::unordered_map<int, formats::ItemEntry> itemDefs_;
    std::array<CharacterInventory, 4> inventories_ = {};
};

} // namespace runeharbor::game
