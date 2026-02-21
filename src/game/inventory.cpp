// SPDX-License-Identifier: MIT
#include "inventory.hpp"

#include <algorithm>
#include <charconv>
#include <optional>

#include "../util/string_utils.hpp"

namespace runeharbor::game
{

namespace
{
EquipType mapEquipTypeCode(int code)
{
    switch (code)
    {
    case 0:
        return EquipType::Weapon1H;
    case 1:
        return EquipType::Weapon2H;
    case 2:
        return EquipType::Missile;
    case 3:
        return EquipType::Armor;
    case 4:
        return EquipType::Shield;
    case 5:
        return EquipType::Helmet;
    case 6:
        return EquipType::Belt;
    case 7:
        return EquipType::Cloak;
    case 8:
        return EquipType::Gauntlets;
    case 9:
        return EquipType::Boots;
    case 10:
        return EquipType::Ring;
    case 11:
        return EquipType::Amulet;
    case 12:
        return EquipType::Wand;
    case 13:
        return EquipType::Reagent;
    case 14:
        return EquipType::Potion;
    case 15:
        return EquipType::SpellScroll;
    case 16:
        return EquipType::Book;
    case 17:
        return EquipType::MessageScroll;
    case 18:
        return EquipType::Deed;
    case 19:
        return EquipType::GoldItem;
    case 20:
    default:
        return EquipType::None;
    }
}

std::optional<int> parseIntStrict(std::string_view raw)
{
    int value = 0;
    const char* begin = raw.data();
    const char* end = raw.data() + raw.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
    {
        return std::nullopt;
    }
    return value;
}
} // namespace

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

EquipType Inventory::getEquipType(int itemId) const
{
    auto* def = getItemDef(itemId);
    if (!def)
        return EquipType::None;
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
    case EquipType::Weapon1H:
    case EquipType::Wand:
        return 3 + def->material;
    case EquipType::Weapon2H:
        return 5 + def->material;
    case EquipType::Missile:
        return 3 + def->material;
    case EquipType::Armor:
        return 5 + def->material * 2;
    case EquipType::Shield:
        return 3;
    case EquipType::Helmet:
        return 2;
    case EquipType::Boots:
    case EquipType::Gauntlets:
        return 1;
    case EquipType::Belt:
    case EquipType::Cloak:
        return 1;
    case EquipType::Ring:
    case EquipType::Amulet:
        return 0;
    case EquipType::Potion:
    case EquipType::SpellScroll:
    case EquipType::Reagent:
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
    if (characterIndex < 0 || characterIndex >= static_cast<int>(inventories_.size()))
    {
        return false;
    }
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

EquipType Inventory::categorizeItem(const formats::ItemEntry& entry) const
{
    const std::string es = util::toLower(util::trim(entry.equipStat));
    if (es.empty())
    {
        return EquipType::None;
    }

    if (auto code = parseIntStrict(es); code.has_value())
    {
        return mapEquipTypeCode(*code);
    }

    if (es == "weapon" || es == "weapon1h" || es == "weapon1or2")
        return EquipType::Weapon1H;
    if (es == "weapon2" || es == "weapon2h" || es == "twohand")
        return EquipType::Weapon2H;
    if (es == "missile")
        return EquipType::Missile;
    if (es == "armor")
        return EquipType::Armor;
    if (es == "shield")
        return EquipType::Shield;
    if (es == "helm" || es == "helmet")
        return EquipType::Helmet;
    if (es == "belt")
        return EquipType::Belt;
    if (es == "cloak")
        return EquipType::Cloak;
    if (es == "gauntlets")
        return EquipType::Gauntlets;
    if (es == "boots")
        return EquipType::Boots;
    if (es == "ring")
        return EquipType::Ring;
    if (es == "amulet")
        return EquipType::Amulet;
    if (es == "wand" || es == "weaponw")
        return EquipType::Wand;
    if (es == "reagent" || es == "herb")
        return EquipType::Reagent;
    if (es == "potion" || es == "bottle")
        return EquipType::Potion;
    if (es == "scroll" || es == "spell" || es == "spellscroll" || es == "sscroll")
        return EquipType::SpellScroll;
    if (es == "book")
        return EquipType::Book;
    if (es == "message" || es == "mscroll")
        return EquipType::MessageScroll;
    if (es == "deed")
        return EquipType::Deed;
    if (es == "gold" || es == "golditem")
        return EquipType::GoldItem;
    return EquipType::None;
}

EquipSlot Inventory::mapEquipSlot(const formats::ItemEntry& entry) const
{
    auto cat = categorizeItem(entry);
    switch (cat)
    {
    case EquipType::Weapon1H:
    case EquipType::Weapon2H:
    case EquipType::Wand:
        return EquipSlot::MainHand;
    case EquipType::Missile:
        return EquipSlot::Bow;
    case EquipType::Armor:
        return EquipSlot::Armor;
    case EquipType::Shield:
        return EquipSlot::OffHand;
    case EquipType::Helmet:
        return EquipSlot::Helmet;
    case EquipType::Belt:
        return EquipSlot::Belt;
    case EquipType::Cloak:
        return EquipSlot::Cloak;
    case EquipType::Gauntlets:
        return EquipSlot::Gauntlets;
    case EquipType::Boots:
        return EquipSlot::Boots;
    case EquipType::Amulet:
        return EquipSlot::Amulet;
    case EquipType::Ring:
        return EquipSlot::Ring1; // Will find first free ring slot during equip
    default:
        return EquipSlot::Count; // Not equippable
    }
}

} // namespace runeharbor::game
