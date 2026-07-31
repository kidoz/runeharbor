// SPDX-License-Identifier: MIT
//
// MM7 building/shop type codes. The numeric values mirror the discriminator
// stored at struct offset +0x00 of the in-memory 2dEvents array (base
// 0x005912B8 in MM7-Rel.exe), decoded from the `2dEvents.txt` "Type" column
// via the loader's string->code comparisons. See docs/re/29-shops-and-economy.md
// section 2.2 for the recovered token->code mapping.
#pragma once

#include <cstdint>
#include <string_view>

namespace runeharbor::game
{

enum class BuildingType : int16_t
{
    None = 0, // sentinel (entry 0 in the 2dEvents array is never populated)

    // Item shops (the core buy/sell/identify/repair loop)
    WeaponShop = 1,
    ArmorShop = 2,
    MagicShop = 3,
    Alchemist = 4,

    // Magic guilds (learn spells)
    FireGuild = 5,
    AirGuild = 6,
    WaterGuild = 7,
    EarthGuild = 8,
    SpiritGuild = 9,
    MindGuild = 0x0A,
    BodyGuild = 0x0B,
    LightGuild = 0x0C,
    DarkGuild = 0x0D,

    // Misc / civic
    TownHall = 0x11,
    GeneralMerchant = 0x12,
    Tavern = 0x15,
    Bank = 0x16,
    Temple = 0x17,
    Stables = 0x1B,
    Boat = 0x1C,
    Travel = 0x1E,
    Training = 0x1F, // "Training Grounds" — used by the 89-row table
};

// Converts the `2dEvents.txt` "Type" text to a typed code. Returns None for
// unrecognized labels. (Mirrors the loader's token comparisons.)
BuildingType buildingTypeFromName(std::string_view typeText);

// Whether entering this building opens a 2D shop/service UI at all, mirroring
// the per-type validity table at 0x4F04BC consulted by FUN_004B8DA0.
bool hasShopUI(BuildingType type);

// Broad UI family used by the shop window to pick a layout. Coincides with the
// renderer dispatch: item shops share one layout; temples, training, and travel
// each have their own.
enum class ShopFamily
{
    None,         // not a shop UI
    ItemShop,     // Weapon/Armor/Magic/Alchemist — buy/sell/identify/repair
    Temple,       // heal / cure / resurrect
    Training,     // spend gold for experience
    Travel,       // stables / boats -> map transitions
};

ShopFamily shopFamily(BuildingType type);

} // namespace runeharbor::game
