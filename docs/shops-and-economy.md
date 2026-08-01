---
title: "Shops, Buildings, and Economy"
summary: "Buildings dispatch shop and service behavior through shared registry and pricing data."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Shops, Buildings, and Economy

Buildings dispatch shop and service behavior through shared registry and pricing data.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

This page documents the previously unresolved
NPC, shop, and service behavior.

---

## 1. Data tables involved

All three were previously listed as "schema unreversed." They are **plain
tab-delimited text** packed inside `Events.lod` under a nested-LOD wrapper
(276-byte build header + 16-byte name + sizes + zlib stream). Confirmed by
extracting and decompressing:

| File | Purpose | Format |
|------|---------|--------|
| `2dEvents.txt` | Building/shop registry (89 entries) | Tab-delimited text, 24+ columns |
| `MERCHANT.TXT` | Shopkeeper dialogue text (Buy/Sell/Repair/Identify × skill tier) | Tab-delimited text |
| `TRANS.TXT` | Map-transition area descriptions (not shop transactions) | Tab-delimited text |

Wrapper layout (in-LOD record):

```cpp
0x000..0x113  276-byte high-entropy build/checksum header (skip)
0x114         char name[16]
0x12c         u32 compressed payload size
0x140         u32 uncompressed-size hint (sometimes off; ignore)
0x144         u32 flags
0x148..EOF    zlib stream   (note: 2dEvents.txt has a larger multi-record
                             header; its zlib starts at 0x1a08)

```

The RuneHarbor `LODArchive`/`GameLODArchive` already strips the outer LOD; the
shop-table loader must additionally skip this inner wrapper and zlib-inflate.

---

## 2. `2dEvents.txt` — the building/shop registry

### 2.1 Text column layout (authoritative, from the file's own header row)

```cpp
#  Type  Map  Picture  Name  ProprieterName  Title  Picture  State  Rep  Per  Val  A  B  C  Notes  Notes2  Open  Closed  Pic  Map  Restrictions  Text  ...

```

Mapping to the in-memory struct (array base `0x005912B8`, stride 0x34 = 52 bytes,
526-entry capacity, entry 0 = sentinel):

| Text col | Struct offset | Type | Meaning |
|----------|---------------|------|---------|
| `#` (col1) | — | i32 | Line no (1-based); col2 `#` is the per-type index |
| `Type` | +0x00 | i16 | **Building/shop type** (string→code, see §2.2) |
| `Map` | +0x02 | i16 | Map id the building belongs to |
| `Picture` (col5) | +0x2C | i16 | Shopkeeper portrait/sprite id |
| `Name` | +0x04 | ptr→string | Shop title shown in the UI |
| `Proprieter Name` | +0x08 | ptr→string | NPC name |
| `Title` | +0x10 | ptr→string | Proprietor title ("Blacksmith", "Healer"…) |
| `Val` (col13) | +0x20 | **float** | **Buy price multiplier / service base cost** |
| `A` (col14) | +0x24 | float | Secondary service markup (identify/etc.) |
| `B` (col16) | +0x1C | i16 | Service-cost seed (inn/training) |
| `C` (col19, "Open") | +0x28 | i16 | Opening hour (e.g. 6) |
| `Closed` (col20) | +0x2A | i16 | Closing hour (e.g. 18) |
| `Restrictions`/`Text` | +0x30 | i16 | Visited/availability bitmask index |

Fields +0x14/+0x16/+0x18/+0x1A/+0x2E remain ambiguous (parsed as ints, no
unambiguous reader found). Confirm later via Ghidra if needed.

## 2.2 Building/shop type codes (field +0x00)

From the loader's string→code comparisons (`0x4E7B44`–`0x4E7BA8`):

| Code | Token | Category | Code | Token | Category |
|------|-------|----------|------|-------|----------|
| 1 | `wea` | Weapon Shop | 0x12 (18) | `mer` | General Merchant |
| 2 | `arm` | Armor Shop | 0x15 (21) | `tav` | Tavern / Inn |
| 3 | `mag` | Magic Shop | 0x16 (22) | `ban` | Bank |
| 4 | `alc` | Alchemist | 0x17 (23) | `tem` | Temple |
| 5–11 | fire/air/water/earth/spi/min/bod | Magic guilds | 0x1B (27) | `sta` | Stables |
| 0x0C/0x0D | `lig`/`dar` | Light/Dark guild | 0x1C (28) | `boa` | Boat |
| 0x11 | `tow` | Town Hall / Tower | 0x1E (30) | `tra` | Travel |
| 0x0E/0x0F/0x10 | `ele`/`sel`/`mir` | (elemental misc) | | | |

A per-type "has shop UI" byte table at `0x4F04BC` is consulted at `0x4B8DB8`
(`cmp byte [typeCode + 0x4F04BC], 0`) to gate whether entering opens a screen
at all.

The extracted `2dEvents.txt` populates 89 buildings across 8 types:
Weapon Shop (14), Armor Shop (14), Magic Shop (13), Alchemist (12), Temple (15),
Stables (9), Boats (11), Training (1). Guilds/Bank/Tavern/Travel are not in
this file (they live in other event mechanisms).

### 2.3 Observed data ranges (from the real table)

- `Val` (col13): shops use 1.5 / 2.0 / 2.5 / 3.0 / 4.0 / 5.0; **Temple** uses
  fixed service costs (50, 100, 30, 40, 1.5, 5, 10…); Training uses larger
  fixed costs.
- `A` (col14): 1.0 / 1.5 / 2.0 (secondary multiplier / membership tier).
- `Open`/`Closed`: typically 6 / 18 (6am–6pm); evil-aligned shops flip to 18 / 6.

---

## 3. Shop pricing formulas

All verified by disassembly. Float constants live at `0x4D8470..0x4D89B0`.

### 3.1 Item full price — `FUN_0045646E` (`Item_GetFullPrice`, 87 B)

Already documented in [item-system.md](item-system.md) §10.3. Returns the item's gold value
before shop modifiers: `base = itemDesc.value`; +`stdEnchantPower*100` if
standard enchant; `× multiplier` (or `+multiplier`) if special enchant.

### 3.2 Merchant skill discount — `FUN_004911EB` (103 B)

Returns an integer **percentage** discount consumed by every price finalizer.

```cpp
skillPts = SkillPoints(activeChar, MERCHANT)   // masked 0..63
modifier = charData[+0x134] & 0x3F             // per-char modifier
mastery  = GetMastery(MERCHANT)                // 1=Norm 2=Expert 3=Master 4=GM
if mastery == GM: return 10000                 // 100% (free)
rep = ReputationBonus()                        // party faction sum
if skillPts == 0: return -rep                  // untrained -> penalty
masteryBase = (GM?5 : Master?3 : Expert?2 : 1)
return (masteryBase - 1) * modifier + skillPts + 7 - rep

```

### 3.3 Buy price — `FUN_004B8065` (64 B)

Args: `fullPrice` (int), `shopMult` (float = building +0x20).

```text
discPct   = Merchant_GetDiscountPct()         // 0x4911EB
floorTerm = floor((fullPrice + 2.0) / shopMult)
base      = (fullPrice * discPct) / 100 + floorTerm
result    = min(base, fullPrice)              // never exceed full price
result    = max(result, 1)                    // min 1 gold

```

### 3.4 Identify price — `FUN_004B8126` (80 B)

```text
base   = floor((fullPrice - 6.0) / fullPrice)
result = base * (100 - discPct) / 100
round up to next multiple of 3; min 1

```

### 3.5 Repair price — `FUN_004B80DC` (74 B, takes float, `ret 4`)

```text
base   = floor(fullPrice * 50.0)
result = base * (100 - discPct) / 100
round up to next multiple of 3; min 1

```

### 3.6 Type-gated fixed service costs

`FUN_004B63DB` (1138 B) and `FUN_004B68A6` (1718 B), called from the shop
dispatcher, compute Temple/Training/Inn/Stables service prices that depend on
the **building type (+0x00)** rather than an item:

- `0x4B63DB`: `base = (type == 0x12 ? 100 : 250) × shopMult(+0x20)` → discount → round up to mult of 3.
- `0x4B68A6`: `base = (type == 0x1B ? 50 : 25) × shopMult(+0x20)` → discount → round up to mult of 3.

### 3.7 Sell path

`FUN_004BE240` (sell-one-item, 103 B) loads the same `+0x20` multiplier and
**calls the buy finalizer `0x4B8065`** — so sell credit reuses the buy formula,
parameterized by the per-building `+0x20` float. There is **no separate
sell-fraction constant**; the buy/sell distinction is encoded in the `+0x20`
value authored per building. (Field +0x24 appears in some service paths combined
with `+500.0` but is not unambiguously a sell multiplier — treat as unresolved.)

---

## 4. Shop UI flow (Window Type 10)

### 4.1 Entry — opcode 0x16 `EVT_SHOW_BUILDING`

Executor `FUN_004304D6` handles opcode 0x16 by:

1. Storing the building id into `0x005C3444` (`TwoD_EventParam`).
2. Setting `0x005C3450` = active building index (from `FUN_0044608D`).
3. Opening the Shop/NPC window (type 10).

`FUN_0044608D` (BuildingEnterInit, 431 B) reads, for the active building
(`idx × 0x34`): `+0x00` (group → NPC dialog set lookup), `+0x2C` (→ active shop
sub-index), `+0x30` (visited-bit test against `0xACD59D`), then collects the
building's NPCs from the per-map NPC list (`0x72D560`, stride 0x4C).

### 4.2 Shop screen renderer — `FUN_004B30BA` (1397 B)

Called from UI driver `0x4158D2`. Reads `0x5912B0` (active record ptr),
`0x591270`/`0x5912A4` (dialog counts), `0x5C3450` (active building), and
dispatches sub-actions. Calls `0x4B29D7` (NPC dialog/portrait) and the
type-gated service-price functions.

### 4.3 Layout selection — `FUN_004B8DA0` (inside `0x4B81E8`)

```cpp
typeCode = int16[buildingIdx*0x34 + 0x5912B8]   // field +0x00
if byte[typeCode + 0x4F04BC] == 0: no shop UI    // per-type "has shop" table
// else cascade on buildingIdx to pick a layout/coordinate table:
buildingIdx <= 0x0E -> table @ 0x4F027E
buildingIdx <= 0x1C -> table @ 0x4F0318 / 0x4F01EE
buildingIdx <= 0x29 -> table @ 0x4F03F6
buildingIdx <= 0x35 -> table @ 0xAD45B4

```

So **whether a shop UI opens is gated by the type code**; **which screen layout
renders is selected by the building index** cascading through fixed ranges.

### 4.4 Action dispatcher — `FUN_0044686D` (7134 B, 60-case switch)

Entered with a sub-opcode in `edx`; jumps through a 60-case table at
`0x44844B` (`jmp [edx*4 + 0x44844B]`). Observed case labels: 8, 9, 12, 14, 16,
17, 18, 22, 25, 26, 32, 34, 35, 36, 39, 40, 42, 43, 47, 48, 49, 50, 51, 54,
55, 56, 59, 60 — each an individual shop/NPC interaction (buy, sell, identify,
repair, hire, learn-spell, travel, rest, deposit, withdraw, …). Full per-case
semantics out of scope; the dispatch structure is the deliverable.

---

## Integration notes

Existing assets to build on (confirmed by audit):

- `EventOpcode::ShowBuilding` (22) → `onShowBuilding(int buildingId)` callback
  → currently a status-message stub at `application.cpp:2835`.
- `game::Inventory` has `getItemDef`, `giveItem`, `removeItem`, `takeFromBackpack`,
  `identifyItem`, `repairItem`, `totalValue` — the money/item backbone.
- `game::Party` has `spendGold`/`addGold`.
- `formats::TwoDEventsParser` already keeps every raw column in `columns`; only
  `displayName` is read today — the typed fields (§2.1) must be decoded from it.
- `ui::DialogueWindow` + `Panel`/`ListBox`/`Button`/`ScrollBar` for the screen.

RE-derived constants to encode:

- `Merchant_GetDiscountPct` formula (§3.2).
- Buy/Identify/Repair formulas (§3.3–3.5) with constants 2.0, 6.0, 50.0, round-to-3.
- Type-gated service costs (§3.6) with the 100/250 and 50/25 splits.

Function index to add to [function-index.md](data/function-index.md):
`0x4911EB` (Merchant_GetDiscountPct), `0x4B8065` (BuyPrice), `0x4B8126`
(IdentifyPrice), `0x4B80DC` (RepairPrice), `0x4B30BA` (ShopScreen),
`0x4B8DA0` (ShopLayoutSelect), `0x44686D` (ShopActionDispatch, 60 cases),
`0x44608D` (BuildingEnterInit).

Global to add to [globals-reference.md](data/globals-reference.md): `0x005912B8` (BuildingShop array,
stride 0x34, 526 entries, sentinel at 0). Correct `0x5C344C` description to
"raw text buffer pointer (parse input)".
