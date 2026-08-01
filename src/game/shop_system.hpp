// SPDX-License-Identifier: MIT
//
// Shop / building economy transactions. The pricing formulas and the merchant
// discount are RE-derived from MM7-Rel.exe and documented in
// docs/re/29-shops-and-economy.md section 3:
//
//   - FUN_004911EB  merchant discount percent
//   - FUN_0045646E  item full price (base value + enchant adjustments)
//   - FUN_004B8065  buy price finalizer
//   - FUN_004B8126  identify price finalizer
//   - FUN_004B80DC  repair price finalizer
//   - FUN_004B63DB / FUN_004B68A6  type-gated service costs (temple/training)
//
// This is pure game logic with no SDL dependency, so it is fully unit-testable.
// Mutators compose the existing Inventory / Party APIs.

#pragma once

#include <expected>
#include <string>
#include <vector>

#include "../formats/items_parser.hpp"
#include "../formats/two_d_events_parser.hpp"
#include "../util/error.hpp"
#include "building_type.hpp"
#include "character.hpp"
#include "inventory.hpp"
#include "party.hpp"

namespace runeharbor::game
{

// Reason a shop transaction could not complete.
enum class ShopError
{
    NoSuchBuilding,
    NoSuchItem,
    ItemNotOwned,
    InsufficientGold,
    NoFreeInventorySpace,
    ItemAlreadyIdentified,
    ItemNotBroken,
    InvalidArgument,
    // Temple service errors.
    NothingToHeal,  // nobody in the party needs healing/curing
    NothingToRaise, // resurrect target is not dead/stoned/eradicated
    // Training / travel errors.
    NothingToLearn,    // character lacks the XP to level up
    NoSuchDestination, // travel building has no destinations / bad index
};

// Outcome of a successful transaction.
struct ShopReceipt
{
    int goldSpent = 0;       // gold removed from the party (buy/repair/identify)
    int goldGained = 0;      // gold added to the party (sell)
    int itemId = 0;          // item involved (0 for service-only receipts)
    int characterIndex = -1; // who received/lost the item
    BuildingType building = BuildingType::None;
};

// Required context for pricing. The building entry supplies the buy multiplier
// (struct +0x20) and the type code; the character supplies the Merchant skill
// and reputation; the party supplies gold.
struct ShopContext
{
    const formats::TwoDEventEntry* building = nullptr;
    Party* party = nullptr;
    Inventory* inventory = nullptr;
};

class ShopSystem
{
  public:
    ShopSystem() = default;

    // -------- Pricing (pure functions, fully RE-derived) --------

    // FUN_004911EB: percentage discount from the active character's Merchant
    // skill + reputation. GM returns a sentinel (see kGrandMasterDiscountPct)
    // meaning "free".
    static constexpr int kGrandMasterDiscountPct = 10000;
    static int merchantDiscountPct(const Character& character, int reputation);

    // FUN_0045646E: base gold value of an item including enchant adjustments.
    // Requires the item table (for base value + enchant lookup).
    static int itemFullPrice(const formats::ItemEntry& def, const Item& item);

    // FUN_004B8065: final buy price charged to the player. sellPrice() reuses
    // the same shape per the RE finding that buy/sell share the +0x20
    // multiplier (the buy/sell distinction is authored per-building, not a
    // separate code path).
    static int buyPrice(int fullPrice, float shopMultiplier, int discountPct);
    static int sellPrice(int fullPrice, float shopMultiplier, int discountPct);

    // FUN_004B8126 / FUN_004B80DC.
    static int identifyPrice(int fullPrice, int discountPct);
    static int repairPrice(int fullPrice, int discountPct);

    // FUN_004B63DB / FUN_004B68A6: fixed service cost for temples / training,
    // gated on the building type and scaled by the building's +0x20 multiplier.
    static int serviceCost(BuildingType type, float shopMultiplier, int discountPct);

    // -------- Transactions (mutate party/inventory) --------

    // Buy an item into the active character's backpack. Returns the receipt or
    // an error (insufficient gold / no space).
    std::expected<ShopReceipt, ShopError> buyItem(const ShopContext& ctx, int characterIndex,
                                                  int itemId) const;

    // Sell a specific backpack slot from a character. Returns the gold gained.
    std::expected<ShopReceipt, ShopError> sellItem(const ShopContext& ctx, int characterIndex,
                                                   int backpackSlot) const;

    // Identify an item in the active character's backpack.
    std::expected<ShopReceipt, ShopError> identifyItem(const ShopContext& ctx, int characterIndex,
                                                       int backpackSlot) const;

    // Repair a broken item in the active character's backpack.
    std::expected<ShopReceipt, ShopError> repairItem(const ShopContext& ctx, int characterIndex,
                                                     int backpackSlot) const;

    // -------- Temple services (RE-derived, see docs/re/30-...) --------

    // Per-character temple cost, tiered by condition severity (FUN_004B7FDF).
    //   severity: worst in {Dead,Stoned} -> tierMult 5; Eradicated -> 10;
    //             otherwise the alive/sick tier (1). A healthy character (no
    //             condition, HP/SP full) costs templeVal * 1 as the floor.
    static int templeCost(const Character& member, float templeVal, int discountPct);

    // Heal/Cure the whole party: every member needing service is restored to
    // full HP/SP and cleared of all conditions. Charges the summed per-member
    // cost (the RE "Heal All" action, FUN_004B6FC1 mode 86).
    std::expected<ShopReceipt, ShopError> healParty(const ShopContext& ctx) const;

    // Resurrect one dead/stoned/eradicated member to full HP/SP with all
    // conditions cleared (FUN_004B6FC1 mode 10). Eradicated costs 10x, but is
    // still raisable.
    std::expected<ShopReceipt, ShopError> resurrectMember(const ShopContext& ctx,
                                                          int characterIndex) const;

    // Donate a flat int(templeVal) gold for a small reputation gain (mode 11).
    // The escalating-stat-buff cascade is out of scope for this pass.
    std::expected<ShopReceipt, ShopError> donate(const ShopContext& ctx) const;

    // -------- Training service (RE-derived, see docs/re/35-...) --------

    // Level-up training cost (FUN_004B4673): round(level * shopMult * classTier),
    // then the merchant-discount finalizer. classTier is 1/2/3 by promotion.
    static int trainingCost(const Character& member, float shopMult, int discountPct);

    // Spend gold to level up one character (FUN_004B4673). Requires the
    // character to already have enough XP (canLevelUp); training is the
    // level-up button, not an XP grant.
    std::expected<ShopReceipt, ShopError> trainMember(const ShopContext& ctx,
                                                      int characterIndex) const;

    // -------- Travel service (stables/boats, FUN_004B68A6) --------

    // Flat per-trip cost: (Stables?50:25) * shopMult, then the finalizer.
    static int travelCost(BuildingType type, float shopMult, int discountPct);

  private:
    // Resolves the active character for merchant-skill purposes: the party's
    // active member, clamped.
    static const Character& activeCharacter(const Party& party);

    // Rounds up to the next multiple of 3 (the `idiv 3` idiom in the repair /
    // identify / service-cost finalizers).
    static int roundUpToMultipleOf3(int value);

    // Floors a float to int using the same "truncate toward zero" semantics as
    // the FUN_004CA74C helper used by the price finalizers.
    static int floorToInt(float value);
};

// Human-readable name for a ShopError (for status-line messages).
std::string_view shopErrorText(ShopError error);

} // namespace runeharbor::game
