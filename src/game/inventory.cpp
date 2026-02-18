// SPDX-License-Identifier: MIT
#include "inventory.hpp"

#include <algorithm>

namespace runeharbor::game
{

Inventory::Inventory(util::ILogger& logger) : logger_(logger) {}

void Inventory::loadItemData(const std::vector<formats::ItemEntry>& items)
{
    itemDefs_.clear();
    for (const auto& item : items)
    {
        itemDefs_[item.id] = item;
    }
    logger_.info("Loaded " + std::to_string(items.size()) + " item definitions");
}

const formats::ItemEntry* Inventory::getItemDef(int itemId) const
{
    auto it = itemDefs_.find(itemId);
    return it != itemDefs_.end() ? &it->second : nullptr;
}

ItemCategory Inventory::getItemCategory(int itemId) const
{
    auto* def = getItemDef(itemId);
    if (!def)
        return ItemCategory::Misc;
    return categorizeItem(*def);
}

EquipSlot Inventory::getEquipSlot(int itemId) const
{
    auto* def = getItemDef(itemId);
    if (!def)
        return EquipSlot::Count; // Invalid
    return mapEquipSlot(*def);
}

int Inventory::getItemWeight(int itemId) const
{
    auto* def = getItemDef(itemId);
    // MM7 doesn't have explicit weight in items.txt; approximate from value/material
    if (!def)
        return 0;
    // Base weight by category
    switch (categorizeItem(*def))
    {
    case ItemCategory::Weapon:
        return 3 + def->material;
    case ItemCategory::Armor:
        return 5 + def->material * 2;
    case ItemCategory::Shield:
        return 3;
    case ItemCategory::Helmet:
        return 2;
    case ItemCategory::Boots:
    case ItemCategory::Gauntlets:
        return 1;
    case ItemCategory::Belt:
    case ItemCategory::Cloak:
        return 1;
    case ItemCategory::Ring:
    case ItemCategory::Amulet:
        return 0;
    case ItemCategory::Potion:
    case ItemCategory::Scroll:
    case ItemCategory::Reagent:
        return 0;
    default:
        return 1;
    }
}

CharacterInventory& Inventory::getInventory(int characterIndex)
{
    return inventories_[static_cast<size_t>(std::clamp(characterIndex, 0, 3))];
}

const CharacterInventory& Inventory::getInventory(int characterIndex) const
{
    return inventories_[static_cast<size_t>(std::clamp(characterIndex, 0, 3))];
}

bool Inventory::canEquip(int characterIndex, int itemId) const
{
    (void)characterIndex; // TODO: check class restrictions
    auto slot = getEquipSlot(itemId);
    return slot != EquipSlot::Count;
}

bool Inventory::equip(int characterIndex, int backpackSlot)
{
    auto& inv = getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= kBackpackSlots)
        return false;

    auto& item = inv.backpack[static_cast<size_t>(backpackSlot)];
    if (!item.valid())
        return false;

    EquipSlot slot = getEquipSlot(item.itemId);
    if (slot == EquipSlot::Count)
    {
        logger_.warning("Item " + std::to_string(item.itemId) + " cannot be equipped");
        return false;
    }

    // Handle ring slots: find first empty ring slot
    if (slot == EquipSlot::Ring1)
    {
        bool found = false;
        for (int r = static_cast<int>(EquipSlot::Ring1); r <= static_cast<int>(EquipSlot::Ring6);
             r++)
        {
            if (!inv.equipped[static_cast<size_t>(r)].valid())
            {
                slot = static_cast<EquipSlot>(r);
                found = true;
                break;
            }
        }
        if (!found)
        {
            // All ring slots full, replace first ring
            slot = EquipSlot::Ring1;
        }
    }

    // Swap: if slot is occupied, move equipped item to backpack
    auto slotIdx = static_cast<size_t>(slot);
    if (inv.equipped[slotIdx].valid())
    {
        // Find free backpack slot for the displaced item
        int freeSlot = -1;
        for (int i = 0; i < kBackpackSlots; i++)
        {
            if (i == backpackSlot || !inv.backpack[static_cast<size_t>(i)].valid())
            {
                if (i != backpackSlot && !inv.backpack[static_cast<size_t>(i)].valid())
                {
                    freeSlot = i;
                    break;
                }
            }
        }
        if (freeSlot < 0)
        {
            // Use the same slot we're taking from
            inv.backpack[static_cast<size_t>(backpackSlot)] = inv.equipped[slotIdx];
            inv.equipped[slotIdx] = item;
            // Already swapped
            return true;
        }
        inv.backpack[static_cast<size_t>(freeSlot)] = inv.equipped[slotIdx];
    }

    inv.equipped[slotIdx] = item;
    inv.backpack[static_cast<size_t>(backpackSlot)] = Item{};
    return true;
}

bool Inventory::unequip(int characterIndex, EquipSlot slot)
{
    auto& inv = getInventory(characterIndex);
    auto slotIdx = static_cast<size_t>(slot);

    if (!inv.equipped[slotIdx].valid())
        return false;

    // Find free backpack slot
    for (int i = 0; i < kBackpackSlots; i++)
    {
        if (!inv.backpack[static_cast<size_t>(i)].valid())
        {
            inv.backpack[static_cast<size_t>(i)] = inv.equipped[slotIdx];
            inv.equipped[slotIdx] = Item{};
            return true;
        }
    }

    logger_.warning("No free backpack slot to unequip item");
    return false;
}

bool Inventory::addToBackpack(int characterIndex, const Item& item)
{
    auto& inv = getInventory(characterIndex);
    for (int i = 0; i < kBackpackSlots; i++)
    {
        if (!inv.backpack[static_cast<size_t>(i)].valid())
        {
            inv.backpack[static_cast<size_t>(i)] = item;
            return true;
        }
    }
    return false;
}

bool Inventory::removeFromBackpack(int characterIndex, int backpackSlot)
{
    auto& inv = getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= kBackpackSlots)
        return false;
    inv.backpack[static_cast<size_t>(backpackSlot)] = Item{};
    return true;
}

std::optional<Item> Inventory::takeFromBackpack(int characterIndex, int backpackSlot)
{
    auto& inv = getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= kBackpackSlots)
        return std::nullopt;
    auto& slot = inv.backpack[static_cast<size_t>(backpackSlot)];
    if (!slot.valid())
        return std::nullopt;
    Item taken = slot;
    slot = Item{};
    return taken;
}

bool Inventory::identifyItem(int characterIndex, EquipSlot slot)
{
    auto& inv = getInventory(characterIndex);
    auto& item = inv.equipped[static_cast<size_t>(slot)];
    if (!item.valid() || item.identified)
        return false;
    item.identified = true;
    return true;
}

bool Inventory::identifyBackpackItem(int characterIndex, int backpackSlot)
{
    auto& inv = getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= kBackpackSlots)
        return false;
    auto& item = inv.backpack[static_cast<size_t>(backpackSlot)];
    if (!item.valid() || item.identified)
        return false;
    item.identified = true;
    return true;
}

bool Inventory::repairItem(int characterIndex, EquipSlot slot)
{
    auto& inv = getInventory(characterIndex);
    auto& item = inv.equipped[static_cast<size_t>(slot)];
    if (!item.valid() || !item.broken)
        return false;
    item.broken = false;
    return true;
}

int Inventory::totalWeight(int characterIndex) const
{
    const auto& inv = getInventory(characterIndex);
    int weight = 0;
    for (const auto& item : inv.equipped)
    {
        if (item.valid())
            weight += getItemWeight(item.itemId);
    }
    for (const auto& item : inv.backpack)
    {
        if (item.valid())
            weight += getItemWeight(item.itemId);
    }
    return weight;
}

int Inventory::totalValue(int characterIndex) const
{
    const auto& inv = getInventory(characterIndex);
    int value = 0;
    auto addValue = [&](const Item& item)
    {
        if (!item.valid())
            return;
        auto* def = getItemDef(item.itemId);
        if (def)
            value += def->value;
    };
    for (const auto& item : inv.equipped)
        addValue(item);
    for (const auto& item : inv.backpack)
        addValue(item);
    return value;
}

std::optional<Inventory::ItemLocation> Inventory::findItem(int itemId) const
{
    for (int c = 0; c < 4; c++)
    {
        const auto& inv = getInventory(c);
        for (size_t i = 0; i < inv.equipped.size(); i++)
        {
            if (inv.equipped[i].itemId == itemId)
                return ItemLocation{c, true, static_cast<int>(i)};
        }
        for (int i = 0; i < kBackpackSlots; i++)
        {
            if (inv.backpack[static_cast<size_t>(i)].itemId == itemId)
                return ItemLocation{c, false, i};
        }
    }
    return std::nullopt;
}

bool Inventory::removeItem(int itemId)
{
    auto loc = findItem(itemId);
    if (!loc)
        return false;

    auto& inv = getInventory(loc->characterIndex);
    if (loc->equipped)
    {
        inv.equipped[static_cast<size_t>(loc->slotIndex)] = Item{};
    }
    else
    {
        inv.backpack[static_cast<size_t>(loc->slotIndex)] = Item{};
    }
    return true;
}

bool Inventory::giveItem(const Item& item)
{
    // Try each character in order
    for (int c = 0; c < 4; c++)
    {
        if (addToBackpack(c, item))
            return true;
    }
    logger_.warning("No space to give item " + std::to_string(item.itemId));
    return false;
}

ItemCategory Inventory::categorizeItem(const formats::ItemEntry& entry) const
{
    const auto& es = entry.equipStat;
    if (es == "weapon" || es == "wand")
        return ItemCategory::Weapon;
    if (es == "armor")
        return ItemCategory::Armor;
    if (es == "shield")
        return ItemCategory::Shield;
    if (es == "helm")
        return ItemCategory::Helmet;
    if (es == "belt")
        return ItemCategory::Belt;
    if (es == "cloak")
        return ItemCategory::Cloak;
    if (es == "gauntlets")
        return ItemCategory::Gauntlets;
    if (es == "boots")
        return ItemCategory::Boots;
    if (es == "amulet")
        return ItemCategory::Amulet;
    if (es == "ring")
        return ItemCategory::Ring;
    if (es == "potion")
        return ItemCategory::Potion;
    if (es == "scroll" || es == "spell")
        return ItemCategory::Scroll;
    if (es == "reagent")
        return ItemCategory::Reagent;
    if (es == "gem")
        return ItemCategory::Gem;
    if (es == "gold")
        return ItemCategory::Gold;
    return ItemCategory::Misc;
}

EquipSlot Inventory::mapEquipSlot(const formats::ItemEntry& entry) const
{
    auto cat = categorizeItem(entry);
    switch (cat)
    {
    case ItemCategory::Weapon:
        return EquipSlot::MainHand;
    case ItemCategory::Armor:
        return EquipSlot::Armor;
    case ItemCategory::Shield:
        return EquipSlot::OffHand;
    case ItemCategory::Helmet:
        return EquipSlot::Helmet;
    case ItemCategory::Belt:
        return EquipSlot::Belt;
    case ItemCategory::Cloak:
        return EquipSlot::Cloak;
    case ItemCategory::Gauntlets:
        return EquipSlot::Gauntlets;
    case ItemCategory::Boots:
        return EquipSlot::Boots;
    case ItemCategory::Amulet:
        return EquipSlot::Amulet;
    case ItemCategory::Ring:
        return EquipSlot::Ring1; // Will find first free ring slot during equip
    default:
        return EquipSlot::Count; // Not equippable
    }
}

} // namespace runeharbor::game
