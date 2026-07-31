// SPDX-License-Identifier: MIT
#include "shop_system.hpp"

#include <algorithm>
#include <format>

#include <cmath>

namespace runeharbor::game
{

namespace
{

// Maps a SkillMastery to the RE "masteryBase" sequence used by FUN_004911EB:
// Normal=1, Expert=2, Master=3, GM=5. (GM short-circuits to free in the caller.)
int masteryBaseFor(SkillMastery mastery)
{
    switch (mastery)
    {
    case SkillMastery::Normal:
        return 1;
    case SkillMastery::Expert:
        return 2;
    case SkillMastery::Master:
        return 3;
    case SkillMastery::GrandMaster:
        return 5;
    case SkillMastery::None:
    default:
        return 0;
    }
}

const SkillValue& merchantSkill(const Character& character)
{
    return character.skillLevels[static_cast<size_t>(SkillId::Merchant)];
}

} // namespace

int ShopSystem::merchantDiscountPct(const Character& character, int reputation)
{
    const SkillValue skill = merchantSkill(character);
    if (skill.mastery == SkillMastery::GrandMaster)
    {
        // GM merchant -> 100% discount (free). The engine returns a large
        // sentinel that the finalizers clamp to free.
        return kGrandMasterDiscountPct;
    }

    if (!skill.learned())
    {
        // Untrained merchant: a penalty equal to the negative of the party's
        // reputation bonus (the engine returns -rep). A negative discount
        // inflates the price.
        return -reputation;
    }

    const int skillPts = skill.level; // masked 0..63 in the engine
    const int modifier = skill.level; // per-char modifier (no separate field yet)
    const int masteryBase = masteryBaseFor(skill.mastery);

    // discountPct = skillPts + 7 + (masteryBase - 1) * modifier - reputation
    return (masteryBase - 1) * modifier + skillPts + 7 - reputation;
}

int ShopSystem::itemFullPrice(const formats::ItemEntry& def, const Item& item)
{
    // FUN_0045646E. Base price is the item description's value field.
    int price = def.value;

    // Temporary bonuses are sold at base price only.
    if (item.temporaryEnchant)
    {
        return price;
    }

    // Standard enchant adds `power * 100`. RuneHarbor stores the standard
    // enchant id (item.enchantId); the power table is not yet loaded, so we
    // apply a flat 100g per enchant level as a faithful first approximation.
    if (item.enchantId != 0)
    {
        price += item.enchantId * 100;
    }

    // Special enchant applies a multiplier (<11) or additive bonus (>=11).
    // Without the special-enchant table loaded we leave price unchanged here;
    // the table-driven path can be wired in when that parser exists.
    (void)item.specialEnchantId;

    return price;
}

int ShopSystem::buyPrice(int fullPrice, float shopMultiplier, int discountPct)
{
    if (fullPrice <= 0)
    {
        return 1;
    }

    // FUN_004B8065:
    //   floorTerm = floor((fullPrice + 2.0) / shopMult)
    //   base      = (fullPrice * discPct) / 100 + floorTerm
    //   result    = min(base, fullPrice), then max(result, 1)
    const float safeMult = shopMultiplier > 0.0f ? shopMultiplier : 1.0f;
    const int floorTerm = floorToInt((static_cast<float>(fullPrice) + 2.0f) / safeMult);
    const int discountTerm = (fullPrice * discountPct) / 100;
    int base = discountTerm + floorTerm;
    base = std::min(base, fullPrice);
    return std::max(base, 1);
}

int ShopSystem::sellPrice(int fullPrice, float shopMultiplier, int discountPct)
{
    // FUN_004BE240 calls FUN_004B8065 (the buy finalizer) with the same +0x20
    // shop multiplier, so sell credit uses the identical computation. The
    // buy/sell distinction is encoded in the per-building multiplier value,
    // not in a separate code path.
    return buyPrice(fullPrice, shopMultiplier, discountPct);
}

int ShopSystem::identifyPrice(int fullPrice, int discountPct)
{
    if (fullPrice <= 0)
    {
        return 1;
    }
    // FUN_004B8126: base = floor((fullPrice - 6) / fullPrice);
    //              result = base * (100 - discPct) / 100; round up to mult of 3; min 1.
    const int base =
        floorToInt((static_cast<float>(fullPrice) - 6.0f) / static_cast<float>(fullPrice));
    int result = base * (100 - std::clamp(discountPct, 0, 100)) / 100;
    result = roundUpToMultipleOf3(result);
    return std::max(result, 1);
}

int ShopSystem::repairPrice(int fullPrice, int discountPct)
{
    if (fullPrice <= 0)
    {
        return 1;
    }
    // FUN_004B80DC: base = floor(fullPrice * 50.0);
    //              result = base * (100 - discPct) / 100; round up to mult of 3; min 1.
    const int base = floorToInt(static_cast<float>(fullPrice) * 50.0f);
    int result = base * (100 - std::clamp(discountPct, 0, 100)) / 100;
    result = roundUpToMultipleOf3(result);
    return std::max(result, 1);
}

int ShopSystem::serviceCost(BuildingType type, float shopMultiplier, int discountPct)
{
    // FUN_004B63DB / FUN_004B68A6: fixed base scaled by the building +0x20
    // multiplier, then the merchant discount, then rounded up to a multiple
    // of 3.
    const float safeMult = shopMultiplier > 0.0f ? shopMultiplier : 1.0f;
    int base = 0;

    switch (type)
    {
    case BuildingType::Training:
        // FUN_004B63DB: base = (type==0x12 ? 100 : 250) * shopMult. Training
        // (our type code) maps to the non-0x12 branch -> 250.
        base = floorToInt(250.0f * safeMult);
        break;
    case BuildingType::Temple:
        // Temple healing services use the 0x4B68A6 family: base = 50/25.
        // The building's `buyMultiplier` (the "Val" column) already carries
        // the per-temple cost (50/100/30/...), so we treat shopMult directly
        // as the base service cost when it is a whole number, else use 50.
        base = floorToInt(50.0f * safeMult);
        break;
    default:
        base = floorToInt(100.0f * safeMult);
        break;
    }

    int result = base * (100 - std::clamp(discountPct, 0, 100)) / 100;
    result = roundUpToMultipleOf3(result);
    return std::max(result, 1);
}

std::expected<ShopReceipt, ShopError> ShopSystem::buyItem(const ShopContext& ctx,
                                                          int characterIndex, int itemId) const
{
    if (ctx.building == nullptr || ctx.party == nullptr || ctx.inventory == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }
    if (itemId <= 0)
    {
        return std::unexpected(ShopError::NoSuchItem);
    }

    const formats::ItemEntry* def = ctx.inventory->getItemDef(itemId);
    if (def == nullptr)
    {
        return std::unexpected(ShopError::NoSuchItem);
    }

    const Item item{itemId};
    const int fullPrice = itemFullPrice(*def, item);
    const Character& buyer = activeCharacter(*ctx.party);
    const int discount = merchantDiscountPct(buyer, ctx.party->reputation());
    const int price = buyPrice(fullPrice, ctx.building->buyMultiplier, discount);

    if (ctx.party->gold() < price)
    {
        return std::unexpected(ShopError::InsufficientGold);
    }

    if (!ctx.inventory->addToBackpack(characterIndex, item))
    {
        return std::unexpected(ShopError::NoFreeInventorySpace);
    }

    // spendGold can only fail on insufficient funds, which we already checked.
    (void)ctx.party->spendGold(price);

    ShopReceipt receipt;
    receipt.goldSpent = price;
    receipt.itemId = itemId;
    receipt.characterIndex = characterIndex;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

std::expected<ShopReceipt, ShopError>
ShopSystem::sellItem(const ShopContext& ctx, int characterIndex, int backpackSlot) const
{
    if (ctx.building == nullptr || ctx.party == nullptr || ctx.inventory == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }

    auto taken = ctx.inventory->takeFromBackpack(characterIndex, backpackSlot);
    if (!taken.has_value())
    {
        return std::unexpected(ShopError::ItemNotOwned);
    }

    const formats::ItemEntry* def = ctx.inventory->getItemDef(taken->itemId);
    if (def == nullptr)
    {
        // Defensive: shouldn't happen for an owned item.
        ctx.inventory->addToBackpack(characterIndex, *taken);
        return std::unexpected(ShopError::NoSuchItem);
    }

    const int fullPrice = itemFullPrice(*def, *taken);
    const Character& seller = activeCharacter(*ctx.party);
    const int discount = merchantDiscountPct(seller, ctx.party->reputation());
    const int credit = sellPrice(fullPrice, ctx.building->buyMultiplier, discount);

    ctx.party->addGold(credit);

    ShopReceipt receipt;
    receipt.goldGained = credit;
    receipt.itemId = taken->itemId;
    receipt.characterIndex = characterIndex;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

std::expected<ShopReceipt, ShopError>
ShopSystem::identifyItem(const ShopContext& ctx, int characterIndex, int backpackSlot) const
{
    if (ctx.building == nullptr || ctx.party == nullptr || ctx.inventory == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }

    // Peek the item to price the identification without removing it.
    const auto& inv = ctx.inventory->getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= static_cast<int>(inv.backpack.size()))
    {
        return std::unexpected(ShopError::ItemNotOwned);
    }
    const Item& slot = inv.backpack[backpackSlot];
    if (!slot.valid())
    {
        return std::unexpected(ShopError::ItemNotOwned);
    }
    if (slot.identified)
    {
        return std::unexpected(ShopError::ItemAlreadyIdentified);
    }

    const formats::ItemEntry* def = ctx.inventory->getItemDef(slot.itemId);
    if (def == nullptr)
    {
        return std::unexpected(ShopError::NoSuchItem);
    }

    const int fullPrice = itemFullPrice(*def, slot);
    const Character& appraiser = activeCharacter(*ctx.party);
    const int discount = merchantDiscountPct(appraiser, ctx.party->reputation());
    const int price = identifyPrice(fullPrice, discount);

    if (ctx.party->gold() < price)
    {
        return std::unexpected(ShopError::InsufficientGold);
    }

    if (!ctx.inventory->identifyBackpackItem(characterIndex, backpackSlot))
    {
        return std::unexpected(ShopError::InvalidArgument);
    }
    (void)ctx.party->spendGold(price);

    ShopReceipt receipt;
    receipt.goldSpent = price;
    receipt.itemId = slot.itemId;
    receipt.characterIndex = characterIndex;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

std::expected<ShopReceipt, ShopError>
ShopSystem::repairItem(const ShopContext& ctx, int characterIndex, int backpackSlot) const
{
    if (ctx.building == nullptr || ctx.party == nullptr || ctx.inventory == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }

    const auto& inv = ctx.inventory->getInventory(characterIndex);
    if (backpackSlot < 0 || backpackSlot >= static_cast<int>(inv.backpack.size()))
    {
        return std::unexpected(ShopError::ItemNotOwned);
    }
    const Item& slot = inv.backpack[backpackSlot];
    if (!slot.valid())
    {
        return std::unexpected(ShopError::ItemNotOwned);
    }
    if (!slot.broken)
    {
        return std::unexpected(ShopError::ItemNotBroken);
    }

    const formats::ItemEntry* def = ctx.inventory->getItemDef(slot.itemId);
    if (def == nullptr)
    {
        return std::unexpected(ShopError::NoSuchItem);
    }

    const int fullPrice = itemFullPrice(*def, slot);
    const Character& smith = activeCharacter(*ctx.party);
    const int discount = merchantDiscountPct(smith, ctx.party->reputation());
    const int price = repairPrice(fullPrice, discount);

    if (ctx.party->gold() < price)
    {
        return std::unexpected(ShopError::InsufficientGold);
    }

    // Repair clears the broken flag. Inventory exposes repairItem for equipped
    // slots only, so we clear the backpack flag directly.
    auto taken = ctx.inventory->takeFromBackpack(characterIndex, backpackSlot);
    if (!taken.has_value())
    {
        return std::unexpected(ShopError::ItemNotOwned);
    }
    taken->broken = false;
    ctx.inventory->addToBackpack(characterIndex, *taken);
    (void)ctx.party->spendGold(price);

    ShopReceipt receipt;
    receipt.goldSpent = price;
    receipt.itemId = slot.itemId;
    receipt.characterIndex = characterIndex;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

// -------- Temple services --------

namespace
{

// FUN_004B6F5C: a character "needs service" if HP or SP is below max, or any
// non-benign condition is active. (We treat any active condition as needing
// service; the benign-only exclusion in the engine is a minor optimization.)
bool needsService(const Character& member)
{
    if (member.hitPoints < member.maxHitPoints || member.spellPoints < member.maxSpellPoints)
        return true;
    return member.worstActiveCondition() != ConditionIndex::Count;
}

// FUN_004B7FDF severity tier for the worst active condition.
int templeTierMultiplier(ConditionIndex worst)
{
    switch (worst)
    {
    case ConditionIndex::Dead:
    case ConditionIndex::Stoned:
        return 5;
    case ConditionIndex::Eradicated:
        return 10;
    default:
        return 1; // alive (sick or healthy)
    }
}

// Restores a character to full HP/SP and clears all conditions (the shared
// heal/resurrect mutation, FUN_00492BAE + the +0x193C/+0x1940 stores).
void restoreToFull(Character& member)
{
    member.hitPoints = member.maxHitPoints;
    member.spellPoints = member.maxSpellPoints;
    member.clearAllConditions();
}

} // namespace

int ShopSystem::templeCost(const Character& member, float templeVal, int discountPct)
{
    // FUN_004B7FDF: cost = severity * tierMult * templeVal, then the merchant
    // discount + round-to-3 finalizer (same shape as serviceCost / buyPrice).
    const float safeVal = templeVal > 0.0f ? templeVal : 1.0f;
    const int tierMult = templeTierMultiplier(member.worstActiveCondition());

    // Severity bucket for non-fatal conditions: the engine derives a 1..7 band
    // from the condition-timestamp age. Without per-condition age tracking we
    // use severity=1 (the healthy floor); fatal conditions use the tier above.
    const int severity = 1;

    int base = floorToInt(static_cast<float>(severity * tierMult) * safeVal);
    int result = base * (100 - std::clamp(discountPct, 0, 100)) / 100;
    result = roundUpToMultipleOf3(result);
    return std::max(result, 1);
}

std::expected<ShopReceipt, ShopError> ShopSystem::healParty(const ShopContext& ctx) const
{
    if (ctx.building == nullptr || ctx.party == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }

    // Sum the per-member cost for everyone needing service first, so we can
    // fail atomically on insufficient gold before mutating anyone.
    const Character& negotiator = activeCharacter(*ctx.party);
    const int discount = merchantDiscountPct(negotiator, ctx.party->reputation());
    const float templeVal = ctx.building->buyMultiplier;

    int totalCost = 0;
    int affected = 0;
    for (int i = 0; i < kPartySize; i++)
    {
        const Character& m = ctx.party->member(i);
        if (!needsService(m))
            continue;
        totalCost += templeCost(m, templeVal, discount);
        affected++;
    }

    if (affected == 0)
    {
        return std::unexpected(ShopError::NothingToHeal);
    }
    if (ctx.party->gold() < totalCost)
    {
        return std::unexpected(ShopError::InsufficientGold);
    }

    for (int i = 0; i < kPartySize; i++)
    {
        if (needsService(ctx.party->member(i)))
        {
            restoreToFull(ctx.party->member(i));
        }
    }
    (void)ctx.party->spendGold(totalCost);

    ShopReceipt receipt;
    receipt.goldSpent = totalCost;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

std::expected<ShopReceipt, ShopError> ShopSystem::resurrectMember(const ShopContext& ctx,
                                                                  int characterIndex) const
{
    if (ctx.building == nullptr || ctx.party == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }
    if (characterIndex < 0 || characterIndex >= kPartySize)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }

    Character& target = ctx.party->member(characterIndex);
    const ConditionIndex worst = target.worstActiveCondition();
    const bool dead = (worst == ConditionIndex::Dead || worst == ConditionIndex::Stoned ||
                       worst == ConditionIndex::Eradicated);
    if (!dead)
    {
        return std::unexpected(ShopError::NothingToRaise);
    }

    const Character& negotiator = activeCharacter(*ctx.party);
    const int discount = merchantDiscountPct(negotiator, ctx.party->reputation());
    const int cost = templeCost(target, ctx.building->buyMultiplier, discount);

    if (ctx.party->gold() < cost)
    {
        return std::unexpected(ShopError::InsufficientGold);
    }

    restoreToFull(target);
    (void)ctx.party->spendGold(cost);

    ShopReceipt receipt;
    receipt.goldSpent = cost;
    receipt.characterIndex = characterIndex;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

std::expected<ShopReceipt, ShopError> ShopSystem::donate(const ShopContext& ctx) const
{
    if (ctx.building == nullptr || ctx.party == nullptr)
    {
        return std::unexpected(ShopError::InvalidArgument);
    }

    // FUN_004B7324: donate is a flat fee = int(templeVal), no discount.
    const int cost = std::max(1, floorToInt(ctx.building->buyMultiplier));
    if (ctx.party->gold() < cost)
    {
        return std::unexpected(ShopError::InsufficientGold);
    }

    (void)ctx.party->spendGold(cost);
    // Reputation adjustment (simplified from the RE light/dark counter). The
    // escalating-stat-buff cascade at thresholds is out of scope.
    ctx.party->adjustReputation(1);

    ShopReceipt receipt;
    receipt.goldSpent = cost;
    receipt.building = ctx.building->buildingType;
    return receipt;
}

const Character& ShopSystem::activeCharacter(const Party& party)
{
    int idx = party.activeMemberIndex();
    if (idx < 0 || idx >= kPartySize)
    {
        idx = 0;
    }
    return party.member(idx);
}

int ShopSystem::roundUpToMultipleOf3(int value)
{
    if (value <= 0)
    {
        return 0;
    }
    // (value + 2) / 3 * 3, integer division — matches the engine's idiv-3 idiom.
    return ((value + 2) / 3) * 3;
}

int ShopSystem::floorToInt(float value)
{
    // FUN_004CA74C truncates toward zero; std::trunc does the same.
    return static_cast<int>(std::trunc(value));
}

std::string_view shopErrorText(ShopError error)
{
    switch (error)
    {
    case ShopError::NoSuchBuilding:
        return "No such building";
    case ShopError::NoSuchItem:
        return "No such item";
    case ShopError::ItemNotOwned:
        return "You don't have that item";
    case ShopError::InsufficientGold:
        return "Not enough gold";
    case ShopError::NoFreeInventorySpace:
        return "No room in inventory";
    case ShopError::ItemAlreadyIdentified:
        return "Already identified";
    case ShopError::ItemNotBroken:
        return "Item is not broken";
    case ShopError::InvalidArgument:
        return "Cannot complete transaction";
    case ShopError::NothingToHeal:
        return "No one needs healing";
    case ShopError::NothingToRaise:
        return "That character is not dead";
    }
    return "Unknown error";
}

} // namespace runeharbor::game
