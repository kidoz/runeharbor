// SPDX-License-Identifier: MIT
//
// Unit tests for the shop economy engine. The expected values are computed by
// hand from the RE-derived formulas in docs/shops-and-economy.md
// section 3, so these tests pin the implementation to the original engine's
// arithmetic.
#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/items_parser.hpp"
#include "../../src/formats/two_d_events_parser.hpp"
#include "../../src/game/character.hpp"
#include "../../src/game/inventory.hpp"
#include "../../src/game/party.hpp"
#include "../../src/game/shop_system.hpp"
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

ItemEntry makeItemDef(int id, int value)
{
    ItemEntry e;
    e.id = id;
    e.name = "item";
    e.value = value;
    return e;
}

runeharbor::formats::TwoDEventEntry makeShop(BuildingType type, float buyMult)
{
    runeharbor::formats::TwoDEventEntry b;
    b.id = 1;
    b.buildingType = type;
    b.buyMultiplier = buyMult;
    return b;
}

Character untrainedCharacter()
{
    Character c; // Merchant skill not learned by default.
    return c;
}

Character merchantCharacter(SkillMastery mastery, int level)
{
    Character c;
    c.skillLevels[static_cast<size_t>(SkillId::Merchant)] =
        SkillValue{static_cast<uint8_t>(level), mastery};
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// Pure pricing formulas (FUN_004B8065 / 004B8126 / 004B80DC / 004911EB)
// ---------------------------------------------------------------------------

TEST_CASE("Merchant discount: untrained is the negative reputation penalty", "[shop]")
{
    // RE: untrained merchant returns -reputation.
    const auto c = untrainedCharacter();
    REQUIRE(ShopSystem::merchantDiscountPct(c, 0) == 0);
    REQUIRE(ShopSystem::merchantDiscountPct(c, 5) == -5);
}

TEST_CASE("Merchant discount: trained character follows skillPts+7+(m-1)*mod-rep", "[shop]")
{
    // Normal mastery, level 5, reputation 0:
    //   masteryBase=1 -> (1-1)*5 + 5 + 7 - 0 = 12
    const auto normal = merchantCharacter(SkillMastery::Normal, 5);
    REQUIRE(ShopSystem::merchantDiscountPct(normal, 0) == 12);

    // Expert mastery, level 5, reputation 0:
    //   masteryBase=2 -> (2-1)*5 + 5 + 7 - 0 = 17
    const auto expert = merchantCharacter(SkillMastery::Expert, 5);
    REQUIRE(ShopSystem::merchantDiscountPct(expert, 0) == 17);

    // Master mastery, level 10, reputation 3:
    //   masteryBase=3 -> (3-1)*10 + 10 + 7 - 3 = 34
    const auto master = merchantCharacter(SkillMastery::Master, 10);
    REQUIRE(ShopSystem::merchantDiscountPct(master, 3) == 34);
}

TEST_CASE("Merchant discount: Grand Master is free (sentinel)", "[shop]")
{
    const auto gm = merchantCharacter(SkillMastery::GrandMaster, 1);
    REQUIRE(ShopSystem::merchantDiscountPct(gm, 0) == ShopSystem::kGrandMasterDiscountPct);
}

TEST_CASE("Buy price: floor term and discount combine, clamped to full price", "[shop]")
{
    // fullPrice=100, shopMult=1.5, discPct=12
    //   floorTerm = floor((100 + 2) / 1.5) = floor(68.0) = 68
    //   discountTerm = (100 * 12) / 100 = 12
    //   base = 12 + 68 = 80, <= fullPrice(100), >= 1
    REQUIRE(ShopSystem::buyPrice(100, 1.5f, 12) == 80);

    // fullPrice=100, shopMult=1.0, discPct=0 (untrained, no reputation)
    //   floorTerm = floor(102/1.0) = 102
    //   discountTerm = 0
    //   base = 102 -> clamped to fullPrice 100
    REQUIRE(ShopSystem::buyPrice(100, 1.0f, 0) == 100);

    // GM discount (sentinel 10000): discountTerm = (100*10000)/100 = 10000,
    // floorTerm = 68, base = 10068 -> clamped to fullPrice 100. (GM free only
    // truly zeroes the price when the finalizer special-cases the sentinel;
    // here the clamp gives fullPrice, which is the worst case. The
    // transaction layer treats GM as free.)
    REQUIRE(ShopSystem::buyPrice(100, 1.5f, ShopSystem::kGrandMasterDiscountPct) == 100);
}

TEST_CASE("Buy price: never below 1 gold", "[shop]")
{
    REQUIRE(ShopSystem::buyPrice(1, 100.0f, 0) == 1);
    REQUIRE(ShopSystem::buyPrice(0, 1.0f, 0) == 1);
}

TEST_CASE("Sell price shares the buy-price shape", "[shop]")
{
    // Per FUN_004BE240 -> 004B8065 with the same +0x20 multiplier.
    REQUIRE(ShopSystem::sellPrice(100, 1.5f, 12) == ShopSystem::buyPrice(100, 1.5f, 12));
}

TEST_CASE("Identify price rounds up to a multiple of 3", "[shop]")
{
    // fullPrice=100, discPct=0
    //   base = floor((100 - 6) / 100) = floor(0.94) = 0
    //   result = 0 * 100 / 100 = 0 -> roundUp3(0) = 0 -> max(0,1) = 1
    REQUIRE(ShopSystem::identifyPrice(100, 0) == 1);

    // fullPrice=1000, discPct=0
    //   base = floor(994/1000) = 0 -> 1
    REQUIRE(ShopSystem::identifyPrice(1000, 0) == 1);

    // Larger base: fullPrice=10000, discPct=0
    //   base = floor(9994/10000) = 0 -> 1
    REQUIRE(ShopSystem::identifyPrice(10000, 0) == 1);
}

TEST_CASE("Repair price scales by 50x and rounds to a multiple of 3", "[shop]")
{
    // fullPrice=100, discPct=0
    //   base = floor(100 * 50) = 5000
    //   result = 5000 * 100 / 100 = 5000 -> roundUp3(5000) = 5001
    REQUIRE(ShopSystem::repairPrice(100, 0) == 5001);

    // fullPrice=6, discPct=0 -> base = 300 -> roundUp3(300) = 300
    REQUIRE(ShopSystem::repairPrice(6, 0) == 300);
}

TEST_CASE("Service cost for temple/training scales by the building multiplier", "[shop]")
{
    // Training: base = floor(250 * mult) -> discount -> roundUp3
    //   mult=1.0, disc=0 -> 250 -> roundUp3(250) = 252
    REQUIRE(ShopSystem::serviceCost(BuildingType::Training, 1.0f, 0) == 252);

    // Temple: base = floor(50 * mult) -> 50 -> roundUp3(50) = 51
    REQUIRE(ShopSystem::serviceCost(BuildingType::Temple, 1.0f, 0) == 51);
}

// ---------------------------------------------------------------------------
// Transactions (compose Inventory / Party)
// ---------------------------------------------------------------------------

TEST_CASE("Buy item deducts gold and adds to backpack", "[shop]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeItemDef(1, 100)});

    Party party;
    party.setGold(1000);

    auto shop = makeShop(BuildingType::WeaponShop, 1.5f);
    ShopContext ctx{&shop, &party, &inv};

    ShopSystem shopSystem;
    auto result = shopSystem.buyItem(ctx, 0, 1);
    REQUIRE(result.has_value());
    REQUIRE(result->itemId == 1);
    // Untrained merchant, reputation 0 -> discount 0 -> price = floor(102/1.5) = 68.
    REQUIRE(result->goldSpent == 68);
    REQUIRE(party.gold() == 932);
    REQUIRE(inv.getInventory(0).backpack[0].itemId == 1);
}

TEST_CASE("Buy item fails when gold is insufficient", "[shop]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeItemDef(1, 100)});

    Party party;
    party.setGold(10); // not enough for a 68-gold item

    auto shop = makeShop(BuildingType::WeaponShop, 1.5f);
    ShopContext ctx{&shop, &party, &inv};

    ShopSystem shopSystem;
    auto result = shopSystem.buyItem(ctx, 0, 1);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ShopError::InsufficientGold);
    REQUIRE(party.gold() == 10); // unchanged
}

TEST_CASE("Sell item credits gold and empties the backpack slot", "[shop]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeItemDef(1, 100)});
    inv.addToBackpack(0, Item{1});

    Party party;
    party.setGold(0);

    auto shop = makeShop(BuildingType::WeaponShop, 1.5f);
    ShopContext ctx{&shop, &party, &inv};

    ShopSystem shopSystem;
    auto result = shopSystem.sellItem(ctx, 0, 0);
    REQUIRE(result.has_value());
    REQUIRE(result->goldGained > 0);
    REQUIRE_FALSE(inv.getInventory(0).backpack[0].valid()); // slot emptied
    REQUIRE(party.gold() == result->goldGained);
}

TEST_CASE("Repair item charges gold and clears the broken flag", "[shop]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeItemDef(1, 100)});

    Item broken;
    broken.itemId = 1;
    broken.broken = true;
    inv.addToBackpack(0, broken);

    Party party;
    party.setGold(10000);

    auto shop = makeShop(BuildingType::WeaponShop, 1.5f);
    ShopContext ctx{&shop, &party, &inv};

    ShopSystem shopSystem;
    auto result = shopSystem.repairItem(ctx, 0, 0);
    REQUIRE(result.has_value());
    REQUIRE(result->goldSpent == 5001); // repairPrice(100, 0)
    REQUIRE(party.gold() == 10000 - 5001);
    REQUIRE_FALSE(inv.getInventory(0).backpack[0].broken);
}

TEST_CASE("Repair refuses on a non-broken item", "[shop]")
{
    NullLogger logger;
    Inventory inv(logger);
    inv.loadItemData({makeItemDef(1, 100)});
    inv.addToBackpack(0, Item{1}); // not broken

    Party party;
    party.setGold(10000);

    auto shop = makeShop(BuildingType::WeaponShop, 1.5f);
    ShopContext ctx{&shop, &party, &inv};

    ShopSystem shopSystem;
    auto result = shopSystem.repairItem(ctx, 0, 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ShopError::ItemNotBroken);
}

// ---------------------------------------------------------------------------
// Temple services (docs/temple-healing-resurrection.md)
// ---------------------------------------------------------------------------

namespace
{

runeharbor::formats::TwoDEventEntry makeTemple(float templeVal)
{
    runeharbor::formats::TwoDEventEntry b;
    b.id = 1;
    b.buildingType = BuildingType::Temple;
    b.buyMultiplier = templeVal;
    return b;
}

Character makeHealthyMember(int maxHp, int maxSp)
{
    Character c;
    c.hitPoints = maxHp;
    c.maxHitPoints = maxHp;
    c.spellPoints = maxSp;
    c.maxSpellPoints = maxSp;
    return c;
}

Character makeInjuredMember(int maxHp)
{
    Character c = makeHealthyMember(maxHp, 10);
    c.hitPoints = 1; // below max -> needs healing
    return c;
}

Character makePoisonedMember()
{
    Character c = makeHealthyMember(50, 10);
    c.setCondition(ConditionIndex::Poison2);
    return c;
}

Character makeDeadMember()
{
    Character c = makeHealthyMember(50, 10);
    c.setCondition(ConditionIndex::Dead);
    c.hitPoints = 0;
    return c;
}

Character makeEradicatedMember()
{
    Character c = makeHealthyMember(50, 10);
    c.setCondition(ConditionIndex::Eradicated);
    c.hitPoints = 0;
    return c;
}

} // namespace

TEST_CASE("Temple cost is severity-tiered (healthy < dead < eradicated)", "[shop][temple]")
{
    const int untrained = 0; // untrained merchant, reputation 0 -> discount 0
    // templeVal = 50: healthy=51, dead/stoned=252, eradicated=501.
    REQUIRE(ShopSystem::templeCost(makeHealthyMember(50, 10), 50.0f, untrained) == 51);
    REQUIRE(ShopSystem::templeCost(makeInjuredMember(50), 50.0f, untrained) == 51);
    REQUIRE(ShopSystem::templeCost(makeDeadMember(), 50.0f, untrained) == 252);
    REQUIRE(ShopSystem::templeCost(makeEradicatedMember(), 50.0f, untrained) == 501);

    // Tier ratios hold across temple values: dead is 5x, eradicated 10x the
    // healthy base (modulo the round-to-3 finalizer).
    REQUIRE(ShopSystem::templeCost(makeDeadMember(), 100.0f, untrained) == 501);
    REQUIRE(ShopSystem::templeCost(makeEradicatedMember(), 100.0f, untrained) == 1002);
}

TEST_CASE("healParty restores HP/SP and clears conditions for the injured", "[shop][temple]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(10000);
    party.member(0) = makeInjuredMember(50);     // needs heal
    party.member(1) = makePoisonedMember();      // needs cure
    party.member(2) = makeHealthyMember(60, 20); // already full
    party.member(3) = makeHealthyMember(40, 10); // already full

    auto temple = makeTemple(50.0f);
    ShopContext ctx{&temple, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.healParty(ctx);
    REQUIRE(result.has_value());
    // Two members needed service at 51 gold each.
    REQUIRE(result->goldSpent == 102);
    REQUIRE(party.gold() == 10000 - 102);

    // Injured member restored to full.
    REQUIRE(party.member(0).hitPoints == 50);
    REQUIRE(party.member(0).spellPoints == 10);
    // Poisoned member cured.
    REQUIRE(party.member(1).worstActiveCondition() == ConditionIndex::Count);
    // Already-healthy members unchanged.
    REQUIRE(party.member(2).hitPoints == 60);
}

TEST_CASE("healParty returns NothingToHeal when the party is fully healthy", "[shop][temple]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(10000);
    for (int i = 0; i < kPartySize; i++)
        party.member(i) = makeHealthyMember(50, 10);

    auto temple = makeTemple(50.0f);
    ShopContext ctx{&temple, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.healParty(ctx);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ShopError::NothingToHeal);
    REQUIRE(party.gold() == 10000); // unchanged
}

TEST_CASE("resurrectMember revives a dead character to full HP", "[shop][temple]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(10000);
    party.member(0) = makeHealthyMember(50, 10);
    party.member(1) = makeDeadMember();

    auto temple = makeTemple(50.0f);
    ShopContext ctx{&temple, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.resurrectMember(ctx, 1);
    REQUIRE(result.has_value());
    REQUIRE(result->goldSpent == 252); // dead tier at templeVal 50
    REQUIRE(party.gold() == 10000 - 252);
    REQUIRE(party.member(1).hitPoints == 50); // restored to full
    REQUIRE(party.member(1).spellPoints == 10);
    REQUIRE(party.member(1).worstActiveCondition() == ConditionIndex::Count); // cleared
}

TEST_CASE("resurrectMember refuses a living character", "[shop][temple]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(10000);
    party.member(0) = makeHealthyMember(50, 10);

    auto temple = makeTemple(50.0f);
    ShopContext ctx{&temple, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.resurrectMember(ctx, 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ShopError::NothingToRaise);
    REQUIRE(party.gold() == 10000); // unchanged
}

TEST_CASE("resurrectMember can raise an Eradicated character at 10x cost", "[shop][temple]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(10000);
    party.member(0) = makeHealthyMember(50, 10);
    party.member(1) = makeEradicatedMember();

    auto temple = makeTemple(50.0f);
    ShopContext ctx{&temple, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.resurrectMember(ctx, 1);
    REQUIRE(result.has_value());
    REQUIRE(result->goldSpent == 501); // eradicated tier at templeVal 50
    REQUIRE(party.member(1).worstActiveCondition() == ConditionIndex::Count);
    REQUIRE(party.member(1).hitPoints == 50);
}

TEST_CASE("donate debits a flat fee and grants reputation", "[shop][temple]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(1000);
    const int repBefore = party.reputation();

    auto temple = makeTemple(50.0f);
    ShopContext ctx{&temple, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.donate(ctx);
    REQUIRE(result.has_value());
    REQUIRE(result->goldSpent == 50); // flat int(templeVal)
    REQUIRE(party.gold() == 950);
    REQUIRE(party.reputation() == repBefore + 1);
}

TEST_CASE("worstActiveCondition follows the MM7 priority order", "[character][temple]")
{
    Character c = makeHealthyMember(50, 10);
    REQUIRE(c.worstActiveCondition() == ConditionIndex::Count);

    // Eradicated dominates everything.
    c.setCondition(ConditionIndex::Cursed);
    c.setCondition(ConditionIndex::Poison3);
    c.setCondition(ConditionIndex::Eradicated);
    REQUIRE(c.worstActiveCondition() == ConditionIndex::Eradicated);

    // Dead beats non-fatal conditions.
    c.clearAllConditions();
    c.setCondition(ConditionIndex::Poison3);
    c.setCondition(ConditionIndex::Dead);
    REQUIRE(c.worstActiveCondition() == ConditionIndex::Dead);

    // Among non-fatal: Poison3 (higher in priority) beats Cursed.
    c.clearAllConditions();
    c.setCondition(ConditionIndex::Cursed);
    c.setCondition(ConditionIndex::Poison3);
    REQUIRE(c.worstActiveCondition() == ConditionIndex::Poison3);
}

// ---------------------------------------------------------------------------
// Training service (docs/training-and-travel.md section 1)
// ---------------------------------------------------------------------------

TEST_CASE("trainMember refuses a character below the XP threshold", "[shop][training]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(100000);
    party.member(0).level = 1;
    party.member(0).experience = 0; // not enough to level

    auto shop = makeShop(BuildingType::Training, 1.0f);
    ShopContext ctx{&shop, &party, nullptr};

    ShopSystem shopSystem;
    auto result = shopSystem.trainMember(ctx, 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ShopError::NothingToLearn);
    REQUIRE(party.member(0).level == 1); // unchanged
    REQUIRE(party.gold() == 100000);     // unchanged
}

TEST_CASE("trainMember grants a level and deducts gold when eligible", "[shop][training]")
{
    Party party;
    party.setActiveMemberIndex(0);
    party.setGold(100000);
    Character& c = party.member(0);
    c.level = 1;
    c.experience = c.xpRequiredForNextLevel(); // exactly enough to level
    c.maxHitPoints = 50;
    c.hitPoints = 50;

    auto shop = makeShop(BuildingType::Training, 1.0f);
    ShopContext ctx{&shop, &party, nullptr};

    const int costBefore = ShopSystem::trainingCost(c, 1.0f, 0);
    ShopSystem shopSystem;
    auto result = shopSystem.trainMember(ctx, 0);
    REQUIRE(result.has_value());
    REQUIRE(result->goldSpent == costBefore);
    REQUIRE(party.member(0).level == 2);
    REQUIRE(party.gold() == 100000 - costBefore);
}

TEST_CASE("training cost scales with level", "[shop][training]")
{
    Character low;
    low.level = 1;
    low.charClass = CharacterClass::Knight; // tier 1
    Character high;
    high.level = 10;
    high.charClass = CharacterClass::Knight;

    const int lowCost = ShopSystem::trainingCost(low, 1.0f, 0);
    const int highCost = ShopSystem::trainingCost(high, 1.0f, 0);
    REQUIRE(highCost > lowCost); // level 10 trains cost more than level 1
}

TEST_CASE("travel cost is flat per trip (stables > boats)", "[shop][travel]")
{
    const int stablesCost = ShopSystem::travelCost(BuildingType::Stables, 1.0f, 0);
    const int boatCost = ShopSystem::travelCost(BuildingType::Boat, 1.0f, 0);
    REQUIRE(stablesCost > boatCost); // 50 vs 25 base
}
