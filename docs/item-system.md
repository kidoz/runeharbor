---
title: "Item System"
summary: "The item system defines item records, equipment, enchantments, generation, and merchant behavior."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Item System

The item system defines item records, equipment, enchantments, generation, and merchant behavior.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Item Instance Struct](#2-item-instance-struct)
3. [Item Description Table](#3-item-description-table)
4. [Data Files](#4-data-files)
5. [Equipment Slots](#5-equipment-slots)
6. [Enchantment System](#6-enchantment-system)
7. [Item Generation](#7-item-generation)
8. [Potions and Consumables](#8-potions-and-consumables)
9. [Quest Items](#9-quest-items)
10. [Merchant System](#10-merchant-system)
11. [Artifact System](#11-artifact-system)
12. [Key Functions](#12-key-functions)
13. [Key Global Variables](#13-key-global-variables)

---

## 1. Overview

The item system in MM7 manages all objects that can appear in inventories, on the ground,
in chests, and in shops. Items are broadly categorized into:

- **Weapons** -- melee and ranged, various skill types
- **Armor** -- body armor, shields, helmets, boots, gauntlets, cloaks, belts
- **Accessories** -- rings, amulets
- **Consumables** -- potions, reagents, bottles
- **Scrolls** -- spell scrolls (single-use and multi-use), skill scrolls
- **Quest items** -- special purpose items with unique handling
- **Artifacts and relics** -- unique named items with fixed special enchantments
- **Gold/treasure** -- gold drops generated on monster death

The system has two distinct data layers:

1. **Item Description Table** -- static definitions loaded from `items.txt`, one entry per
   item type (up to 800 entries), defining base stats like name, value, damage, equip type.
2. **Item Instance** -- a 36-byte runtime struct representing a specific item in the world
   or in an inventory, containing the item type ID plus per-instance state such as
   enchantments, charges, and flags.

---

## 2. Item Instance Struct

**Size: 0x24 (36 bytes)**

Each item instance in the game world (inventory slots, ground objects, chest contents,
monster drops) uses this layout. Accessed at `character_base + 0x1F0 + slot * 0x24`.

```text
Offset  Size  Type     Field
------  ----  -------  -----
0x00    4     int32    itemId          -- Index into item description table (1-based; 0 = empty)
0x04    4     int32    stdEnchantId    -- Standard enchantment ID (from stditems.txt; 0 = none)
0x08    4     int32    stdEnchantPower -- Standard enchantment bonus value (e.g., +5 Might)
0x0C    4     int32    spcEnchantId    -- Special enchantment ID (from spcitems.txt; 0 = none)
0x10    4     int32    numCharges      -- Number of remaining charges (wands, some artifacts)
0x14    4     uint32   flags           -- Bitfield (see below)
0x18    4     int32    bodyPartIndex   -- Equipment body slot index when equipped (see section 5)
0x1C    4     int32    maxCharges      -- Maximum charges (for recharge calculation)
0x20    4     int32    ownerPlayer     -- Owning player index, or timestamp/extra data

```

### Item Flags (offset 0x14)

| Bit | Hex  | Name          | Description |
|-----|------|---------------|-------------|
| 0   | 0x01 | `IDENTIFIED`  | Item has been identified (shows full name/stats) |
| 1   | 0x02 | `BROKEN`      | Item is broken (needs repair) |
| 2   | 0x04 | `CURSED`      | Item is cursed (cannot be unequipped normally) |
| 3   | 0x08 | `TEMP_BONUS`  | Item has a temporary enchantment bonus |
| 4   | 0x10 | `AURA`        | Item has a visible aura effect |
| 5   | 0x20 | `STOLEN`      | Item was stolen (shops may refuse to buy) |
| 6   | 0x40 | `HARDENED`    | Item has been hardened (extra durability) |
| 7   | 0x80 | `QUEST_BIT`   | Used for quest item tracking |

### Empty vs. Valid Items

An item slot is empty when `itemId == 0`. The item generation function `FUN_0045664c`
clears a 0x24-byte block to zero via `memset(dst, 0, 0x24)` before populating it.

---

## 3. Item Description Table

**Entry size: 0x30 (48 bytes)**
**Base address: `DAT_005d2864`**
**Maximum entries: 800 (indices 0-799)**

Loaded from `items.txt` in `FUN_00456dbe`. The table uses a stride of 0x30 bytes per
entry and stores static properties for each item type. Fields are parsed in 17 columns
(case 0 through case 0x10 in the loader switch).

```text
Offset  Size  Type    Field
------  ----  ------  -----
0x00    4     ptr     iconName        -- Pointer to icon texture name string
0x04    4     ptr     name            -- Pointer to item name string
0x08    4     ptr     unidentName     -- Pointer to unidentified item name string
0x0C    4     ptr     description     -- Pointer to item description text
0x10    4     int32   value           -- Base gold value (at DAT_005d2874)
0x14    2     int16   spriteId        -- Sprite ID for ground/world display (at DAT_005d2878)
0x18    2     int16   flags2          -- Additional type flags
0x1C    1     byte    enchantChance   -- Random enchant chance (at DAT_005d2880 == "equip category")
0x1D    1     byte    weaponSkill     -- Weapon skill type (sword, axe, etc.)
0x1E    1     byte    damageDice      -- Number of damage dice
0x1F    1     byte    damageSides     -- Sides per damage die
0x20    1     byte    equipType       -- Equipment category (see below)
0x21    1     byte    materialType    -- Material/skill type for weapons (see section 3.2)
0x22    1     byte    damageNumDice   -- Alt damage representation: number of dice
0x23    1     byte    damageSidesAlt  -- Alt damage representation: sides per die
0x24    1     byte    mod1            -- Modifier 1 (AC bonus for armor, etc.)
0x25    1     byte    itemClass       -- 0=normal, 1=artifact, 2=relic, 3=special
0x26    1     byte    spcEnchantRef   -- Special enchantment cross-reference
0x27    1     byte    stdEnchantRef   -- Standard enchantment cross-reference
0x28    4     int32   valueMultiplier -- Gold value modifier
0x2C    2     int16   idSkillReq      -- Skill required to identify
0x2D    1     byte    treasureLvl1    -- Random generation level (low)
0x2E    1     byte    treasureLvl2    -- Random generation level (mid)
0x2F    1     byte    treasureLvl3    -- Random generation level (high)
0x30    1     byte    treasureLvl4    -- Random generation level (max)
0x31    1     byte    treasureLvl5    -- Random generation probability weight
0x32    1     byte    treasureLvl6    -- Random generation probability weight

```

### 3.1 Equipment Type Enum (field at offset 0x20)

Parsed from the "equip type" column of `items.txt` in `FUN_00456dbe`:

| Value | Hex  | String       | Category |
|-------|------|-------------|----------|
| 0     | 0x00 | `weapon`     | One-handed weapon (also `weapon1or2`) |
| 1     | 0x01 | `weapon2`    | Two-handed weapon |
| 2     | 0x02 | `missile`    | Ranged weapon (bow, crossbow) |
| 3     | 0x03 | `armor`      | Body armor |
| 4     | 0x04 | `shield`     | Shield |
| 5     | 0x05 | (helm)       | Helmet |
| 6     | 0x06 | (belt)       | Belt |
| 7     | 0x07 | `cloak`      | Cloak |
| 8     | 0x08 | `gauntlets`  | Gauntlets |
| 9     | 0x09 | `boots`      | Boots |
| 10    | 0x0A | (ring)       | Ring |
| 11    | 0x0B | `amulet`     | Amulet |
| 12    | 0x0C | `weaponw`    | Wand (weapon that uses charges) |
| 13    | 0x0D | `reagent`    | Alchemy reagent (also `herb`) |
| 14    | 0x0E | `bottle`     | Potion bottle |
| 15    | 0x0F | `sscroll`    | Spell scroll (single use) |
| 16    | 0x10 | (book)       | Spell book |
| 17    | 0x11 | `mscroll`    | Message scroll (readable) |
| 18    | 0x12 | (deed)       | Deed / document |
| 19    | 0x13 | (goldItem)   | Gold pile item |
| 20    | 0x14 | (none)       | Non-equippable / quest item |

### 3.2 Weapon Skill Type Enum (field at offset 0x21)

Parsed from the "skill" column of `items.txt`:

| Value | Hex  | String     | Weapon Type |
|-------|------|-----------|-------------|
| 0     | 0x00 | `staff`    | Staff |
| 1     | 0x01 | `sword`    | Sword |
| 2     | 0x02 | `dagger`   | Dagger |
| 3     | 0x03 | `axe`      | Axe |
| 4     | 0x04 | `spear`    | Spear |
| 5     | 0x05 | `bow`      | Bow (missile) |
| 6     | 0x06 | `mace`     | Mace |
| 7     | 0x07 | `blaster`  | Blaster |
| 8     | 0x08 | `shield`   | Shield |
| 9     | 0x09 | `leather`  | Leather armor |
| 10    | 0x0A | `chain`    | Chain armor |
| 11    | 0x0B | `plate`    | Plate armor |
| 37    | 0x25 | (misc)     | Miscellaneous |
| 38    | 0x26 | (none)     | No skill type |

### 3.3 Item Class Enum (field at offset 0x25)

| Value | String      | Description |
|-------|-------------|-------------|
| 0     | (normal)    | Standard item, can be randomly generated |
| 1     | `artifact`  | Artifact -- unique, one per game, has fixed special enchantment |
| 2     | `relic`     | Relic -- powerful unique item |
| 3     | `special`   | Special item -- has a specific enchantment from stditems/spcitems |

---

## 4. Data Files

All item data is loaded from text files in the LOD archives. The master loading function
is `FUN_00456dbe` (4893 bytes), which calls sub-loaders in sequence.

### 4.1 items.txt

**Loader:** `FUN_00456dbe` (items.txt section)
**Entry count:** Up to 800 (check at `iVar6 < 800`, i.e., `799 < iVar6` triggers exit)
**Entry size:** 0x30 bytes in the runtime table
**Columns:** 17 tab-separated fields per row (indices 0-16)

| Column | Field | Parser Notes |
|--------|-------|--------------|
| 0 | Item number | Numeric index (1-based) |
| 1 | Icon name | String pointer stored at `param_1[iVar6 * 0xc + 1]` |
| 2 | Item name | String pointer at `param_1[iVar6 * 0xc + 2]` |
| 3 | Value | Numeric, stored at `param_1[iVar6 * 0xc + 5]` |
| 4 | Equip type | String-matched to category enum (see section 3.1) |
| 5 | Weapon skill | String-matched to skill enum (see section 3.2) |
| 6 | Damage dice | Format `NdM` parsed as N dice of M sides |
| 7 | Modifier (AC) | Numeric byte |
| 8 | Item class | `artifact` / `relic` / `special` / default (0) |
| 9 | Treasure level | Numeric byte |
| 10 | Unidentified name | String pointer at `param_1[iVar6 * 0xc + 3]` |
| 11 | Sprite ID | Numeric uint16 |
| 12 | Enchantment ref | Cross-ref to stditems/spcitems by name |
| 13 | Ench. strength | Numeric (for special items) |
| 14 | ID skill req | Numeric uint16 |
| 15 | Alt flags | Numeric uint16 |
| 16 | Description | String pointer at `param_1[iVar6 * 0xc + 4]` |

### 4.2 stditems.txt -- Standard Enchantment Table

**Loader:** Part of `FUN_00456dbe`
**Entry count:** 24 (0x18) entries
**Entry size:** 20 bytes (5 dwords) in runtime
**Columns:** 11 per row

Standard enchantments are simple stat bonuses like "+5 Might" or "+10 Armor Class".
Each entry has a name, a description, and probability weights for 9 treasure levels.

Runtime layout per entry (20 bytes / 5 dwords):

```text
Offset  Size  Field
0x00    4     descriptionStr  -- Enchantment description pointer
0x04    4     nameStr         -- Enchantment name pointer
0x08-   9x1   probWeight[0..8] -- Probability for each of 9 treasure levels

```

Probability sums are precomputed in `param_1[0x45b9]` (9 cumulative entries).

### 4.3 spcitems.txt -- Special Enchantment Table

**Loader:** Part of `FUN_00456dbe`
**Entry count:** 72 (0x48) entries
**Entry size:** 28 bytes (7 dwords) in runtime
**Columns:** 16 per row

Special enchantments are named effects like "of Fire", "of the Dragon", "Vampiric", etc.
They provide complex multi-stat bonuses, resistances, or special abilities.

Runtime layout per entry (28 bytes / 7 dwords):

```text
Offset  Size  Field
0x00    4     descriptionStr  -- Enchantment description pointer
0x04    4     nameStr         -- Enchantment name pointer
0x08-   12x1  probWeight[0..11] -- Probability for 12 item categories
0x14    4     valueMultiplier -- Gold value multiplier for this enchantment
0x18    1     iLevel          -- Item level character code ('a'-based)

```

Probability sums precomputed in `param_1[0x45ce]` (12 cumulative entries for categories).
Maximum special enchant index stored at `param_1[0x45e6] = 0x47` (71).

### 4.4 rnditems.txt -- Random Item Generation Table

**Loader:** Part of `FUN_00456dbe`
**Entry count:** Up to 619 (0x26B) entries
**Columns:** 8 per row

Controls per-item-type probability of appearing at each treasure level. Each entry maps
an item ID to probability weights across 6 treasure levels.

Fields parsed (columns 2-7 map to offsets 0x2D-0x32 in the item description):

```text
Column 0: Item number (index)
Column 2: param_1[iVar6 * 0xc + 0xb]  -- base random level byte
Column 3: offset 0x2D in item desc    -- treasure level 1 weight
Column 4: offset 0x2E                 -- treasure level 2 weight
Column 5: offset 0x2F                 -- treasure level 3 weight
Column 6: offset 0x30                 -- treasure level 4 weight
Column 7: offset 0x31                 -- treasure level 5 weight

```

Cumulative probability sums are computed across 800 items for 6 levels and stored at
`param_1[0x45a1]` (6 entries totaling 0x18 bytes).

### 4.5 Treasure Level Tables (in rnditems.txt, second section)

After the per-item rows, `rnditems.txt` contains a second section defining treasure level
range brackets. Three rows (levels 0-2) with columns for gold-min, gold-max, item-min,
item-max, bonus-min, bonus-max:

```text
param_1[0x45a7..0x45b8] -- 3 rows x 6 columns of treasure bracket data

```

### 4.6 potion.txt -- Potion Mixing Table

**Loader:** `FUN_00453b68` (425 bytes)
**Format:** 50x50 matrix (0x32 x 0x32) of int16 values
**Storage:** At address offset 0x77B2 from base, stride 0x32 entries per row

The potion mixing table is a 50x50 grid where `table[potionA][potionB]` gives the
resulting potion ID when mixing A with B. Value 0 means no valid combination. The prefix
`E` in a cell indicates an explosion result.

### 4.7 potnotes.txt -- Potion Autonote Table

**Loader:** `FUN_00453d11` (392 bytes)
**Format:** Same matrix structure as potion.txt
**Storage:** Follows potion table at offset 0x8176

Records which potion recipes the party has discovered (for the Autonotes journal).

### 4.8 merchant.txt

**Loader:** `FUN_00476594`

Defines merchant-specific data including buy/sell multipliers and available item stock
per shop type.

### 4.9 scroll.txt

**Loader:** `FUN_004764c6`

Defines spell scroll properties and which spell each scroll item ID casts.

---

## 5. Equipment Slots

### 5.1 Character Inventory Layout

Each character has a 0x1B3C-byte record. Item-related offsets within this record:

| Offset | Size | Field |
|--------|------|-------|
| +0x01F0 | 0x24 * 138 | `inventory[138]` -- Item instance array (138 slots of 36 bytes each) |
| +0x17A0 | 0x10 * 24 | `equipped[24]` -- Equipment slot descriptors (24 slots of 16 bytes) |
| +0x194C | 4 | `currentSlotIndex` -- Currently selected inventory slot |
| +0x09CC | varies | `spellbook` -- Known spells/conditions data |

The inventory array holds up to 138 (0x8A) item instances. Each is a 36-byte item struct.
The total inventory region is `138 * 0x24 = 0x1368` bytes.

### 5.2 Inventory Grid

The inventory is displayed as a 14-column x 9-row grid (14 * 9 = 126 visible cells).
The function `FUN_004927a0` searches for an empty slot by iterating:

- Outer loop: column 0..13 (step 1)
- Inner loop: row 0..8, each row offset = column + row * 14 (0x0E)

This gives slot indices 0-125 for the visible grid. Slots 126-137 are used for equipped
items referenced by the equipment descriptors.

### 5.3 Equipment Slot Descriptors

The `equipped[]` array at character offset +0x17A0 has 24 entries of 16 bytes each.
Each equipped slot descriptor tracks:

```text
Offset  Size  Field
0x00    4     itemId       -- Item type ID (0 = nothing equipped)
0x04    4     inventorySlot-- Index into the inventory[] array
0x08    2     unknown1
0x0A    2     unknown2
0x0C    1     slotFlags
0x0D    1     slotFlags2
0x0E    1     reserved
0x0F    1     reserved

```

The function `FUN_0048c695` initializes all 24 equipment slots to zero (0x18 iterations
of clearing 16-byte blocks).

### 5.4 Equipment Slot Indices

Based on the equip type parsing and known MM7 behavior, the equipment body slots are:

| Slot Index | Body Part | Accepts Equipment Types |
|------------|-----------|------------------------|
| 0 | Main Hand (Right) | weapon, weapon2, wand |
| 1 | Off Hand (Left) | weapon, shield |
| 2 | Bow/Missile | missile |
| 3 | Body Armor | armor |
| 4 | Helmet | helm |
| 5 | Belt | belt |
| 6 | Cloak | cloak |
| 7 | Gauntlets | gauntlets |
| 8 | Boots | boots |
| 9 | Amulet | amulet |
| 10 | Ring Left | ring |
| 11 | Ring Right | ring |
| 12-23 | Reserved/Extra | (used for special or hireling equipment) |

Two-handed weapons (equipType 1) occupy both main hand and off hand, preventing shield
use. The equip logic checks the `equipType` field of the item description to determine
which slot(s) are valid.

---

## 6. Enchantment System

Items can have two types of enchantments, stored in the item instance struct:

### 6.1 Standard Enchantments (stditems.txt)

Standard enchantments provide simple numeric bonuses. Stored in:

- `item.stdEnchantId` (offset +0x04) -- ID into the 24-entry standard enchant table
- `item.stdEnchantPower` (offset +0x08) -- Bonus magnitude

Examples of standard enchantments:

- "+N to Might" (stat bonus)
- "+N to Armor Class"
- "+N to Fire Resistance"

The enchantment name string is looked up from `DAT_005dbe50 + enchantId * 0x14`.

### 6.2 Special Enchantments (spcitems.txt)

Special enchantments provide named effects with complex behaviors. Stored in:

- `item.spcEnchantId` (offset +0x0C) -- ID into the 72-entry special enchant table

Examples include effects like "of Fire" (fire damage), "Vampiric" (life steal),
"Swift" (speed bonus), etc.

The enchantment name is looked up from `DAT_005dc028 + enchantId * 0x1C`.

Some special enchant IDs have hardcoded behaviors in combat code (e.g., specific IDs
checked in `FUN_00439463` for on-hit effects like stun, knockback, or status infliction).

### 6.3 Enchantment Application

An item can have either a standard enchantment OR a special enchantment, but not both.
The display logic at `FUN_004564df` checks:

- If `stdEnchantId != 0`: shows "ItemName of EnchantName"
- If `spcEnchantId != 0`: shows "EnchantName ItemName" or "ItemName of EnchantName"

Certain special enchant IDs use a prefix format (e.g., "Vampiric Sword") while others
use a suffix format (e.g., "Sword of Fire").

### 6.4 Enchantment Probabilities

When randomly enchanting an item, the system uses probability weight tables:

- **Standard enchantments:** 9 probability weights per enchantment, indexed by treasure level
- **Special enchantments:** 12 probability weights per enchantment, indexed by item category

The cumulative sums precomputed during loading allow efficient weighted random selection.

---

## 7. Item Generation

### 7.1 Random Item Generation Function

**Function:** `FUN_0045664c` at `0045664c` (1689 bytes)
**Signature:** `uint ItemGen_Generate(int treasureLevel, byte* itemCategory, int* destItem)`
**Allocates:** 0x24 bytes if `destItem` is NULL (string: `"newItemGen"`)

The function generates a random item based on a treasure level (1-6) and optional item
category filter. The algorithm:

1. **Clear** the destination item struct (memset 0x24 bytes to zero)
2. If `itemCategory` is NULL (any category):
   a. Sum all artifact availability flags in `DAT_00acd5f2[0..0x1C]` (29 artifacts)
   b. If treasure level 6 and 5% chance and fewer than 13 artifacts found:
      - Mark artifact as found, set `itemId = (rand() % 29) + 500`
      - Call `FUN_00456d51` (artifact init) and return
   c. Otherwise, roll against the cumulative probability table for the treasure level:
      `rand() % cumulativeSum[treasureLevel]`
   d. Walk the item table to find which item ID the random roll selects
3. If `itemCategory` is specified:
   - Use category-specific subtables and probability weights

### 7.2 Treasure Level System

Items are generated at treasure levels 1-6 (passed as `param_2 - 1` internally, so
indices 0-5). Higher treasure levels produce better items with higher enchantment chances.

The generation function then decides enchantment:

- **Roll < 20 (0x14):** No enchantment (plain item), calls `FUN_00402f07`
- **Roll 20-59 (0x14-0x3B):** Gold bonus enchantment with level-scaled value:

| Treasure Level | Gold Item ID | Gold Range |
|----------------|-------------|------------|
| 1 | 0xC5 (197) | 50 + rand() % 51 |
| 2 | 0xC5 (197) | 100 + rand() % 101 |
| 3 | 0xC6 (198) | 200 + rand() % 301 |
| 4 | 0xC6 (198) | 500 + rand() % 501 |
| 5 | 0xC7 (199) | 1000 + rand() % 1001 |
| 6 | 0xC7 (199) | 2000 + rand() % 3001 |

- **Roll >= 60 (0x3C):** Calls `FUN_0045664c` recursively to generate an enchanted item

### 7.3 Potion Generation

When the generated item has `equipType == 0x0E` (bottle) and is not item 0xDC (220):

- The `numCharges` field is populated with a random potion power:
  `charges = (rand() % 4 + 1) + (rand() % 4 + 1) + (rand() % 4 + 1)` (3d4 roll)

### 7.4 Monster Loot Generation

The function `FUN_00450244` at `00450244` (777 bytes) generates loot for all monsters
on the current map:

1. Roll `rand() % 100` for base loot quality
2. Look up the treasure level from a map-specific table at `DAT_005caa67`
3. For each monster's item slots (up to 0x8C = 140 items per actor, stride 0x24):
   - If the item ID is negative (flag for "generate random"):
     - Determine treasure level from the monster's data
     - Apply the same 20/60 threshold logic as item generation
     - Fill in generated item or gold

Actor stride is 0x14CC (5324 bytes), iterating from `DAT_005e4fd0`.

---

## 8. Potions and Consumables

### 8.1 Potion Mixing

Potion mixing is governed by the 50x50 table loaded from `potion.txt`. Each cell is an
int16 where:

- **Positive value:** Result potion item ID
- **Zero:** Invalid combination (no result)
- **Negative or 'E' prefix:** Explosion (damages the mixer)

The table is stored at a fixed memory offset (base + 0x77B2), with each row containing
50 int16 entries (100 bytes per row, 50 rows = 5000 bytes total).

### 8.2 Reagents

Reagent items (equipType 0x0D) are raw ingredients used in alchemy. They are combined
with empty bottles (equipType 0x0E) using the Alchemy skill to create potions.

### 8.3 Scroll Usage

Spell scrolls (equipType 0x0F, `sscroll`) contain a single spell that can be cast once.
The scroll's spell ID is determined by the item ID mapping in `scroll.txt`.

Message scrolls (equipType 0x11, `mscroll`) are readable items that display text when used.

### 8.4 Consumable Effects

The item loading function at `FUN_0045504a` parses various consumable effect strings at
offset 0x13 in the item data (88-byte entries, stride 0x58). Known effect keywords:

| Offset 0x13 Value | Keyword | Effect |
|-------------------|---------|--------|
| 1 | `curse` | Applies curse condition |
| 2 | (weak) | Applies weakness |
| 3 | `asleep` | Applies sleep condition |
| 4 | `afraid` | Applies fear condition |
| 5 | `drunk` | Applies drunk condition |
| 6 | `insane` | Applies insanity condition |
| 7 | `poison1` | Applies poison level 1 |
| 8 | `poison2` | Applies poison level 2 |
| 9 | `poison3` | Applies poison level 3 |
| 10 | `disease1` | Applies disease level 1 |
| 11 | `disease2` | Applies disease level 2 |
| 12 | `disease3` | Applies disease level 3 |
| 13 | `paralyze` | Applies paralysis |
| 14 | `uncon` | Applies unconsciousness |
| 15 | (dead) | Applies death |
| 16 | `stone` | Applies petrification |
| 17 | `errad` | Applies eradication |
| 18 | `brkitem` | Breaks a random item |
| 19 | `brkarmor` | Breaks equipped armor |
| 20 | `brkweapon` | Breaks equipped weapon |
| 21 | `steal` | Steals an item |
| 22 | (age) | Applies aging |
| 23 | `drainsp` | Drains spell points |

---

## 9. Quest Items

### 9.1 Identification

Quest items are identified by:

- `itemClass == 3` (special) in the item description table
- Having specific enchantment cross-references to stditems or spcitems
- Item IDs in the range 500-528 (artifact range, `iVar11 % 0x1d + 500`)

### 9.2 Special Handling

Quest items have special behavior:

- Cannot be sold to merchants
- Cannot be dropped on the ground in some cases
- May trigger event system opcodes when used or obtained
- Event opcode 0x09 (`EVT_GIVE_ITEM`) creates and gives a specific item to a player
- Event opcode 0x11 (`EVT_REMOVE_ITEM`) removes a specific item
- Event opcode 0x22 (`EVT_SPAWN_ITEM`) spawns an item on the ground at coordinates

### 9.3 The `addItem` Function

**Function:** `FUN_0048c6dc` at `0048c6dc` (351 bytes)
**Source:** `D:\mm7Src_eng\MM7\Code\Party.cpp` (error string references this)

Adds an item to the party, trying each character in priority order:

1. If the item has no icon texture (`DAT_005d2864 + itemId * 0x30` is null), assert error
2. Build a character priority order: active character first, then cycle through 0-3
3. For each character, call `FUN_004927a0` to find an empty inventory slot
4. If slot found, copy 36 bytes (9 dwords) from source to destination slot
5. Play pickup sound (sound ID 200) and character voice (sound ID 0x3C = 60)
6. Return 1 on success, 0 if no character has room

The copy loop confirms the item struct is exactly 9 dwords = 36 bytes:

```c
for (i = 9; i != 0; i--) {
    *dest++ = *src++;
}

```

---

## 10. Merchant System

### 10.1 Shop Types

Shops are defined in `2dEvents.txt` (loaded by `FUN_00443824`) and categorized by type.
Known shop types from string references:

| Type | Name | Sells |
|------|------|-------|
| Weapon Smith | `Human Weapon Smith01`, `Elf Weapon Smith`, etc. | Weapons |
| Armor Shop | `Human Armor01`, `Elf Armor`, etc. | Armor and shields |
| Magic Shop | `Human Magic Shop01`, `Elf Magic Shop`, etc. | Wands, scrolls, potions |
| Alchemist | `Necromancer Alchemist01`, `Elf Alchemist`, etc. | Potions, reagents, bottles |
| General Store | (various) | Miscellaneous items |

Each shop type has race/faction variants (Human, Elf, Warlock, Wizard, Dwarven,
Necromancer) that affect available inventory.

### 10.2 Item Identification

Items start unidentified unless the `IDENTIFIED` flag (bit 0) is set. The identification
service reveals the item's true name and enchantments. The `idSkillReq` field in the
item description determines the merchant skill needed.

Unidentified items display the `unidentName` string instead of the full `name`.

### 10.3 Item Pricing

**Function:** `FUN_0045646e` at `0045646e` (87 bytes)

Calculates the full price of an item:

1. Base price = `DAT_005d2874 + itemId * 0x30` (value field from item description)
2. If `TEMP_BONUS` flag (bit 3) is set, return base price only
3. Check `FUN_00456d98` (is artifact?) -- if so, return base price
4. If standard enchant (`stdEnchantId != 0`):
   - `fullPrice = basePrice + stdEnchantPower * 100`
5. If special enchant (`spcEnchantId != 0`):
   - Look up multiplier from `DAT_005dc03c + spcEnchantId * 0x1C`
   - If multiplier < 11: `fullPrice = multiplier * basePrice`
   - Otherwise: `fullPrice = basePrice + multiplier`

### 10.4 Item Repair

Broken items (flag bit 1) cannot be used and must be repaired by a merchant or through
a spell. The broken status is tracked with sprite variants: `brkweapon`, `brkarmor`,
`brkitem` (strings at `004e88c8-004e88e0`).

### 10.5 Buy/Sell Calculation

The merchant system uses `merchant.txt` data to apply buy/sell price multipliers per
shop. The event system opcode 0x16 (`EVT_SHOW_SHOP`) opens the shop UI.

---

## 11. Artifact System

### 11.1 Artifact Tracking

The game tracks which artifacts have been found using a byte array at `DAT_00acd5f2`
with 29 (0x1D) entries. Each byte is 0 (not found) or 1 (already found in this game).

Maximum simultaneous artifacts: 13 (if `sum(DAT_00acd5f2[0..28]) < 13`).

### 11.2 Artifact Generation

Artifacts are generated only at treasure level 6 (the highest) with a 5% chance:

```asm
if (treasureLevel == 6 && rand() % 100 < 5 && !alreadyFound[roll] && totalFound < 13):
    itemId = (rand() % 29) + 500
    mark as found
    call FUN_00456d51 for special initialization

```

Item IDs 500-528 are reserved for artifacts. The `FUN_00456d51` function initializes
artifact-specific properties (special enchantments, charges, etc.).

### 11.3 Artifact Properties

Artifacts have `itemClass == 1` in the item description table. They:

- Have fixed special enchantments defined in `items.txt` (column 12, cross-ref to spcitems)
- Cannot receive random enchantments
- Have unique item names and descriptions
- May have special combat effects checked by ID in damage functions
- Use the `IDENTIFIED` flag (set automatically on some artifacts)

### 11.4 Relic Items

Relics (`itemClass == 2`) are similar to artifacts but use a separate tracking mechanism.
They also have fixed enchantments and unique names. The distinction affects:

- Generation probability tables
- Price calculation
- Display formatting

---

## 12. Key Functions

| Address | Size | Suggested Name | Description |
|---------|------|---------------|-------------|
| `0045504a` | 4904 | `ItemData_LoadItemDescTable` | Parses item description entries from `items.txt` (0x58-byte internal format) |
| `00456dbe` | 4893 | `ItemData_LoadAll` | Master loader: calls stditems, spcitems, items, rnditems loaders |
| `0045664c` | 1689 | `ItemGen_Generate` | Random item generation with treasure level and category |
| `00456d51` | -- | `ItemGen_InitArtifact` | Initializes artifact-specific properties |
| `00456d98` | -- | `ItemGen_IsArtifact` | Checks if an item is an artifact or relic |
| `0045646e` | 87 | `Item_GetFullPrice` | Calculates item value including enchantments |
| `004564c5` | 26 | `Item_GetDisplayName` | Returns display name (unidentified or full) |
| `004564df` | 365 | `Item_GetFormattedName` | Builds formatted name with enchantment prefix/suffix |
| `004505f8` | 111 | `Item_FindFreeArtifactSlot` | Finds available artifact slot from 500-528 range |
| `00450244` | 777 | `MonsterLoot_Generate` | Generates loot for all map monsters |
| `0045054d` | 171 | `Item_SpawnOnGround` | Creates item as a sprite object in the world |
| `0048c6dc` | 351 | `Party_AddItem` | Adds item to party, trying each character |
| `0048c695` | 71 | `Player_ClearInventory` | Clears all 138 inventory slots and 24 equipment slots |
| `004927a0` | 126 | `Inventory_FindEmptySlot` | Searches 14x9 grid for empty slot that fits item |
| `00492520` | -- | `Inventory_CheckSlotFits` | Checks if item fits at specific grid position |
| `004925f8` | -- | `Inventory_PlaceItem` | Places item at grid position |
| `00492894` | 239 | `Inventory_PlaceItemWithCopy` | Places item and copies full 36-byte struct |
| `0049281e` | 118 | `Inventory_AddItemCopy` | Adds item to inventory with artifact init |
| `00453b68` | 425 | `Potion_LoadMixingTable` | Loads 50x50 potion mixing table from `potion.txt` |
| `00453d11` | 392 | `Potion_LoadAutonotes` | Loads potion autonote table from `potnotes.txt` |
| `0045490e` | 978 | `ItemData_ParseMonsterSpell` | Parses monster spell strings (enchantment effects) |

---

## 13. Key Global Variables

| Address | Type | Description |
|---------|------|-------------|
| `DAT_005d2864` | base | Item description table (stride 0x30, 800 entries) |
| `DAT_005d2868` | ptr[] | Item name string pointers (within desc table) |
| `DAT_005d2874` | i32[] | Item base value (within desc table) |
| `DAT_005d2878` | i16[] | Item sprite ID (within desc table) |
| `DAT_005d2880` | u8[] | Item equip type (within desc table) |
| `DAT_005d2881` | u8[] | Item weapon skill (within desc table) |
| `DAT_005d2892` | u8[] | Item "can enchant" flag (within desc table) |
| `DAT_005dbe50` | base | Standard enchantment table (stride 0x14, 24 entries) |
| `DAT_005dc028` | base | Special enchantment table (stride 0x1C, 72 entries) |
| `DAT_005dc03c` | i32[] | Special enchant value multiplier (within spc table) |
| `DAT_005e4aec` | ptr | Items.txt raw file buffer |
| `DAT_005e4b00` | ptr | Potion.txt raw file buffer |
| `DAT_005e4b04` | ptr | Potnotes.txt raw file buffer |
| `DAT_00acd5f2` | byte[29] | Artifact found flags (1 = found in current game) |
| `DAT_00ad458c` | i32 | Currently selected/hovered item ID (cursor item) |
| `DAT_005e4fd0` | base | Actor/monster array (stride 0x14CC) |
| `DAT_005e4fd4` | i32[] | Actor item inventory (within actor array) |
| `DAT_005caa67` | u8[] | Map treasure level table |
| `DAT_005c84f8` | char[] | Formatted item name buffer (used by GetFormattedName) |

---

## Notes

- All addresses reference MM7-Rel.exe v1.21 (PE32 x86).
- Item IDs are 1-based in the description table; ID 0 means "empty slot."
- The item instance struct (0x24 bytes) is the most frequently copied data structure in
  the game, appearing in inventory management, chest code, event handlers, and save/load.
- The distinction between stditems (standard) and spcitems (special) enchantments is
  fundamental: standard enchants use a power value, special enchants use a lookup ID.
- The 138-slot inventory per character (with a 14x9 visible grid) implies that items
  occupy variable grid cells based on their icon dimensions (multi-cell items).
- Artifact IDs 500-528 (29 items) are hardcoded as the artifact range. The game enforces
  a maximum of 13 artifacts in play at any time to maintain game balance.
