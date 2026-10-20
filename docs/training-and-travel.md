---
title: "Training Grounds & Travel (Stables/Boats)"
summary: "Training grounds, stables, and boats share building-service dispatch but perform distinct actions."
doc_type: reference
status: partial
last_updated: 2026-10-20
---
# Training Grounds & Travel (Stables/Boats)

Training grounds, stables, and boats share building-service dispatch but perform distinct actions.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Extends [shops-and-economy.md](shops-and-economy.md) and
[temple-healing-resurrection.md](temple-healing-resurrection.md) with the
training and travel service loops.

---

## 0. Critical correction to doc 29

Doc 29 §2.2 labeled token `tra`/0x1E as "Travel". The disassembly proves
**`tra` = Training Ground (level-up)** (the abbreviation), and **0x1F is the
Skill Trainer** (raise a skill for gold). There is **no separate Travel
building type** — stables (0x1B) and boats (0x1C) ARE the travel service.

| Type code | Token | Service | Function |
|-----------|-------|---------|----------|
| `0x1E` (30) | `tra` | **Training Ground (level-up)** | `0x4B4673` |
| `0x1F` (31) | (skill) | Skill Trainer | `0x4B4EB2` (UI) / `0x4B63DB` (execute) |
| `0x1B` (27) | `sta` | Stables (travel) | `0x4B68A6` |
| `0x1C` (28) | `boa` | Boats (travel) | `0x4B68A6` |
| `0x17` (23) | `tem` | Temple | `0x4B6FC1` (doc 30) |

RuneHarbor originally had `Travel = 0x1E` / `Training = 0x1F` — swapped
relative to the binary. `building_type.hpp` now uses `Training = 0x1E` and
treats stables/boats as travel.

---

## 1. Training Ground (level-up) — `FUN_004B4673` (2111 B)

### 1.1 What training does

Training is the **level-up button**: the character must ALREADY have enough XP,
and training spends gold and grants exactly one level. It does NOT grant XP.

- **XP gate** (0x4B4AFF): `if (char.experience < requiredXpForNextLevel) abort`.
- **Gold gate + debit** (0x4B4B1F): `if (partyGold < cost) abort; debit`.
- **Grant the level** (0x4B4B43): `char.level += 1`.
- **Side effects** (0x4B4B4A): `skillPoints = (newLevel / 10) + 5`;
  `char.skillPointsToSpend += skillPoints`; heal HP/SP to new max.

### 1.2 Required XP

`requiredXp = triangle(level) * 1000` where `triangle(n) = sum(1..n)` —
identical to RuneHarbor's `xpRequiredForNextLevel()`.

### 1.3 Cost — scales with level AND class tier

```text
classTier = ((char.classByte % 4) + 1), clamped to {1,2,3}   // base / promotion1 / promotion2
base      = round_to_nearest( char.level * building.shopMult * classTier )
final     = max( base * (100 - merchantDiscountPct) / 100, base / 3 )

```

So higher level = more expensive, and promoted classes pay 2×/3×. The flat
"250 × shopMult" from doc 29 §3.6 belongs to the **skill trainer**
(`FUN_004B63DB`), not the level trainer.

### 1.4 Level cap (tiered trainers) — AMBIGUOUS

A per-building word table at `0x4F06E6` (indexed by building index) holds a
max-teachable level, compared at 0x4B4AAA. The values read (0xA0–0xA3) are
implausible as MM7 caps (~5/15/25); likely the single Training building is
effectively uncapped and tiered trainers live in the skill-trainer path. Flag
for confirmation; RuneHarbor ships uncapped with a TODO.

---

## 2. Skill Trainer (for contrast) — `FUN_004B63DB`, type 0x1F

This is what doc 29 §3.6 actually disassembled. Cost:
`(type==0x12 ? 100 : 250) × shopMult`, then the standard finalizer. Executes
by gating on class byte, marking a skill "trained" (`word[edi]=1`), debiting
gold. The same idea is implemented in `ShopSystem::learnSkill`; modern
OpenEnroth sources price non-guild teaching at `500 × flt_24` (guilds at
`500 × fPriceMultiplier`), which is the formula RuneHarbor uses.

---

## 3. Stables / Boats Travel — `FUN_004B68A6` (1718 B)

### 3.1 Cost — flat per trip

```text
base  = (type == 0x1B ? 50 : 25) * building.shopMult    // Stables=50, Boats=25
final = max( round(base) * (100 - disc) / 100, round(base) / 3 )

```

Flat per trip (every destination from a given building costs the same); the
per-building `shopMult` (Val field) is the only variable.

### 3.2 Destination data — hardcoded tables (NOT trans.txt)

**`trans.txt` is NOT a transportation table** — it's a "Transition Description"
flavor-text table (`2D# \t description \t area name`), shown when approaching
2D locations. The real destination data is hardcoded in the executable:

- **Destination records** at `0x4F0830`, stride 0x20 (32 bytes):
  - `+0x00` u8 destination map id (index into the map-name table `0x5CAA3C`)
  - `+0x08` u32 travel time in days
  - `+0x0C/+0x10/+0x14` i32 arrival X/Y/Z (world coords)
  - `+0x18` i32 arrival facing (yaw 0–2047)
- **Per-building destination menus**: byte tables at `0x4F0BB8` (render) and
  `0x4F0B4F` (execute), indexed `[slot + buildingIdx*4]`.

So RuneHarbor must hand-author a destination table (no external data file).

### 3.3 The map transition — direct map load, NOT EVT ChangeMap

```text
saveCurrentMapState();
CurrentMapName = dest map filename;
mapNeedsFreshLoad = true;
pendingArrival = (dest.arrivalX, Y, Z, facing);   // applied post-load
gameplaySubState = "Entering";

```

This drives the normal map-load pipeline directly (does not invoke the EVT
ChangeMap opcode). The pending arrival globals write to PartyPos after load.

### 3.4 Travel-time advance

```text
travelDays = dest.days
if (quest shortcuts unlocked) travelDays -= 2/1
minutes = travelDays * 1440
AdvanceGameClock(minutes)        // adds to game-time counter, rolls days
ApplyOvernightEffects()          // per-character hunger/spell-expiry

```

The game clock jumps by `days × 1440` minutes.

---

## Integration notes

Implemented in RuneHarbor:

- `BuildingType::Training = 0x1E`; stables (0x1B) and boats (0x1C) are the
  travel service. `2dEvents.txt` label `tra` maps to Training.
- `Character::canLevelUp()` / `levelUp()` implement §1.1: `levelUp()` awards
  `(newLevel / 10) + 5` skill points and heals HP/SP to the new maximum.
- `ShopSystem::trainingCost()` / `trainMember()` implement §1.3 (level × class
  tier × building multiplier, then the merchant finalizer) and the XP/gold
  gates.
- `ShopSystem::travelCost()` / `chargeTravel()`, the hand-authored destination
  table, and the `onTravelRequest_` map-transition hook implement §3.
- `ShopWindow` exposes Level Up and Skills pages (Tab switches), with Travel
  and the other service families alongside them.

Skill progression:

- Unspent points live in `Character::skillPoints`. Raising a learned skill one
  level costs `current level + 1` points (the original's `UIMSG_SkillUp` path)
  and caps at level 60. The character sheet's Skills page spends points.
- `ShopSystem::learnSkill()` teaches any skill whose class cap is not
  `SkillMastery::None`, for `500 × multiplier` with the merchant finalizer
  (`UIHouses::learnSelectedSkill`). Training Grounds offer the class's
  starting and creation-pick skills; guilds additionally offer their own magic
  school, which is how Light/Dark become learnable after a path promotion.
- `ShopSystem::trainMastery()` handles Expert/Master/Grandmaster with the flat
  cost tables and the gates from the original: skill level 4/7/10, previous
  tier, class cap (`skillMaxMasteryPerClass`), Personality/Endurance/Intellect
  50 for Merchant/Bodybuilding/Learning, the armed/unarmed level-10 pair, and
  the Light/Dark tier-3 class requirement.
- `Character::promoteCharacter()` implements the base → tier-1 → light/dark
  tier-2 class transitions; promotion quests and NPC wiring are still pending.
- Skill points and mastery are persisted in save version 21.

Remaining gaps:

- Mastery trainers in the original are NPC dialogue options (topics
  200..311); RuneHarbor currently hosts mastery training on the Training
  Grounds' Skills page until NPC trainer data is wired. Guilds offer their own
  school skill and spell levels.
- The trainer level-cap table at `0x4F06E6` remains uncapped (§1.4).
- Promotion quests, Light/Dark path selection, and class-change NPCs are not
  yet implemented; `promoteCharacter()` is the engine primitive.
- `GameWorld::calendar()` is the single gameplay clock. Camp and inn rest call
  `GameWorld::advanceTime(8 * 60)`, which expires timed buffs and advances
  day/night/events. Level-up training does not yet consume in-game time (the
  original rests the party until the next dawn plus several hours).

### Function/global additions for `docs/data/`

Functions: `0x4B4673` (TrainingGroundService), `0x4B63DB` (SkillTrainer),
`0x4B68A6` (TravelService), `0x4B1B3E` (AdvanceGameClock), `0x45F4A2`
(SaveCurrentMapState), `0x4CAC80` (SetCurrentMapName), `0x476399`
(IsQuestBitSet).

Globals: `0x4F0830` (destination records, stride 0x20), `0x4F0B4F`/`0x4F0BB8`
(per-building destination menus), `0x4F06E6` (trainer level-cap table),
`0x5B6428..34` (pending arrival position), `0x6BE1C4` (current map name),
`0x6BE1E4` (map-needs-load flag).

---

## Open questions

Track these unresolved items in the [open-question register](open-questions.md).

- **Trainer level-cap table** `0x4F06E6` values (0xA0–0xA3) — likely the single
  Training building is uncapped; tiered trainers may live in the skill-trainer
  path. RuneHarbor ships uncapped with a TODO.
- **Map-id → filename mapping** (`0x5CAA3C`) is zeroed in the static image
  (populated at runtime by the MAPSTATS loader). RuneHarbor resolves map names
  via its own MAPSTATS parse, so hand-authored destinations use map filenames
  directly.
- **Full destination enumeration**: the hardcoded tables list ~20 buildings ×
  ~4 destinations each. RuneHarbor ships a small seed set and can be expanded.
