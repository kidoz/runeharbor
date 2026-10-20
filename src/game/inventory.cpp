// SPDX-License-Identifier: MIT
#include "inventory.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>

#include "../util/string_utils.hpp"
#include "party.hpp"
#include "spells.hpp"

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

// Armor items store their base AC in Mod1 (a flat number) and a quality bonus
// in Mod2; weapons use Mod1 for damage dice. Decide by item type, not slot:
// the off hand can hold a dagger/sword in MM7 as well as a shield.
bool isArmorEquipType(EquipType type)
{
    switch (type)
    {
    case EquipType::Armor:
    case EquipType::Shield:
    case EquipType::Helmet:
    case EquipType::Belt:
    case EquipType::Cloak:
    case EquipType::Gauntlets:
    case EquipType::Boots:
        return true;
    default:
        return false;
    }
}

// Weapon Mod1 is dice ("3d3"); armor Mod1 is a flat AC number.
int parseFlatModifier(std::string_view text)
{
    const std::string trimmed = util::trim(std::string(text));
    if (trimmed.empty())
    {
        return 0;
    }
    int value = 0;
    const char* begin = trimmed.data();
    const char* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
    {
        return 0;
    }
    return value;
}

// Column order in STDITEMS.TXT: Arm, Shld, Helm, Belt, Cape, Gaunt, Boot,
// Ring, Amul.
enum class StdSlot : uint8_t
{
    Arm = 0,
    Shield,
    Helm,
    Belt,
    Cape,
    Gauntlets,
    Boots,
    Ring,
    Amulet,
};

std::optional<StdSlot> standardSlotFor(EquipType type)
{
    switch (type)
    {
    case EquipType::Armor:
        return StdSlot::Arm;
    case EquipType::Shield:
        return StdSlot::Shield;
    case EquipType::Helmet:
        return StdSlot::Helm;
    case EquipType::Belt:
        return StdSlot::Belt;
    case EquipType::Cloak:
        return StdSlot::Cape;
    case EquipType::Gauntlets:
        return StdSlot::Gauntlets;
    case EquipType::Boots:
        return StdSlot::Boots;
    case EquipType::Ring:
        return StdSlot::Ring;
    case EquipType::Amulet:
        return StdSlot::Amulet;
    default:
        return std::nullopt; // weapons have no standard-enchant columns
    }
}

// Standard enchantment rows from STDITEMS.TXT, indexed by enchantId - 1.
// Columns follow StdSlot order (Arm, Shld, Helm, Belt, Cape, Gaunt, Boot,
// Ring, Amul). Item.enchantId is 1-based; 0 means no enchantment. The shipped
// table is truncated after Fire Resistance, so later rows are not modelled.
constexpr std::array<std::array<int, 9>, 11> kStandardEnchantValues = {{
    {5, 0, 10, 10, 10, 10, 5, 10, 10},  // 1  of Might
    {5, 0, 10, 10, 10, 10, 5, 10, 10},  // 2  of Thought
    {5, 0, 10, 10, 10, 10, 10, 10, 10}, // 3  of Charm
    {5, 0, 10, 10, 10, 10, 5, 10, 10},  // 4  of Vigor
    {5, 0, 10, 10, 10, 10, 5, 10, 10},  // 5  of Precision
    {5, 0, 10, 10, 10, 10, 25, 10, 10}, // 6  of Speed
    {5, 0, 10, 10, 10, 10, 10, 10, 10}, // 7  of Luck
    {10, 5, 5, 0, 10, 5, 10, 10, 10},   // 8  of Health
    {10, 5, 10, 0, 10, 10, 10, 10, 10}, // 9  of Magic
    {20, 20, 10, 0, 10, 10, 5, 10, 10}, // 10 of Defense
    {10, 10, 5, 0, 10, 15, 5, 10, 10},  // 11 of Fire Resistance
}};

void applyStandardEnchantment(EquipmentBonuses& bonuses, int enchantId, std::optional<StdSlot> slot)
{
    if (enchantId <= 0 || enchantId > static_cast<int>(kStandardEnchantValues.size()) ||
        !slot.has_value())
    {
        return;
    }

    const int value =
        kStandardEnchantValues[static_cast<size_t>(enchantId - 1)][static_cast<size_t>(*slot)];

    switch (enchantId)
    {
    case 1:
        bonuses.statBonus[0] += value; // Might
        break;
    case 2:
        bonuses.statBonus[1] += value; // Intellect
        break;
    case 3:
        bonuses.statBonus[2] += value; // Personality
        break;
    case 4:
        bonuses.statBonus[3] += value; // Endurance
        break;
    case 5:
        bonuses.statBonus[4] += value; // Accuracy
        break;
    case 6:
        bonuses.statBonus[5] += value; // Speed
        break;
    case 7:
        bonuses.statBonus[6] += value; // Luck
        break;
    case 8:
        bonuses.hitPoints += value;
        break;
    case 9:
        bonuses.spellPoints += value;
        break;
    case 10:
        bonuses.armorClass += value;
        break;
    case 11:
        bonuses.resistanceBonus[0] += value; // Fire
        break;
    default:
        break;
    }
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

std::vector<formats::ItemEntry> Inventory::itemTable() const
{
    std::vector<formats::ItemEntry> out;
    out.reserve(itemDefs_.size());
    for (const auto& [id, def] : itemDefs_)
    {
        out.push_back(def);
    }
    return out;
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

std::optional<SkillId> Inventory::requiredSkillForItem(int itemId) const
{
    // MM7 gates equipping on the item's weaponSkill byte (ItemDesc +0x21).
    // RuneHarbor's data carries this via ItemEntry.skillGroup (the skill family
    // column in items.txt): Sword/Dagger/Axe/Spear/Staff/Mace for weapons,
    // Shield for shields, Leather/Chain/Plate for armor, Bow for bows.
    // Items with skillGroup "Misc" (rings, amulets, cloaks, potions, ...)
    // have skillId >= 37 in the binary and are never skill-gated.
    const auto* def = getItemDef(itemId);
    if (def == nullptr)
    {
        return std::nullopt;
    }

    // First try the explicit skillGroup.
    if (auto skill = skillIdFromName(def->skillGroup))
    {
        return skill;
    }
    // The "Bow" equipStat (Missile) maps to the Bow skill even when skillGroup
    // is generic.
    const EquipType eqType = getEquipType(itemId);
    if (eqType == EquipType::Missile)
    {
        return SkillId::Bow;
    }
    return std::nullopt;
}

bool Inventory::meetsSkillRequirement(int characterIndex, int itemId) const
{
    // FUN_004926F8: if the item requires no skill, allowed; else the character
    // must have that skill learned.
    const auto required = requiredSkillForItem(itemId);
    if (!required.has_value())
    {
        return true;
    }
    if (party_ == nullptr)
    {
        // No party wired in: cannot verify skills, so allow (slot-validity only).
        return true;
    }
    if (characterIndex < 0 || characterIndex >= kPartySize)
    {
        return false;
    }
    const Character& c = party_->member(characterIndex);
    return c.skillLevels[static_cast<size_t>(*required)].learned();
}

bool Inventory::canEquip(int characterIndex, int itemId) const
{
    if (characterIndex < 0 || characterIndex >= static_cast<int>(inventories_.size()))
    {
        return false;
    }
    auto slot = getEquipSlot(itemId);
    if (slot == EquipSlot::Count)
    {
        return false;
    }
    return meetsSkillRequirement(characterIndex, itemId);
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

    // MM7 skill gate (FUN_004926F8): refuse to equip if the character lacks
    // the required skill.
    if (!meetsSkillRequirement(characterIndex, item.itemId))
    {
        logger_.info("Character " + std::to_string(characterIndex) +
                     " lacks the skill to equip item " + std::to_string(item.itemId));
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
        // Find a free backpack slot (other than the source slot) for the
        // displaced item.
        int freeSlot = -1;
        for (int i = 0; i < kBackpackSlots; i++)
        {
            if (i != backpackSlot && !inv.backpack[static_cast<size_t>(i)].valid())
            {
                freeSlot = i;
                break;
            }
        }
        if (freeSlot < 0)
        {
            // No other free slot: swap into the source slot.
            inv.backpack[static_cast<size_t>(backpackSlot)] = inv.equipped[slotIdx];
            inv.equipped[slotIdx] = item;
            refreshEquipmentBonuses(characterIndex);
            return true;
        }
        inv.backpack[static_cast<size_t>(freeSlot)] = inv.equipped[slotIdx];
    }

    inv.equipped[slotIdx] = item;
    inv.backpack[static_cast<size_t>(backpackSlot)] = Item{};
    refreshEquipmentBonuses(characterIndex);
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
            refreshEquipmentBonuses(characterIndex);
            return true;
        }
    }

    logger_.warning("No free backpack slot to unequip item");
    return false;
}

void Inventory::refreshEquipmentBonuses(int characterIndex)
{
    if (party_ == nullptr || characterIndex < 0 ||
        characterIndex >= static_cast<int>(inventories_.size()))
    {
        return;
    }

    EquipmentBonuses bonuses;
    const auto& inv = inventories_[static_cast<size_t>(characterIndex)];

    for (size_t i = 0; i < inv.equipped.size(); ++i)
    {
        const Item& item = inv.equipped[i];
        if (!item.valid() || item.broken)
        {
            continue; // broken gear stops contributing until repaired
        }

        const auto* def = getItemDef(item.itemId);
        if (def == nullptr)
        {
            continue;
        }

        const EquipType type = getEquipType(item.itemId);
        if (isArmorEquipType(type))
        {
            // Armor Mod1 is base AC and Mod2 is the quality bonus.
            bonuses.armorClass += std::max(0, parseFlatModifier(def->mod1) + def->mod2);
        }

        applyStandardEnchantment(bonuses, item.enchantId, standardSlotFor(type));
    }

    Character& character = party_->member(characterIndex);
    character.equipmentBonuses = bonuses;
    character.recalculateDerived();
}

void Inventory::refreshAllEquipmentBonuses()
{
    for (int i = 0; i < static_cast<int>(inventories_.size()); ++i)
    {
        refreshEquipmentBonuses(i);
    }
}

int Inventory::spellIdForScroll(int itemId) const
{
    // scroll.txt maps scroll itemId -> spell id (table at 0x723BB8 in the
    // binary). A proper scroll_parser is a follow-up; this covers the common
    // scroll ids via a small runtime table with a sensible default (the first
    // spell of the school implied by the scroll's position). Scrolls are
    // itemIds 300+; map the common ones, default to a heal-equivalent.
    static const std::unordered_map<int, int> kScrollToSpell = {
        // Healing/Aid scrolls -> Body school heal (spell 67).
        {300, 67},
        {301, 67},
        {302, 67},
        // Light damage scrolls -> Fire bolt (spell 1).
        {303, 1},
        {304, 1},
    };
    const auto it = kScrollToSpell.find(itemId);
    if (it != kScrollToSpell.end())
    {
        return it->second;
    }
    // Default: a minor heal so scrolls are never inert.
    return 67;
}

UseResult Inventory::useItem(int characterIndex, int backpackSlot, int targetCharIndex)
{
    UseResult result;
    if (party_ == nullptr || characterIndex < 0 || characterIndex >= kPartySize ||
        targetCharIndex < 0 || targetCharIndex >= kPartySize)
    {
        result.message = "Invalid target";
        return result;
    }

    auto& inv = getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= kBackpackSlots)
    {
        result.message = "Invalid item slot";
        return result;
    }
    Item& item = inv.backpack[static_cast<size_t>(backpackSlot)];
    if (!item.valid())
    {
        result.message = "No item there";
        return result;
    }

    const EquipType kind = getEquipType(item.itemId);
    const int power = item.chargeCount > 0 ? item.chargeCount : 5; // potion magnitude

    switch (kind)
    {
    case EquipType::Potion:
    {
        // FUN_004680F1 potion branch: 52-case table by itemId (220..271).
        // 220=heal, 221=cure-poison(cond 6), 222=mana; others default to heal.
        Character& target = party_->member(targetCharIndex);
        switch (item.itemId)
        {
        case 222: // mana potion: restore SP directly (+0x1940 writes).
            target.spellPoints = std::min(target.maxSpellPoints, target.spellPoints + power + 10);
            result.spRestored = power + 10;
            result.message = std::format("Restored {} spell points.", result.spRestored);
            break;
        case 221: // cure poison (condition index 6 = Poison1).
            if (target.hasCondition(ConditionIndex::Poison1))
            {
                target.clearCondition(ConditionIndex::Poison1);
                result.message = "Cured poison.";
            }
            else
            {
                result.message = "Had no poison to cure.";
            }
            break;
        default: // 220 heal + all other potions -> heal via the HP-restore
                 // primitive (FUN_0048DB9F, power+10). Route through
                 // castScriptSpell when a SpellSystem is available so the heal
                 // also clears Unconscious; else apply directly.
            if (spellSystem_ != nullptr)
            {
                const int healSpell = 67; // Body school heal
                auto r = spellSystem_->castScriptSpell(healSpell, power + 10, targetCharIndex);
                result.hpHealed = r.healing;
                result.message = r.description;
            }
            else
            {
                const int before = target.hitPoints;
                target.hitPoints = std::min(target.maxHitPoints, target.hitPoints + power + 10);
                if (target.hitPoints > 0)
                    target.clearCondition(ConditionIndex::Unconscious);
                result.hpHealed = std::max(0, target.hitPoints - before);
                result.message = std::format("Restored {} HP.", result.hpHealed);
            }
            break;
        }
        result.used = true;
        result.consumed = true;
        inv.backpack[static_cast<size_t>(backpackSlot)] = Item{};
        return result;
    }
    case EquipType::SpellScroll:
    {
        if (spellSystem_ == nullptr)
        {
            result.message = "Cannot cast scroll here";
            return result;
        }
        const int spellId = spellIdForScroll(item.itemId);
        auto r = spellSystem_->castScriptSpell(spellId, power, targetCharIndex);
        result.used = r.success;
        result.hpHealed = r.healing;
        result.message = r.description;
        result.consumed = true;
        inv.backpack[static_cast<size_t>(backpackSlot)] = Item{};
        return result;
    }
    case EquipType::Book:
    {
        // Books are itemIds 400+; book index = itemId - 400. Learn the spell
        // (spell id = bookIndex+1 for this pass) gated on school skill.
        Character& reader = party_->member(targetCharIndex);
        const int bookIndex = std::max(0, item.itemId - 400);
        const int spellId = bookIndex + 1;

        // Resolve the school from the loaded spell table; fall back to the
        // contiguous 11-spell-per-school ID layout when the table is absent.
        SkillId school = SkillId::Fire;
        int spellLevel = 0; // 0 = derive from the spell id layout
        if (spellSystem_)
        {
            if (const auto* info = spellSystem_->getSpell(spellId))
            {
                school = schoolToSkill(info->school);
                spellLevel = info->level;
            }
        }
        else
        {
            static constexpr SkillId kSchoolSkills[] = {
                SkillId::Fire, SkillId::Air,  SkillId::Water, SkillId::Earth, SkillId::Spirit,
                SkillId::Mind, SkillId::Body, SkillId::Light, SkillId::Dark,
            };
            const int schoolBase = std::clamp((spellId - 1) / 11, 0, 8);
            school = kSchoolSkills[schoolBase];
            spellLevel = (spellId - 1) % 11 + 1;
        }

        if (reader.learnSpell(spellId, school, spellLevel))
        {
            result.spellLearned = spellId;
            result.used = true;
            result.consumed = true;
            result.message = std::format("Learned spell #{}.", spellId);
            inv.backpack[static_cast<size_t>(backpackSlot)] = Item{};
        }
        else
        {
            result.message = "Cannot learn — school not known or already known.";
        }
        return result;
    }
    case EquipType::MessageScroll:
    {
        // FUN_00467F4C: display text, NOT consumed.
        result.used = true;
        result.consumed = false;
        result.message = std::format("You read the scroll (item #{}).", item.itemId);
        return result;
    }
    default:
        result.message = "Cannot use that item.";
        return result;
    }
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
    refreshEquipmentBonuses(characterIndex);
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
        refreshEquipmentBonuses(loc->characterIndex);
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

int startingItemForSkill(SkillId skill)
{
    switch (skill)
    {
    case SkillId::Staff:
        return 61;
    case SkillId::Sword:
        return 1;
    case SkillId::Dagger:
        return 15;
    case SkillId::Axe:
        return 23;
    case SkillId::Spear:
        return 31;
    case SkillId::Bow:
        return 47;
    case SkillId::Mace:
        return 50;
    case SkillId::Shield:
        return 84;
    case SkillId::Leather:
        return 66;
    case SkillId::Chain:
        return 71;
    case SkillId::Plate:
        return 76;
    case SkillId::Fire:
        return 401;
    case SkillId::Air:
        return 412;
    case SkillId::Water:
        return 423;
    case SkillId::Earth:
        return 434;
    case SkillId::Spirit:
        return 445;
    case SkillId::Mind:
        return 456;
    case SkillId::Body:
        return 467;
    // The original routes all of these to the same case (table value 0x12).
    case SkillId::ItemId:
    case SkillId::Repair:
    case SkillId::Meditation:
    case SkillId::Perception:
    case SkillId::DisarmTrap:
    case SkillId::Learning:
        return 220;
    case SkillId::Dodging:
        return 115;
    case SkillId::Unarmed:
        return 110;
    // Blaster, Light, Dark, Merchant, BodyBuilding, Diplomacy, Thievery,
    // MonsterLore, Armsmaster, Stealing and Alchemy fall through to the
    // original's default case, which grants nothing.
    default:
        return 0;
    }
}

void grantStartingEquipment(Inventory& inventory, int characterIndex, const Character& character)
{
    for (size_t i = 0; i < character.skillLevels.size(); i++)
    {
        if (!character.skillLevels[i].learned())
        {
            continue;
        }
        const int itemId = startingItemForSkill(static_cast<SkillId>(i));
        if (itemId <= 0)
        {
            continue;
        }
        Item item;
        item.itemId = itemId;
        item.identified = true;
        inventory.addToBackpack(characterIndex, item);
    }
}

} // namespace runeharbor::game
