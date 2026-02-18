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

// Item category derived from equipStat field
enum class ItemCategory : uint8_t
{
    Weapon = 0,
    Armor,
    Shield,
    Helmet,
    Belt,
    Cloak,
    Gauntlets,
    Boots,
    Amulet,
    Ring,
    Potion,
    Scroll,
    Reagent,
    Gem,
    Gold,
    Misc,
    Count
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
    ItemCategory getItemCategory(int itemId) const;
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
    ItemCategory categorizeItem(const formats::ItemEntry& entry) const;
    EquipSlot mapEquipSlot(const formats::ItemEntry& entry) const;

    util::ILogger& logger_;
    std::unordered_map<int, formats::ItemEntry> itemDefs_;
    std::array<CharacterInventory, 4> inventories_ = {};
};

} // namespace runeharbor::game
