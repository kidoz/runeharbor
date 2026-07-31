// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../formats/items_parser.hpp"
#include "../util/ilogger.hpp"
#include "character.hpp"

namespace runeharbor::game
{

class Party;       // forward declaration (party.hpp does not include this header)
class SpellSystem; // forward declaration (spells.hpp does not include this header)

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

// RE: Character struct at +0x1F0 has 140 Item[36] slots (5,040 bytes total).
// These 140 slots form the item pool; equipment indices and backpack grid reference this pool.
// The backpack is a 14-column grid; items can occupy multi-cell slots.
static constexpr int kItemPoolSize = 140;

// Backpack grid: 14 columns (visual grid width in the inventory UI)
static constexpr int kBackpackColumns = 14;

// Legacy alias
static constexpr int kBackpackSlots = kBackpackColumns;

// Per-character inventory
// RE: 140-item pool, 16 equipment indices, backpack grid references into the pool
struct CharacterInventory
{
    // Item pool: all items this character owns (RE: 140 × 36B at character+0x1F0)
    std::array<Item, kItemPoolSize> items = {};

    // Equipment: indices into items[] array (0 = empty, 1-based index)
    std::array<Item, static_cast<size_t>(EquipSlot::Count)> equipped = {};

    // Backpack display grid (legacy 14-slot view for backward compat)
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

    int freeItemPoolSlots() const
    {
        int count = 0;
        for (const auto& item : items)
        {
            if (!item.valid())
                count++;
        }
        return count;
    }
};

// Outcome of using a consumable item (potion/scroll/book/message-scroll).
struct UseResult
{
    bool used = false;     // did the item take effect (and get consumed)?
    bool consumed = false; // was the item removed from the backpack?
    int hpHealed = 0;      // HP restored (heal potion / scroll)
    int spRestored = 0;    // SP restored (mana potion)
    int spellLearned = 0;  // spell id learned from a book (0 = none)
    std::string message;   // message-scroll text or status description
};

// The inventory system manages items across the party
class Inventory
{
  public:
    explicit Inventory(util::ILogger& logger);

    // Load item definitions from parsed data
    void loadItemData(const std::vector<formats::ItemEntry>& items);

    // Optional party reference: when set, equip/canEquip enforce the MM7 skill
    // requirement (FUN_004926F8) — a sword needs Sword learned, plate needs
    // Plate, etc. When null, the skill gate is skipped (slot validity only).
    // Also used by useItem to mutate character HP/SP/conditions/spellbook.
    void setParty(Party* party) { party_ = party; }

    // Optional spell system: when set, useItem can apply spell effects (potion
    // heal/cure, scroll cast). When null, usable items are refused.
    void setSpellSystem(SpellSystem* spells) { spellSystem_ = spells; }

    // Item data lookup
    const formats::ItemEntry* getItemDef(int itemId) const;
    // All loaded item definitions (for shop stock listing).
    std::vector<formats::ItemEntry> itemTable() const;
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

    // Use a consumable item (potion/scroll/book/message-scroll) from a
    // backpack slot on the target character (FUN_004680F1). Requires
    // setSpellSystem to have been called for potions/scrolls. The item is
    // consumed on a successful use (message scrolls are NOT consumed).
    UseResult useItem(int characterIndex, int backpackSlot, int targetCharIndex);

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

    // Snapshot/restore full runtime inventories (used by save/load).
    const std::array<CharacterInventory, 4>& inventories() const { return inventories_; }
    void setInventories(std::array<CharacterInventory, 4> inventories)
    {
        inventories_ = std::move(inventories);
    }

  private:
    EquipType categorizeItem(const formats::ItemEntry& entry) const;
    EquipSlot mapEquipSlot(const formats::ItemEntry& entry) const;
    // Resolves the skill an item requires (from its skillGroup/equipStat), or
    // nullopt if the item has no skill requirement (rings, amulets, misc).
    std::optional<SkillId> requiredSkillForItem(int itemId) const;
    // True if the character has the item's required skill learned (or the item
    // requires none). Mirrors FUN_004926F8.
    bool meetsSkillRequirement(int characterIndex, int itemId) const;
    // Resolves the spell a scroll casts (scroll.txt mapping), or the heal spell
    // id for a potion's effect. Returns 0 if unresolved.
    int spellIdForScroll(int itemId) const;

    util::ILogger& logger_;
    std::unordered_map<int, formats::ItemEntry> itemDefs_;
    std::array<CharacterInventory, 4> inventories_ = {};
    Party* party_ = nullptr;
    SpellSystem* spellSystem_ = nullptr;
};

} // namespace runeharbor::game
