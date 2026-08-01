// SPDX-License-Identifier: MIT
#include "building_type.hpp"

#include <algorithm>
#include <string>

#include <cctype>

namespace runeharbor::game
{

namespace
{

std::string toLowerAscii(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

} // namespace

BuildingType buildingTypeFromName(std::string_view typeText)
{
    // The `2dEvents.txt` "Type" column uses full labels ("Weapon Shop",
    // "Armor Shop", ...). The engine's loader reduces these to 3-letter tokens
    // (wea/arm/mag/alc/tem/sta/boa/...). Accept either form so the parser is
    // robust to the actual asset text.
    const std::string t = toLowerAscii(typeText);

    if (t == "weapon shop" || t == "wea")
        return BuildingType::WeaponShop;
    if (t == "armor shop" || t == "arm")
        return BuildingType::ArmorShop;
    if (t == "magic shop" || t == "mag")
        return BuildingType::MagicShop;
    if (t == "alchemist" || t == "alc")
        return BuildingType::Alchemist;
    if (t == "temple" || t == "tem")
        return BuildingType::Temple;
    if (t == "stables" || t == "sta")
        return BuildingType::Stables;
    if (t == "boats" || t == "boat" || t == "boa")
        return BuildingType::Boat;
    if (t == "training" || t == "training grounds" || t == "tra")
        return BuildingType::Training;
    if (t == "tavern" || t == "tav")
        return BuildingType::Tavern;
    if (t == "bank" || t == "ban")
        return BuildingType::Bank;
    if (t == "town hall" || t == "tow")
        return BuildingType::TownHall;
    if (t == "fire guild" || t == "fir")
        return BuildingType::FireGuild;
    if (t == "air guild" || t == "air")
        return BuildingType::AirGuild;
    if (t == "water guild" || t == "wat")
        return BuildingType::WaterGuild;
    if (t == "earth guild" || t == "ear")
        return BuildingType::EarthGuild;
    if (t == "spirit guild" || t == "spi")
        return BuildingType::SpiritGuild;
    if (t == "mind guild" || t == "min")
        return BuildingType::MindGuild;
    if (t == "body guild" || t == "bod")
        return BuildingType::BodyGuild;
    if (t == "light guild" || t == "lig")
        return BuildingType::LightGuild;
    if (t == "dark guild" || t == "dar")
        return BuildingType::DarkGuild;
    if (t == "general merchant" || t == "mer")
        return BuildingType::GeneralMerchant;

    return BuildingType::None;
}

bool hasShopUI(BuildingType type)
{
    return shopFamily(type) != ShopFamily::None;
}

ShopFamily shopFamily(BuildingType type)
{
    switch (type)
    {
    case BuildingType::WeaponShop:
    case BuildingType::ArmorShop:
    case BuildingType::MagicShop:
    case BuildingType::Alchemist:
    case BuildingType::GeneralMerchant:
        return ShopFamily::ItemShop;
    case BuildingType::Temple:
        return ShopFamily::Temple;
    case BuildingType::Training:
        return ShopFamily::Training;
    case BuildingType::Stables:
    case BuildingType::Boat:
        return ShopFamily::Travel;
    // Guilds/Tavern/Bank/TownHall are out of scope for the first playable
    // economy pass (see docs/re/29-shops-and-economy.md, "Out of scope").
    case BuildingType::None:
    case BuildingType::FireGuild:
    case BuildingType::AirGuild:
    case BuildingType::WaterGuild:
    case BuildingType::EarthGuild:
    case BuildingType::SpiritGuild:
    case BuildingType::MindGuild:
    case BuildingType::BodyGuild:
    case BuildingType::LightGuild:
    case BuildingType::DarkGuild:
    case BuildingType::TownHall:
    case BuildingType::Tavern:
    case BuildingType::Bank:
        return ShopFamily::None;
    }
    return ShopFamily::None;
}

} // namespace runeharbor::game
