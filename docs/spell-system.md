---
title: "Spell System"
summary: "The spell system combines spell data, mastery, targeting, cost, duration, and effect dispatch."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Spell System

The spell system combines spell data, mastery, targeting, cost, duration, and effect dispatch.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Source file reference: `D:\mm7Src_eng\MM7\Code\seffects.cpp` (spell effects rendering and
animation), plus spell casting logic distributed across `Damage.cpp`, `Events.cpp`, and the
main spell casting handler.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Spell Data Table (spells.txt)](#2-spell-data-table-spellstxt)
3. [Spell Identification and Numbering](#3-spell-identification-and-numbering)
4. [Spell Schools](#4-spell-schools)
5. [Mastery Level System](#5-mastery-level-system)
6. [Casting Mechanics](#6-casting-mechanics)
7. [Spell Effect Application](#7-spell-effect-application)
8. [Spell Damage Calculation](#8-spell-damage-calculation)
9. [Spell Resistance](#9-spell-resistance)
10. [Duration Calculation](#10-duration-calculation)
11. [Area of Effect](#11-area-of-effect)
12. [Spell Visual Effects](#12-spell-visual-effects)
13. [Key Functions](#13-key-functions)
14. [Key Data Structures](#14-key-data-structures)
15. [Key Global Variables](#15-key-global-variables)

---

## 1. Overview

The spell system implements approximately 100 spells across 9 magic schools, with 4 mastery
levels affecting power, cost, and duration. Each school contains up to 11 spells, numbered
sequentially within the school. Spells are defined in a data-driven text file (`spells.txt`)
loaded from the LOD archive and parsed into a runtime table at startup.

Key characteristics:

- **9 spell schools**: Fire, Air, Water, Earth, Spirit, Mind, Body, Light, Dark
- **4 mastery levels**: Novice, Expert, Master, Grandmaster
- **~99 spells** total (11 per school), plus special/monster-only spells
- **Data-driven**: spell properties loaded from `spells.txt`
- **Skill-based**: casting cost, duration, and power scale with skill level and mastery
- **Spell IDs**: 1-based sequential numbering across all schools

---

## 2. Spell Data Table (spells.txt)

### Loading

The spell data table is loaded by `FUN_00453876` (718 bytes) from `spells.txt` in the game's
LOD archive. The file is tab-separated with one spell per row.

The parser reads fields separated by tab characters (`\t`). Each row produces a spell record
of **0x24 bytes** (36 bytes) stride. The base spell data array starts with an offset of
`+0x40` from the SpellManager base, with an additional flags byte array at a separate
location (stride 0x14 = 20 bytes between flag entries).

### Field Layout (per spell row)

The parser processes up to 11 fields per row (field indices 0 through 10):

| Field Index | Offset from Record Base | Size | Description |
|-------------|------------------------|------|-------------|
| 0 | -- | -- | Spell name (skipped during parsing) |
| 1 | -- | -- | Spell description (skipped during parsing) |
| 2 | -0x1C | 4 | Normal description / alternate text (string pointer) |
| 3 | +0x00 | 1 | **Spell school** (enumerated, see below) |
| 4 | -0x18 | 4 | Expert-level description (string pointer) |
| 5 | -0x14 | 4 | Master-level description (string pointer) |
| 6 | -0x10 | 4 | Grandmaster-level description (string pointer) |
| 7 | -0x0C | 4 | Mana cost / base parameter A |
| 8 | -0x08 | 4 | Casting delay / base parameter B |
| 9 | -0x04 | 4 | Recovery time / base parameter C |
| 10 | (flags) | 1 | **Target flags** (bitmask, see below) |

### Spell School Values (Field 3)

The school field is parsed by comparing the string against known school names (case-sensitive
string comparison via `FUN_004caaf0`):

| Value | School | String Key |
|-------|--------|------------|
| 0 | Fire | `"fire"` (at `DAT_004e845c`) |
| 1 | Air | (at `PTR_DAT_004e7b70`) |
| 2 | Water | `"water"` (at `s_water_004e8454`) |
| 3 | Earth | `"earth"` (at `s_earth_004e844c`) |
| 4 | (default) | Fallback if no match |
| 5 | Magic | `"magic"` (at `s_magic_004e841c`) |
| 6 | Spirit | `"spirit"` (at `s_spirit_004e8444`) |
| 7 | Mind | (at `DAT_004e843c`) |
| 8 | Body | (at `DAT_004e8434`) |
| 9 | Light | `"light"` (at `s_light_004e842c`) |
| 10 | Dark | (at `DAT_004e8424`) |

### Target Flags (Field 10)

Field 10 is a character string where each character sets a bit in the flags byte:

| Character | Bit | Hex | Meaning |
|-----------|-----|-----|---------|
| `m` | 0 | 0x01 | Targets monsters |
| `e` | 1 | 0x02 | Targets environment/objects |
| `c` | 2 | 0x04 | Targets caster/party |
| `x` | 3 | 0x08 | Special targeting mode |

Characters are lowercased via `FUN_004cc91e` before comparison. Multiple flags can be set
(e.g. `"mc"` sets bits 0 and 2).

### Row Grouping

Every 11 rows, an extra header line is consumed (via `FUN_004cc17b`). This corresponds to the
school separator headers in the text file. The loop processes entries until an end address of
`0x4e4417` is reached, indicating a fixed maximum number of spell entries.

---

## 3. Spell Identification and Numbering

Spells are identified by a numeric ID starting at 1. The IDs map sequentially to the 9
schools:

| Spell IDs | School | Skill ID |
|-----------|--------|----------|
| 1 - 11 | Fire | 0x0C (12) |
| 12 - 22 | Air | 0x0D (13) |
| 23 - 33 | Water | 0x0E (14) |
| 34 - 44 | Earth | 0x0F (15) |
| 45 - 55 | Spirit | 0x10 (16) |
| 56 - 66 | Mind | 0x11 (17) |
| 67 - 77 | Body | 0x12 (18) |
| 78 - 88 | Light | 0x13 (19) |
| 89 - 99 | Dark | 0x14 (20) |
| 100 | Special | 0x05 (Blaster skill) |

The mapping from spell ID to skill ID is computed in the main spell casting handler
(`FUN_00427db8`):

```text
if spell_id < 12:  skill_id = 0x0C  (Fire)
if spell_id < 23:  skill_id = 0x0D  (Air)
if spell_id < 34:  skill_id = 0x0E  (Water)
if spell_id < 45:  skill_id = 0x0F  (Earth)
if spell_id < 56:  skill_id = 0x10  (Spirit)
if spell_id < 67:  skill_id = 0x11  (Mind)
if spell_id < 78:  skill_id = 0x12  (Body)
if spell_id < 89:  skill_id = 0x13  (Light)
if spell_id < 100: skill_id = 0x14  (Dark)
if spell_id == 100: skill_id = 0x05 (Blaster/Special)

```

---

## 4. Spell Schools

### Fire (Spells 1-11)

Offensive school focused on direct damage and area-of-effect attacks.

Notable spells by position within school:

- Spell 1: Torch Light (party buff)
- Spell 2: Fire Bolt (single-target projectile)
- Spell 3: Protection from Fire (buff with resistance)
- Spell 5: Haste (party speed buff)
- Spell 6: Fireball (AoE projectile)
- Spell 7: Fire Spike (area denial)
- Spell 8: Immolation (party aura)
- Spell 9: Meteor Shower (area bombardment)
- Spell 10: Inferno (indoor AoE)
- Spell 11: Incinerate (high-damage single target)

### Air (Spells 12-22)

Focused on speed, lightning, and utility.

### Water (Spells 23-33)

Focused on cold damage, healing, and enchantment.

### Earth (Spells 34-44)

Focused on physical protection, stone effects, and raw damage.

### Spirit (Spells 45-55)

Focused on healing, resurrection, and anti-undead effects.

### Mind (Spells 56-66)

Focused on mental effects, fear, charm, and psychic damage.

### Body (Spells 67-77)

Focused on curing conditions, regeneration, and physical enhancement.

### Light (Spells 78-88)

Powerful beneficial magic; requires good alignment or quest completion to learn.

### Dark (Spells 89-99)

Powerful destructive and necromantic magic; requires evil alignment or quest completion.

---

## 5. Mastery Level System

Each spell school skill has 4 mastery tiers. The player's mastery level is encoded in the
upper bits of the skill value stored in the player record.

### Mastery Encoding

The skill value is a 16-bit field where:

- **Bits 0-5** (mask `0x3F`): Skill level (1-63)
- **Bit 6** (mask `0x40`): Expert flag
- **Bit 7** (mask `0x80`): Master flag
- **Bit 8** (mask `0x100`): Grandmaster flag

Mastery level extraction:

```text
raw_skill = player.skills[skill_id]
skill_level = raw_skill & 0x3F
if (raw_skill & 0x100):  mastery = 4 (Grandmaster)
elif (raw_skill & 0x80): mastery = 3 (Master)
elif (raw_skill & 0x40): mastery = 2 (Expert)
else:                    mastery = 1 (Novice)

```

This is confirmed in the main casting handler where the skill is read and decomposed:

```c
uVar12 = FUN_0048f87a();       // Get raw skill value
puVar19 = uVar12 & 0x3F;      // Skill level (low 6 bits)

if (uVar12 & 0x100):  mastery = 4
elif (uVar12 & 0x80): mastery = 3
elif (uVar12 & 0x40): mastery = 2
else:                  mastery = 1

```

### Mastery Effects on Spells

Mastery level typically affects:

1. **Duration multiplier**: Higher mastery = longer buff/debuff duration
2. **Power multiplier**: Higher mastery = more damage dice, stronger healing
3. **Target count**: Some spells gain multi-target at higher mastery
4. **Special effects**: Master/Grandmaster unlocks additional spell behaviors
5. **Mana cost**: Cost formula varies by mastery tier

---

## 6. Casting Mechanics

### Spell Cast Entry Structure (20 bytes)

The spell casting queue uses entries of 20 bytes (0x14) at a runtime address:

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 2 | Spell ID (uint16, 0 = empty slot) |
| 0x02 | 2 | Caster index (player 0-3) |
| 0x04 | 4 | Flags (bit field) |
| 0x08 | 4 | Target reference |
| 0x0A | 2 | Skill override (if non-zero, replaces looked-up skill) |
| 0x0C | 4 | Spell power / parameter |
| 0x10 | 4 | Additional data |

### Mana Cost Calculation

Mana cost is read from the spell data table based on spell ID and mastery. The base mana
cost per spell is stored in a table at `DAT_004e3c46` with a stride of 8 bytes per spell,
indexed as:

```text
offset = (mastery + spell_id * 10) * 2
mana_cost = table[offset + 0x00]   // at DAT_004e3c46
recovery  = table[offset + 0x08]   // at DAT_004e3c4e

```

The cost is compared against the player's current SP (spell points) stored at player
record offset `+0x1940` (field index 0x650 in dword units). If the player cannot afford
the spell, the cast is cancelled:

```c
if (spell_id < 100 && (player.sp - mana_cost) < 0) {
    FUN_0044c1a1();   // "Not enough spell points" message
    cancel_spell();
}

```

When a skill override is provided (from event system opcode 0x29 or scrolls), the mana
cost is set to 0.

### Casting Failure (Fizzle)

When a player has active conditions that impair casting (checked via condition timer fields),
there is a 50% chance the spell fizzles:

```c
if (player has casting conditions && spell_id < 100) {
    if (rand() % 100 < 50) {
        // Spell fizzles
        // In turn-based: recovery time = 100
        // Show fizzle message, play sound 0xD1
        player.sp -= mana_cost;
        return;
    }
}

```

### Recovery Time

After casting, the player incurs a recovery delay that prevents further actions. In
turn-based combat (`DAT_00acd6b4 == 1`), recovery is tracked in a separate counter. The
recovery value comes from the spell data table alongside the mana cost.

---

## 7. Spell Effect Application

The main spell casting handler is `FUN_00427db8` (27,569 bytes), one of the largest
functions in the executable. It uses a massive switch statement on the spell ID to dispatch
to individual spell effect implementations.

### Spell Categories by Effect Type

Based on the switch cases in the main handler, spells group into these categories:

#### Projectile Damage Spells (launch missile)

Spell IDs: 2, 6, 11, 18, 26, 32, 34, 37, 57, 65, 70, 78, 97

These spells create a projectile object via `FUN_0042f5c9` (Combat_SpawnProjectile):

- Set up projectile origin at party position
- Direction from caster's facing (`DAT_00acd4f8` yaw, pitch)
- Projectile type from spell effect table
- Damage resolved on impact via the damage system

Projectile setup:

```c
projectile.x = party_x;
projectile.y = party_y;
projectile.z = party_z + (party_height / 3);
projectile.direction = FUN_0049aba0(x, y, z);  // Angle to target
projectile.source = (player_index << 3) | 4;    // Player source encoding
projectile.spell_id = spell_id;
projectile.skill_level = skill_level;
projectile.mastery = mastery;

```

#### Buff Spells (apply timed effect to party)

Spell IDs: 1, 3, 5, 8, 12, 13, 14, 17, 19, 21, 25, 36, 38, 46, 51, 55, 58, 69, 71,
73, 75

These spells call `FUN_00458519` to set a timed condition/buff on party members. The
buff system works as follows:

1. Calculate duration based on skill level and mastery
2. Convert duration to game ticks (multiply by 128)
3. Add current game time to get expiry time
4. Store in player buff slot via `FUN_00458519(expiry_time, mastery, power, flags)`
5. Apply visual overlay via `FUN_004a894d` (one call per party member, indices 0-3)

#### Direct Damage Spells (immediate effect)

Spell IDs: 10, 20

These apply damage directly to visible/nearby monsters without launching a projectile.
Example: Spell 10 (Fire Spike / Inferno variant) iterates over visible monsters and
calls `FUN_00439463` (DamageMonsterFromParty) for each.

#### Healing Spells

Spell IDs: Various within Spirit/Body/Light schools

Healing is applied by modifying player HP directly. Amount scales with skill level and
mastery.

#### Enchantment Spells

Spell IDs: 4 (Fire school enchantment)

These modify items in the player's inventory. The enchantment handler:

1. Gets the target inventory slot
2. Checks item validity (item ID range, no existing enchantment, correct type)
3. Sets enchantment type based on mastery:
   - Novice: enchantment 10 (0x0A)
   - Expert: enchantment 11 (0x0B)
   - Master: enchantment 12 (0x0C)
   - Grandmaster: enchantment 12, infinite duration
4. If not Grandmaster, sets an expiry timer

#### Teleportation Spells

Spell IDs: 16 (Fly), 21 (Water Walk), and others

These set party movement flags:

- `DAT_00ad45b0` bit 3 (0x08): Fly active
- `DAT_00ad45b0` bits 4-5 (0x10, 0x20): Water walk / special movement

#### Environment Restriction Spells

Some spells are restricted by map type:

- Spell 9 (Meteor Shower): outdoor only (`DAT_006be1e0 != 1`)
- Spell 10 (Inferno): indoor only (`DAT_006be1e0 != 2`)

---

## 8. Spell Damage Calculation

### Player Spell Damage (FUN_0048e189)

`FUN_0048e189` (18 bytes) is a simple accessor that returns the spell's base damage type:

```c
return spell_data[spell_id * 0x24];   // at DAT_005cbecc + spell_id * 0x24

```

This returns an index into the damage type table used by the resistance system.

### Comprehensive Damage Calculation (FUN_0048e19b)

The full spell damage calculation (`FUN_0048e19b`, 853 bytes) considers:

1. **Base weapon damage type** from equipped items (for weapon-enchantment spells)
2. **Skill-based bonus**: looked up from the player's skill in the matching school
3. **Stat bonus**: Intelligence/Personality modifier via `FUN_0048ea13`
4. **Equipment bonus**: `FUN_0048eaa6` and `FUN_0048fbf8` for artifact/enchantment bonuses
5. **Race/class bonus**: checked for specific class IDs (e.g., class 0x23 caps at 200)
6. **Condition penalties**: active Hour of Power / Day of Protection / etc.
7. **Special weapon bonus**: items 500, 0x3B (59), 0x29 (41) grant +20 bonus

The formula (simplified):

```text
damage = base_damage_type
       + skill_bonus
       + stat_modifier
       - equipment_penalty
       - special_weapon_bonus
       - condition_penalty
       - race_penalty
       + misc_buffs

if (damage < 0): damage = 0

```

### Damage Type Enumeration

From the resistance and damage application functions, damage types observed:

| Type ID | Element |
|---------|---------|
| 0 | Fire |
| 1 | Air / Lightning |
| 2 | Water / Cold |
| 3 | Earth / Physical |
| 4 | Special (magic-physical) |
| 6 | Fire (alt) |
| 7 | Mind |
| 8 | Body |
| 9 | Light |
| 10 | Dark |
| 11 | Air (alt) |
| 12 | Spirit |
| 14 | Air (lightning) |
| 17 | Earth (alt) |

### Dice-Based Damage Rolling

Actual damage is rolled by `FUN_00452b5a` (RollDice), which takes a dice count and die size:

```c
int RollDice(int dice_count, int die_size) {
    int total = 0;
    if (die_size == 0) return 0;
    for (int i = 0; i < dice_count; i++) {
        total += 1 + (rand() % die_size);
    }
    return total;
}

```

---

## 9. Spell Resistance

### Monster Resistance Check (FUN_0043b006)

`FUN_0043b006` (116 bytes) calculates the effective resistance a monster has against a
given damage type. It is called during damage application in `FUN_00439463`.

```c
int CalculateResistance(int damage_type, int monster_hp, int param3, int param4) {
    int resistance = 0;
    if (damage_type == 7) {
        // Mind-type: special handling for certain monster states
        if (param3 > 0 && (param3 < 3 || param3 == 3 || param3 == 4)) {
            resistance = RollDice(param1, param2);
        }
    } else if (damage_type == 0x2C) {
        // Type 44: percentage-based resistance
        resistance = (DAT_004e3fc8 + monster_hp * 2) * param4 / 100;
    } else {
        // Standard elemental resistance
        resistance = RollDice(param1, param2);
        resistance += spell_resistance_table[damage_type * 0x14];
    }
    return resistance;
}

```

The resistance table is at `DAT_004e3c58`, indexed by damage type with stride 0x14 (20 bytes
per entry).

### Monster-to-Player Resistance

Monster spell damage against players uses a similar path through `FUN_0043b07a` and
`FUN_0043b1d3`, which:

1. Check the source type (player, monster, or projectile object)
2. Look up monster attack type (at offsets `+0x19`, `+0x1F`, `+0x25`, `+0x27` in monster
   record)
3. Apply the damage type to the player's elemental resistance stats
4. Roll damage dice and subtract resistance

### Player Resistance Stats

Player resistance values are stored at specific offsets in the player record and are
checked during incoming damage:

| Player Offset | Resistance Type |
|---------------|----------------|
| +0x1774 | Fire resistance |
| +0x1776 | Air resistance |
| +0x1778 | Water resistance |
| +0x177A | Earth resistance |
| +0x177C | Mind resistance |
| +0x177E | Body resistance |
| +0x1780 | (additional) |

These are 16-bit (short) values. Resistance reduces incoming damage and can provide
immunity at sufficiently high levels.

### Resistance Buff Spells

Protection spells (3, 14, 25, 36, 58, 69) add temporary resistance bonuses. The bonus
amount and duration depend on skill level and mastery:

```text
For Protection from Fire (spell 3):
  Novice:       resistance_bonus = skill_level,     duration = skill_level * 3600s
  Expert:       resistance_bonus = skill_level * 2,  duration = skill_level * 3600s
  Master:       resistance_bonus = skill_level * 3,  duration = skill_level * 3600s
  Grandmaster:  resistance_bonus = skill_level * 4,  duration = skill_level * 3600s

```

---

## 10. Duration Calculation

### Base Duration Formula

Spell durations are calculated from the skill level and mastery, then converted to game
ticks by left-shifting by 7 (multiplying by 128):

```text
game_ticks = duration_seconds * 128
expiry_time = current_game_time + game_ticks

```

Where `current_game_time` is the 64-bit value at `DAT_00acce64:DAT_00acce68`.

### Duration Patterns by Mastery

Observed duration formulas from the decompiled switch cases:

#### Pattern A: Hour-based (0xE10 = 3600 seconds)

Used by: Spells 1, 3, 12, 14, 19, 25, 36

```text
duration = skill_level * 3600   // 1 hour per skill level

```

#### Pattern B: Minute-based with mastery scaling

Used by: Spells 5, 8, 13, 17, 38, 51

```text
Novice:       (skill_level + 15) * 60     // (skill + 15) minutes
Expert:       (skill_level + 15) * 60     // Same as Novice
Master:       (skill_level + 5) * 180     // (skill + 5) * 3 minutes
Grandmaster:  (skill_level + 15) * 240    // (skill + 15) * 4 minutes

```

#### Pattern C: Tiered duration

Used by: Spells 17, 38, 51 (Shield/protection type spells)

```text
Novice:  (skill_level + 3) * 300    // (skill + 3) * 5 minutes
Expert:  (skill_level + 3) * 300    // Same
Master:  (skill_level + 1) * 900    // (skill + 1) * 15 minutes
GM:      (skill_level + 1) * 3600   // (skill + 1) hours

```

#### Pattern D: Short duration (combat spells)

Used by: Spells 8 (Immolation type)

```text
Novice/Expert/Master: skill_level * 60   // skill minutes
Grandmaster:          skill_level * 600  // skill * 10 minutes

```

#### Pattern E: Permanent / infinite

Grandmaster-level enchantment spells (e.g., spell 4 at GM) have no duration timer set,
making the effect permanent until dispelled.

---

## 11. Area of Effect

### Targeting Modes

Spells use different targeting modes based on their category:

#### Single-Target Projectile

Most offensive spells launch a single projectile toward the crosshair target. The projectile
travels in a straight line and hits the first valid target.

#### Multi-Projectile (Spray)

Spell 15 (and similar) creates a spray of projectiles distributed across an angular range:

```c
total_spread = (DAT_005c84e8 * 60) / 360;    // Angular spread in game units
step = total_spread / (projectile_count - 1);

for (angle = -total_spread/2; angle <= total_spread/2; angle += step) {
    projectile.direction = base_direction + angle;
    spawn_projectile();
}

```

Projectile count by mastery:

- Novice: 3
- Expert: 5
- Master: 7
- Grandmaster: 9

#### Area Bombardment (Meteor Shower)

Spell 9 drops multiple impacts in a circular area around the target:

```c
// Outdoor only (DAT_006be1e0 != 1)
impact_count = (mastery <= 3) ? 16 : 20;

for (i = 0; i < impact_count; i++) {
    offset_x = rand() % 1000;
    offset_y = rand() % 1024 - 512;    // random in -512..+511
    offset_z = rand() % 1024 - 512;
    spawn_projectile_at(target + offset);
}

```

#### Room-Wide (Inferno)

Spell 10 targets all monsters in the current indoor room:

```c
// Indoor only (DAT_006be1e0 != 2)
visible_count = FUN_0046a6b0();     // Get visible monster count

for (i = 0; i < visible_count; i++) {
    monster_index = visible_monster_list[i];
    // Get monster position
    target_x = monster[index].x;
    target_y = monster[index].y;
    target_z = monster[index].z;
    // Apply damage directly
    DamageMonsterFromParty(monster_index);
}

```

#### Party-Wide Buff

Buff spells that affect all 4 party members iterate with stride 0x1B3C (player record
size = 6972 bytes):

```c
player_base = DAT_00acd804;
for (i = 0; i < 4; i++) {
    apply_buff(player_base + i * 0x1B3C, ...);
    play_visual_effect(i);    // FUN_004a894d per player slot
}

```

This is confirmed by the repeated 4-call pattern of `FUN_004a894d` with player indices
0, 1, 2, 3 observed in many buff spell implementations.

#### Summoning (Fire Spike / similar)

Spell 7 and similar summoning spells have a maximum active count based on mastery:

```text
Novice: 3 active
Expert: 5 active
Master: 7 active
GM:     9 active

```

The function counts existing active objects of the same type before allowing a new summon.

---

## 12. Spell Visual Effects

### Effect Animation System (seffects.cpp)

The spell visual effects are managed by the system referenced in `seffects.cpp`. The primary
functions are:

- `FUN_004a9030` (1099 bytes): **LoadSpellEffectAssets** - Preloads all spell animation
  sprites and sound effects at initialization
- `FUN_004a894d` (303 bytes): **SetPlayerSpellOverlay** - Assigns a visual overlay animation
  to a specific player portrait slot
- `FUN_004a8bb7` (1027 bytes): **UpdateSpellEffects** - Renders ongoing spell effects
  including the casting animation and active spell overlays
- `FUN_004a8fba` (118 bytes): **AdvanceSpellOverlays** - Advances per-player overlay
  animation timers

### Spell Effect Asset Names

The following sprite/sound assets are loaded at initialization:

**Particle effects:**

- `effpar01`, `effpar02`, `effpar03` - General spell particle effects

**Spell-specific animations (sprite sets):**

- `spell01` through `spell97c` - Individual spell cast animations
- Named by spell ID: `spell01`, `spell02`, `spell03`, `spell09`, `spell11`, `spell14`,
  `spell17`, `spell18`, `spell21`, `spell22`, `spell25`, `spell26`, `spell27`, `spell29`,
  `spell36`, `spell38`, `spell39`, `spell39c`, `spell41`, `spell46`, `spell51`, `spell55`,
  `spell57c`, `spell58`, `spell62`, `spell65`, `spell66`, `spell69`, `spell70`, `spell71`,
  `spell73`, `spell75`, `spell76`, `spell84`, `spell90`, `spell92`, `spell93`, `spell96`,
  `spell97`, `spell97c`

**Category-based overlay animations:**

- `spheal1` - Healing overlay type 1 (used by spells 0x31, 0x38, 0x43)
- `spheal2` - Healing overlay type 2 (used by spells 0x36, 0x3D, 0x40, 0x44, 0x48, 0x4A, 0x60)
- `spheal3` - Healing overlay type 3 (used by spells 0x4D, 99/0x63)
- `spboost1` - Boost overlay type 1 (used by spells 0x0D, 0x2D, 0x2F, 0x96)
- `spboost2` - Boost overlay type 2 (used by spells 0x13, 0x1B, 0x32, 0x97)
- `spboost3` - Boost overlay type 3 (used by spells 0x53, 0x55, 0x56, 0x58, 0x98)
- `sp57c` - Special effect for spell 57 variant

### Player Overlay Structure (16 bytes per slot)

Each player can have an active spell overlay. The overlay structure:

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 2 | Spell ID (0 = no overlay) |
| 0x04 | 4 | Current animation frame timer |
| 0x08 | 4 | Total animation duration |
| 0x0C | 4 | Sprite sequence index |

There are 4 overlay slots (one per player), accessed at 16-byte intervals starting at
the spell effect manager base + 0x208.

### Casting Animation

The casting bar animation (`FUN_004a8bb7`) uses a fade-in/fade-out curve:

```c
progress = (float)remaining_time / (float)total_time;
alpha = 1.0 - progress * progress;
if (alpha > threshold) {
    alpha = 1.0 - (alpha - threshold) * scale_factor;
}
FUN_004a5281(sound_id, alpha);    // Play sound with volume
remaining_time -= frame_delta;     // DAT_0050ba7c = frame time

```

The casting animation also renders the `spell84` sprite sequence as a fullscreen overlay
using the rendering pipeline.

---

## 13. Key Functions

| Address | Size | Suggested Name | Description |
|---------|------|---------------|-------------|
| `FUN_00453876` | 718 | `SpellData_LoadFromFile` | Parses `spells.txt` into spell data table |
| `FUN_00427db8` | 27569 | `SpellCast_ProcessAll` | Main spell casting handler (giant switch) |
| `FUN_0048e189` | 18 | `SpellData_GetDamageType` | Returns spell damage type from table |
| `FUN_0048e19b` | 853 | `Player_CalculateSpellDamage` | Full spell damage calculation |
| `FUN_0048e4f0` | 109 | `Player_GetMeleeRecovery` | Recovery time with spell bonuses |
| `FUN_0048e55d` | 194 | `Player_GetRangedRecovery` | Ranged recovery with spell bonuses |
| `FUN_0048f87a` | 894 | `Player_GetSkillMastery` | Gets skill value and computes bonuses |
| `FUN_00452b5a` | 42 | `RollDice` | Rolls N dice of M sides |
| `FUN_0043b006` | 116 | `Monster_CalculateResistance` | Monster resistance calculation |
| `FUN_0043b07a` | 345 | `Monster_ApplyPlayerSpellDamage` | Applies player spell to monster |
| `FUN_0043b1d3` | 560 | `Monster_ApplyMonsterSpellDamage` | Monster vs monster spell damage |
| `FUN_004a9030` | 1099 | `SpellEffects_LoadAssets` | Loads all spell animation assets |
| `FUN_004a894d` | 303 | `SpellEffects_SetPlayerOverlay` | Sets spell overlay on player portrait |
| `FUN_004a8bb7` | 1027 | `SpellEffects_Update` | Updates/renders spell visual effects |
| `FUN_004a8b8c` | 43 | `SpellEffects_InitCastAnim` | Initializes casting bar animation |
| `FUN_004a8fba` | 118 | `SpellEffects_AdvanceOverlays` | Advances overlay animation timers |
| `FUN_004a1f71` | 80 | `SpellEffects_OnCastComplete` | Finalizes spell cast, triggers rendering |
| `FUN_00458519` | ~127 | `Player_SetTimedBuff` | Sets timed buff on player |
| `FUN_004585be` | ~40 | `Player_ClearTimedBuff` | Clears a timed buff |
| `FUN_0042f5c9` | ~500 | `Combat_SpawnProjectile` | Creates spell projectile object |
| `FUN_004276e7` | ~50 | `SpellCast_ValidateTarget` | Validates spell targeting |
| `FUN_0042eb46` | ~50 | `SpellCast_GetPlayerSlot` | Gets player slot for overlay |
| `FUN_00491f7f` | 570 | `Party_UpdateSpellEffectUI` | Updates party spell effect UI display |
| `FUN_0045490e` | -- | `SpellData_LookupMonsterSpell` | Looks up monster spell by name |
| `FUN_00454e66` | -- | `SpellData_LookupElement` | Looks up element type by name |

---

## 14. Key Data Structures

### Spell Data Record (0x24 = 36 bytes)

Base array at `DAT_005cbecc` (approximately), indexed by spell ID:

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 1 | Damage type / school index |
| 0x01-0x03 | 3 | Padding |
| 0x04 | 4 | Text pointer A |
| 0x08 | 4 | Text pointer B (Expert) |
| 0x0C | 4 | Text pointer C (Master) |
| 0x10 | 4 | Text pointer D (Grandmaster) |
| 0x14 | 4 | Base mana cost parameter |
| 0x18 | 4 | Casting delay parameter |
| 0x1C | 4 | Recovery parameter |
| 0x20 | 4 | School / damage type (parsed from field 3) |

### Mana Cost / Recovery Table

At `DAT_004e3c46`, indexed by `(mastery + spell_id * 10) * 2`:

| Relative Offset | Size | Field |
|-----------------|------|-------|
| +0x00 | 2 | Mana cost (uint16) |
| +0x08 | 2 | Recovery time (uint16) |

### Spell Target Flags Array

At `DAT_004e3c6e`, with stride 0x14 (20 bytes between entries):

| Bit | Meaning |
|-----|---------|
| 0 | Target monsters |
| 1 | Target environment |
| 2 | Target caster/party |
| 3 | Special mode |

### Player Buff Slot

Each player has multiple timed buff slots, each 20 bytes:

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 4 | Expiry time (low dword) |
| 0x04 | 4 | Expiry time (high dword) |
| 0x08 | 4 | Mastery level |
| 0x0C | 4 | Power / skill level |
| 0x10 | 4 | Flags |

### Spell Queue Entry

At runtime address (`local_f8` in the main handler), 20 bytes per entry:

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 2 | Spell ID (0 = unused) |
| 0x02 | 2 | Caster player index (0-3) |
| 0x04 | 4 | Target flags / conditions |
| 0x08 | 4 | Target entity reference |
| 0x0A | 2 | Skill override (0 = use player skill) |
| 0x0C | 4 | Spell parameter / power |
| 0x10 | 4 | Additional parameter |

---

## 15. Key Global Variables

### Spell System State

| Address | Type | Description |
|---------|------|-------------|
| `DAT_005e4af4` | ptr | Loaded `spells.txt` file buffer |
| `DAT_005cbecc` | base | Spell data table (0x24 stride per spell) |
| `DAT_004e3c46` | u16[] | Mana cost table |
| `DAT_004e3c4e` | u16[] | Recovery time table |
| `DAT_004e3c6e` | u8[] | Target flags array (0x14 stride) |
| `DAT_004e3c52` | u16[] | Weapon damage type table |

### Player Spell State

| Address | Type | Description |
|---------|------|-------------|
| `DAT_00acd804` | base | Player 0 record base (stride 0x1B3C) |
| Player+0x0108 | u16[] | Player skill array (indexed by skill ID) |
| Player+0x01F0 | i32[] | Player inventory / spell item array |
| Player+0x157C | i32[] | Spellbook grid (14 columns x 10 rows) |
| Player+0x1774 | int16[8] | Player elemental resistance stats |
| Player+0x1940 | i32 | Current spell points (SP / mana) |
| Player+0x1944 | i32 | Player age (for age-dependent calculations) |

### Combat and Targeting

| Address | Type | Description |
|---------|------|-------------|
| `DAT_00acd6b4` | i32 | Combat mode (0 = real-time, 1 = turn-based) |
| `DAT_00acd554` | i32 | Active spell school restriction |
| `DAT_00acd558` | i32 | Additional spell restriction flag |
| `DAT_006be1e0` | i32 | Map type (1 = indoor, 2 = outdoor) |
| `DAT_00acce64` | u32 | Game time low dword |
| `DAT_00acce68` | u32 | Game time high dword |
| `DAT_005c84e8` | i32 | Angular conversion constant |
| `DAT_004f86f4` | i32 | Turn-based projectile counter |
| `DAT_004f86dc` | i32 | Turn-based action type |
| `DAT_0050ba7c` | i32 | Frame time delta (for animation) |

### Spell Effect Rendering

| Address | Type | Description |
|---------|------|-------------|
| `DAT_00e31af0` | ptr | Hardware rendering interface |
| `DAT_005e4fc4` | i32 | Spell effect redraw flag |
| `DAT_0050ba54` | i32 | Frame time delta (overlay animation) |

### Event System Integration

| Address | Meaning |
|---------|---------|
| Opcode `0x29` | EVT_CAST_SPELL: Cast spell on party from event script |
| Opcode `0x2B` | EVT_CHECK_SKILL: Check player skill level with mastery |

Event opcode 0x29 layout:

```asm
Offset +5: spell_id (byte)
Offset +6: spell_sub_id (byte)
Offset +7: skill_level_override (byte)

```

---

## Notes

- All addresses are virtual addresses from `MM7-Rel.exe` (v1.21).
- Function names prefixed with `FUN_` are Ghidra auto-generated identifiers.
- `DAT_` prefixed names are Ghidra auto-generated global variable references.
- The main spell handler at `FUN_00427db8` (27,569 bytes) is one of the largest functions in
  the binary, containing individual implementations for all ~100 spells.
- Spell effect rendering uses both software (billboard sprites) and hardware (Direct3D
  render states) paths, selected at runtime based on `DAT_00e31af0`.
- The spell system integrates tightly with the combat system (`Damage.cpp`), event system
  (`Events.cpp`), and timer system for buff duration tracking.
- Monster spell names like "Fireball", "Lightning", "Spirit", "Light" are looked up by
  string comparison in `FUN_0045490e` with the error message "Unknown monster spell %s"
  for unrecognized entries.

---

**Trademark Notice**: Not affiliated with or endorsed by the IP holder. All trademarks
belong to their respective owners.
