// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

// Shopkeeper flavor text, indexed by the player's Merchant-skill scenario and
// the service kind. RE-derived from `MERCHANT.TXT` (5 rows x 4 service columns).
// This is *dialogue text only* — actual pricing is computed by
// game::ShopSystem from the formulas in docs/re/29-shops-and-economy.md.
//
// The four service columns (matching the table header) are Buy / Sell / Repair
// / Identify; %06/%24/%25/%27/%29 are MM7 text-substitution tokens resolved by
// the dialogue layer (player name / item name / list price / final price /
// identify cost).

enum class MerchantServiceColumn : uint8_t
{
    Buy = 0,
    Sell = 1,
    Repair = 2,
    Identify = 3,
    Count = 4
};

// The merchant-skill scenario selects which line of flavor text is shown. The
// numeric order follows the rows in `MERCHANT.TXT`.
enum class MerchantScenario : uint8_t
{
    NotEnoughGold = 0, // "I'm sorry, %06, but you don't have enough money."
    NoSkill = 1,       // untrained merchant (no discount)
    RegularSkill = 2,  // trained, normal/expert
    GoodSkill = 3,     // master / highly skilled
};

struct MerchantTextRow
{
    std::array<std::string, static_cast<size_t>(MerchantServiceColumn::Count)> services;
};

class MerchantTextParser
{
  public:
    explicit MerchantTextParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);

    // Returns the flavor text for a scenario/service, or empty if unavailable.
    std::string_view text(MerchantScenario scenario, MerchantServiceColumn service) const;

    bool loaded() const { return !rows_.empty(); }

  private:
    util::ILogger& logger_;
    std::vector<MerchantTextRow> rows_;
};

} // namespace runeharbor::formats
