---
title: "Combat System"
summary: "Combat resolves attacks through hit checks, damage calculation, resistance, and recovery."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Combat System

Combat resolves attacks through hit checks, damage calculation, resistance, and recovery.

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
2. [Combat Modes](#2-combat-modes)
3. [Attack Types](#3-attack-types)
4. [Damage Calculation Pipeline](#4-damage-calculation-pipeline)
5. [Hit/Miss Determination](#5-hitmiss-determination)
6. [Resistance System](#6-resistance-system)
7. [Player Combat](#7-player-combat)
8. [Monster Combat](#8-monster-combat)
9. [Knockback and Stun](#9-knockback-and-stun)
10. [Death and Unconscious Handling](#10-death-and-unconscious-handling)
11. [Combat Encounter Detection](#11-combat-encounter-detection)
12. [Key Constants and Enumerations](#12-key-constants-and-enumerations)
13. [Function Map](#13-function-map)
14. [Integration notes](#integration-notes)

---

## 1. Overview

MM7 uses a **hybrid real-time/turn-based** combat system. In real-time mode, all
participants act simultaneously with action delays governed by recovery timers. When the
player presses the combat key (`KEY_COMBAT`, string at 0x004e90d8), the game switches to
turn-based mode where each participant takes ordered turns.

The central damage resolution function (`DamageMonsterFromParty` at 0x00439463, 2,483 bytes)
orchestrates the entire pipeline for player-attacks-monster. A parallel function
(`DamageMonsterFromMonster` at 0x0043b1d3) handles monster-on-monster damage, and
`DamagePlayerFromMonster` (at 0x00439fee, also 2,483 bytes) handles monster-attacks-player.

### Core Combat Flow

```text
 Attacker Action
       |
       v
 +---------------------+
 | Identify Source Type |  (bits 0-2 of param: 2=projectile, 3=monster, 4=player)
 +---------------------+
       |
       v
 +---------------------+
 | Range Check         |  (3D distance approximation for projectiles)
 +---------------------+
       |
       v
 +---------------------+
 | Determine Attack    |  (melee=0x66, ranged_spell=0x64, ranged_phys=0x65,
 | Type                |   spell=0x27, auto_stun=0x22)
 +---------------------+
       |
       v
 +---------------------+
 | Calculate Base      |  (weapon dice, skill bonuses, enchantments)
 | Damage              |
 +---------------------+
       |
       v
 +---------------------+
 | Hit/Miss Roll       |  (attack ability vs armor class + buffs)
 +---------------------+
       |
 [miss]--> Play miss sound, return
       |
 [hit]
       v
 +---------------------+
 | Apply Resistance    |  (element-based, multi-roll reduction)
 +---------------------+
       |
       v
 +---------------------+
 | Subtract HP         |  (monster HP at DAT_005ff000 + idx * 0x1A2)
 +---------------------+
       |
       v
 +---------------------+
 | Post-Hit Effects    |  (knockback, stun, status conditions, XP award)
 +---------------------+
       |
       v
 +---------------------+
 | Death Check         |  (HP <= 0 -> death sequence)
 +---------------------+

```

---

## 2. Combat Modes

### Real-Time Mode

- Default gameplay mode (`DAT_00acd6b4 == 0`)
- All entities act simultaneously
- Recovery time controls action speed
- Stun duration calculated via `__ftol()` float conversion

### Turn-Based Mode

- Activated when `DAT_00acd6b4 == 1`
- Turn indicator sounds: `turn0` through `turn4`, `turnstart`, `turnstop`, `turnhour`
  (loaded by `Combat_LoadSounds` at 0x0042f3b2)
- Stun duration in turn-based mode: fixed at 20 (0x14) ticks
- UI mode set to 3 (`DAT_004e28d8 == 3`) during combat

### Combat Sound Table

| Global Address | Sound Name | Purpose |
|----------------|-----------|---------|
| `DAT_0050c7ec` | `turn0` | Turn indicator 0 |
| `DAT_0050c7f0` | `turn1` | Turn indicator 1 |
| `DAT_0050c7f4` | `turn2` | Turn indicator 2 |
| `DAT_0050c7f8` | `turn3` | Turn indicator 3 |
| `DAT_0050c7fc` | `turn4` | Turn indicator 4 |
| `DAT_0050c800` | `turnstop` | Turn end |
| `DAT_0050c804` | `turnhour` | Hour passing |
| `DAT_0050c810` | `turnstart` | Turn start |

---

## 3. Attack Types

The attack type is determined from the projectile/source record at offset +0x48 (field at
`local_10 + 0x24` in the decompilation, which is an `int` field).

### Attack Type Enumeration

| Value | Hex  | Name              | Damage Source                     | Uses Resistance? |
|-------|------|-------------------|-----------------------------------|------------------|
| 34    | 0x22 | Auto-Stun         | No damage roll; forced stun       | No               |
| 39    | 0x27 | Spell Direct      | `CalculateSpellDamage`            | Yes              |
| 100   | 0x64 | Ranged Spell      | `CalculateRangedDamage` + element | Yes (via spell)  |
| 101   | 0x65 | Ranged Physical   | `CalculateRangedDamage`           | No               |
| 102   | 0x66 | Melee             | `CalculateMeleeBaseDamage`        | No               |
| other | --   | Generic Spell     | `CalculateSpellDamage`            | Yes              |

### Attack Type Processing

```text
if attack_type == 0x66:       # Melee
    GetWeaponSkillLevel()
    CalculateMeleeBaseDamage()
    # Check for double attack (bladed skill >= 3)
    # Check for stun/knockback (polearm skill >= 3/4)

elif attack_type == 0x64:     # Ranged Spell
    CalculateRangedDamage()
    if projectile.spell_type != 0 and projectile.skill == 3:
        enable_knockback = true
    physical_damage = true

elif attack_type == 0x65:     # Ranged Physical
    CalculateRangedDamage()
    physical_damage = true

elif attack_type == 0x27:     # Spell Direct
    CalculateSpellDamage()
    ApplyResistance()
    physical_damage = false

elif attack_type == 0x22:     # Auto-Stun
    force_stun = true

else:                         # Generic Spell
    CalculateSpellDamage()
    ApplyResistance()
    physical_damage = false
    force_knockback = true

```

---

## 4. Damage Calculation Pipeline

### 4.1 Source Identification

The `param_1` argument encodes the source:

```text
Bits 0-2: Source type
    2 = projectile/object (missile)
    3 = monster (AI actor)
    4 = player character

Bits 3+:  Source index
    Player index = param_1 >> 3  (0-3)
    Monster index = param_1 >> 3

```

Player record stride: **0x1B3C** (6,972 bytes), base at `DAT_00acd804`.
Monster record stride: **0x344** (836 bytes), base at `DAT_005feffc`.

#### 4.2 Range Check (Projectile Sources)

For projectile-type sources (`type == 2`), the engine computes 3D distance using a
fast approximation:

```text
dx = abs(target.x - source.x)
dy = abs(target.y - source.y)
dz = abs(target.z - source.z)

# Sort into max, mid, min
distance = max + (min / 4) + (mid * 11 / 32)

```

Range thresholds for projectile validity:

- If `projectile.max_range != 99`: distance must be within allowed range
- If the target's "in-range" flag (byte at monster+1, bit 2) is clear and distance
  exceeds `0x13FF` (5,119): attack is rejected

#### 4.3 Melee Base Damage (FUN_0048cdc1)

The `CalculateMeleeBaseDamage` function computes damage for melee attacks:

```text
Algorithm:
1. Check if player is unarmed (FUN_0048d65c returns 1):
   - Unarmed: damage = rand() % 3 + 1  (i.e., 1-3)

2. If player has main-hand weapon (slot index from player + 0x194C):
   a. Look up weapon item record at player + 0x1F0 + slot_index * 0x24
   b. Get item ID, compute item_data_offset = itemID * 0x30
   c. Read dice from item data table at DAT_005d2880:
      - dice_count = DAT_005d2882[offset]   (byte)
      - dice_sides = DAT_005d2883[offset]   (byte)
      - dice_bonus = DAT_005d2884[offset]   (byte)
   d. Roll: for i in 0..dice_count: total += 1 + rand() % dice_sides
   e. Add bonus: total += dice_bonus
   f. Check for 2x damage multiplier (weapon vs monster type):
      - Certain enchantment types (0x40, 0x27, 0x28, 0x3F, 0x41)
        and specific item IDs (0x1FB, 0x1FC, 0x205, 0x20F)
        deal double damage against matching monster types
   g. Check for critical hit (bladed weapons, skill type 0x02):
      - Skill level >= 3 AND rand() % 100 < 10: damage *= 3

3. If player has off-hand weapon (slot index from player + 0x1948):
   - Same dice rolling procedure
   - Same 2x/3x multiplier checks
   - Off-hand damage added to main-hand damage

4. If attack type is pure melee (param_2 == 0), add stat bonuses:
   - Get melee stat bonus (FUN_0048c922 -> FUN_0048ea13)
   - Add skill bonus (FUN_0048fbf8 with param 0x1A)
   - Add equipment bonus (FUN_0048f734 with param 0x1A)
   - Add character-level bonus (player + 0x1A92, signed byte)

5. Minimum damage = 1

```

#### 4.4 Ranged Damage (FUN_0048d1e4)

```cpp
Algorithm:
1. Check if player has ranged weapon equipped (FUN_0048d690 with slot 2)
2. If no ranged weapon, return 0
3. Look up weapon at player + 0x1F0 + ranged_slot * 0x24
4. Roll dice same as melee (dice_count, dice_sides, dice_bonus)
5. Apply 2x multiplier for matching enchantments vs monster type
6. If weapon skill level >= 4 (master) and ranged skill set:
   - Add (player + 0x112 & 0x3F) bonus damage
7. Return total

```

#### 4.5 Spell Damage (FUN_0048e189)

Spell damage type is looked up from the spell data table:

```text
damage_type = DAT_005cbecc[spell_id * 0x24]

```

This returns an element type index used for resistance calculation.

The actual spell damage amount is computed elsewhere (by the spell casting system) and
passed into the combat resolution pipeline.

#### 4.6 Weapon Enchantment Damage (FUN_00439e16)

After base damage, enchantment-based bonus damage is calculated:

```text
Algorithm:
1. Check each weapon slot (main-hand, off-hand)
2. For each equipped weapon:
   - Read enchantment data from item record
   - Roll additional enchantment dice via FUN_00427522
   - Add to total damage
3. For pure melee (no projectile), iterate twice (main + off-hand)
4. For projectile attacks, iterate once

```

#### 4.7 Double Damage Roll (Two-Weapon Fighting)

If the player has a weapon with the "double attack" property (determined by skill mastery
and an active buff timer):

```text
if no_projectile AND has_double_attack_buff:
    # Check buff timers at player + 0xACF008 (64-bit time value)
    if buff_time > 0:
        extra = RollDamage()
        total_damage += extra

```

---

## 5. Hit/Miss Determination

### 5.1 Player Attacks Monster (FUN_004272ac - CheckHitChance)

```text
Parameters:
    param_1: unused (attack context)
    param_2: pointer to monster record
    param_3: attack mode (0=normal, 2=spell_bonus, 3=double_AC)
    param_4: attacker bonus

Algorithm:
1. Read monster AC from monster_record + 0x70
2. Check for "halved AC" condition:
   - If monster buff at +0x158 is active (64-bit time > 0):
     AC = AC / 2
3. Check for AC buffs on monster:
   - Buff at +0x1B8 active: add buff_value from +0x1CC
   - Buff at +0x1D8 active: use max(current, buff_value from +0x1DC)
4. Effective_AC = base_AC + buff_bonus
5. Get attacker's attack bonus:
   - If attack_mode == 0: use FUN_0048ccdb(0) for base attack
   - Else: use FUN_0048d09f() for ranged/modified attack
6. Roll: roll = rand() % (Effective_AC + 30 + attack_bonus * 2)
7. Compare based on attack mode:
   - mode 0 or 1: hit if (roll + param_4) >= (Effective_AC + 15)
   - mode 2: hit if (roll + param_4) >= (Effective_AC + (AC+15)/2 + 15)
   - mode 3: hit if (roll + param_4) >= (Effective_AC * 2 + 30)

```

The key formula:

```text
roll_range = AC + 30 + (attack_bonus * 2)
hit_threshold = AC + 15  (normal mode)
roll = rand() % roll_range + attacker_bonus
success = roll >= hit_threshold

```

#### 5.2 Monster Attacks Monster (FUN_00427372)

```text
Algorithm:
1. Read defender AC from defender + 0x70
2. Apply halved-AC buff check (defender + 0x158)
3. Collect defender AC buffs (offsets +0x1B8, +0x1D8)
4. Collect attacker attack buffs (offsets +0x1B8, +0x1E8)
5. Check attacker's "Heroism"-style buff (offset +0x188):
   - Adds buff_value from +0x18C to attack bonus
6. Roll: rand() % (AC + buffs + 10 + attacker_level * 2)
7. Hit if roll + 1 + attack_bonus > AC + buffs + 5

```

#### 5.3 Monster Attacks Player (FUN_00427464)

Uses the same general framework but checks player-side buffs and equipment-based AC
contributions.

---

## 6. Resistance System

### 6.1 Monster Resistance Check (FUN_0043b006 - DamageMonster_ApplyResistance)

```text
Parameters:
    param_1: element type
    param_2: monster current HP (short)
    param_3: resistance sub-type (1-4)
    param_4: damage amount

Algorithm:
    if element == 7:              # Physical
        if sub_type in [1, 2, 3, 4]:
            reduction = FUN_00452b5a()  # Look up specific physical resist
    elif element == 0x2C:         # Magic (general)
        reduction = (DAT_004e3fc8 + monster_HP * 2) * damage / 100
    else:                         # Elemental (fire, air, water, earth, etc.)
        reduction = FUN_00452b5a()
        reduction += DAT_004e3c58[element * 0x14]  # Base resistance table

    return reduction

```

#### 6.2 Resistance Damage Reduction (FUN_00427522 - RollDamage)

This function applies resistance-based damage reduction through **cascading rolls**:

```text
Parameters:
    param_1: pointer to monster record
    param_2: resistance type (0-10, selects resistance byte)
    param_3: incoming damage

Resistance Byte Offsets (within monster record):
    Type 0:  +0x50  (Fire resistance)
    Type 1:  +0x51  (Air/Electricity resistance)
    Type 2:  +0x52  (Water/Cold resistance)
    Type 3:  +0x53  (Earth resistance)
    Type 4:  +0x59  (Spirit resistance)
    Type 5:  (default, 0)
    Type 6:  +0x55  (Body resistance)
    Type 7:  +0x54  (Mind resistance)
    Type 8:  +0x56  (Light resistance)
    Type 9:  +0x57  (Dark resistance)
    Type 10: +0x58  (Physical/general resistance)

Algorithm:
1. Read resistance value from monster + offset[type]
2. If resistance value has a buff active (monster + 0x1B8):
   - Add buff bonus from monster + 0x1BC

3. If resistance >= 200: immune (return 0 damage)

4. Cascading reduction rolls:
   threshold = resistance + buff_bonus + 30  (0x1E)

   roll = rand() % threshold
   if roll > 29:             # Failed to resist
       damage = damage       # Full damage
   else:
       damage = damage / 2   # Half damage
       roll = rand() % threshold
       if roll > 29:
           damage = damage   # Stay at half
       else:
           damage = damage / 2  # Quarter damage
           roll = rand() % threshold
           if roll > 29:
               damage = damage
           else:
               damage = damage / 2  # Eighth
               roll = rand() % threshold
               if roll > 29:
                   damage = damage
               else:
                   damage = damage / 2  # Sixteenth

5. Return final damage

```

The cascading roll means each resistance check can halve damage up to 4 times (total
reduction to 1/16th), with each halving controlled by `rand() % (resist + 30) < 30`.

#### 6.3 Critical Effect Roll (FUN_00427619 - CheckCritical)

Used to determine if status effects (stun, knockback) penetrate monster resistance:

```text
Resistance Byte Offsets: Same as RollDamage (type 0-10)

Algorithm:
1. Read resistance value from monster + offset[type]
2. If resistance >= 200: immune (return false)
3. Roll: rand() % (monster_level/4 + 30 + resistance)
4. If roll < 30: effect applies (return true)
5. Otherwise: resisted (return false)

Note: monster_level is read from monster + 0x34 and right-shifted by 2.

```

---

## 7. Player Combat

### 7.1 Player Record Offsets (Combat-Relevant)

| Offset    | Size  | Field                            |
|-----------|-------|----------------------------------|
| +0x0070   | i32 | Armor Class (base)               |
| +0x0080   | i64 | Dead condition timer             |
| +0x0108+  | i16 | Weapon skill level per skill     |
| +0x0112   | i16 | Ranged bonus (low 6 bits = value)|
| +0x014C   | i16 | Recovery time base               |
| +0x0154   | i64 | Shield buff timer                |
| +0x0158   | i64 | Halved-AC debuff timer           |
| +0x0194   | i64 | Charm/control timer              |
| +0x0198   | i64 | Berserk/fear timer               |
| +0x01B4   | i64 | AC buff 1 timer                  |
| +0x01B8   | i64 | AC buff 1 active flag            |
| +0x01BC   | i16 | AC buff 1 power                  |
| +0x01CC   | i16 | AC buff 2 power                  |
| +0x01D4   | i64 | AC buff 2 timer                  |
| +0x01DC   | i16 | AC buff 2 power (alternate)      |
| +0x01E4   | i64 | Attack buff timer                |
| +0x01EC   | i16 | Attack buff power                |
| +0x01F0   | 36B   | Equipped item slot 0             |
| +0x1810   | i64 | Global damage buff timer         |
| +0x1814   | i64 | Global damage buff (high dword)  |
| +0x1948   | i32 | Off-hand weapon slot index       |
| +0x194C   | i32 | Main-hand weapon slot index      |
| +0x1950   | i32 | Ranged weapon slot index         |
| +0x1A92   | i8    | Melee damage stat bonus          |
| +0x1A94   | i8    | Ranged attack stat bonus         |
| +0x1A96   | i8    | Ranged damage stat bonus         |

Player stride: **0x1B3C** (6,972 bytes). Up to 4 players (indices 0-3).

#### 7.2 Weapon Skill System

Weapon skill level is retrieved by `FUN_0045827d` (GetWeaponSkillLevel). The skill level
determines available combat abilities:

| Weapon Type | Byte Offset | Skill >= 3 (Expert)         | Skill >= 4 (Master)       |
|-------------|-------------|------------------------------|---------------------------|
| 0 (Bladed)  | +0x00       | Double attack chance         | --                        |
| 6 (Polearm) | +0x06       | Stun on hit                  | Knockback on hit          |
| 2 (Ranged)  | +0x02       | Bonus damage (+skill & 0x3F)| Bonus damage (+skill)     |

#### 7.3 Double Attack Chance

For bladed weapons (weapon type `'\0'` / type 0), when skill level >= 3:

```text
skill_value = GetSkillMastery()  # returns full mastery + level word
chance = skill_value & 0x3F      # low 6 bits = percentage
roll = rand() % 100
if roll < chance:
    double_attack = true          # Roll damage twice, add together

```

For polearm weapons (type `'\x06'`), the same formula controls stun (skill >= 3)
and knockback (skill >= 4) chances.

#### 7.4 Recovery Time

After attacking, the player enters a recovery period:

```text
recovery_base = player + 0x14C   # Base recovery time (ushort)
# Modified by weapon speed, skill mastery, and buffs
# Scaled by mastery tier:
#   Multiplier table at DAT_004edd48[skill_level]
#   Randomization: DAT_004edd5c[(rand() % 100) / 20]

```

In real-time mode, recovery is based on float-scaled game time.
In turn-based mode, recovery consumes the character's turn.

#### 7.5 Player Attack Ability (FUN_0048d09f)

```text
Algorithm:
1. Get main weapon item ID
2. If weapon is not a wand (item_id < 0x40 or > 0x41):
   a. Get base attack stat (FUN_0048cb1f)
   b. Add stat bonus (FUN_0048ea13)
   c. Add equipment bonus (FUN_0048eaa6 with param 0x1D)
   d. Add skill bonus (FUN_0048fbf8 with param 0x1D)
   e. Add level bonus (FUN_0048f734 with param 0x1D)
   f. Add character bonus (player + 0x1A94, signed byte)
3. If weapon IS a wand (0x40-0x41):
   - Use special wand attack formula (FUN_0048ccdb with param 1)
4. Return total attack ability

```

---

## 8. Monster Combat

### 8.1 Monster Record Layout (836 bytes = 0x344)

| Offset | Size  | Field                                |
|--------|-------|--------------------------------------|
| +0x00  | u32   | Flags (bit 5=AI disabled, 6-7=combat)|
| +0x01  | u8  | Status byte (bit 6-7 = engaged)      |
| +0x02  | u8  | Alert flags (bit 3 = alerted)        |
| +0x08  | i16 | Monster type / sprite ID             |
| +0x0C  | u8  | Level                                |
| +0x19  | u8  | Attack1 damage type                  |
| +0x1E  | u8  | Status flags (bit 4 = invisible)     |
| +0x1F  | u8  | Attack2 damage type                  |
| +0x25  | u8  | Special attack 1 type                |
| +0x27  | u8  | Special attack 2 type                |
| +0x34  | i32 | Group ID / level (byte at +0x34)     |
| +0x38  | i16 | Monster data table index             |
| +0x40  | i16 | Special ability data                 |
| +0x44  | i32 | Max HP                               |
| +0x50  | u8  | Fire resistance                      |
| +0x51  | u8  | Air resistance                       |
| +0x52  | u8  | Water resistance                     |
| +0x53  | u8  | Earth resistance                     |
| +0x54  | u8  | Mind resistance                      |
| +0x55  | u8  | Body resistance                      |
| +0x56  | u8  | Light resistance                     |
| +0x57  | u8  | Dark resistance                      |
| +0x58  | u8  | Physical resistance                  |
| +0x59  | u8  | Spirit resistance                    |
| +0x5C  | i32 | Treasure type                        |
| +0x60  | i16 | Radius / collision size              |
| +0x62  | i16 | Height                               |
| +0x66  | i16 | Position X                           |
| +0x68  | i16 | Position Y                           |
| +0x6A  | i16 | Position Z                           |
| +0x6C  | i16 | Velocity X                           |
| +0x6E  | i16 | Velocity Y                           |
| +0x70  | i16 | Velocity Z / Armor Class             |
| +0x88  | i16 | AI state                             |
| +0x8C  | i16 | Current HP                           |
| +0x124 | i16 | Event trigger ID                     |
| +0x164 | i32 | Hostility override (low dword)       |
| +0x168 | i32 | Hostility override (high dword)      |
| +0x194 | i64 | Charm timer                          |
| +0x198 | i64 | Summoned/controlled timer            |
| +0x1A0 | i64 | Shield buff timer                    |
| +0x2C4 | i32 | Alliance group                       |
| +0x2C8 | i32 | Hostility level override             |
| +0x308 | i32 | Last attacker ID                     |

Monster HP is accessed at: `DAT_005ff000 + monster_index * 0x1A2` (short-word stride).

#### 8.2 Monster AI States

| Value | Hex  | State       | Description                           |
|-------|------|-------------|---------------------------------------|
| 0     | 0x00 | Idle        | Not engaged, wandering                |
| 4     | 0x04 | Fleeing     | Running away from threat              |
| 5     | 0x05 | Dead        | Killed, playing death animation       |
| 7     | 0x07 | Pursuing    | Engaged, chasing target               |
| 11    | 0x0B | Stunned     | Temporarily unable to act             |
| 17    | 0x11 | Paralyzed   | Cannot move or act                    |
| 19    | 0x13 | Special     | Petrified / special status            |

#### 8.3 Monster Attack Patterns

Monsters have multiple attack types selected from their record:

```text
Attack selection (from FUN_00439fee, monster-attacks-player):
    attack_type = monster.attack_byte

    if attack_type == 0:  damage_element = Fire
    if attack_type == 1:  damage_element = Air
    ...

Monster attack data offsets (per attack, from monster record):
    Attack 1 type:    monster + 0x19   (byte, element type)
    Attack 1 chance:  monster + 0x18   (byte, probability)
    Attack 2 type:    monster + 0x1F   (byte, element type)
    Special atk 1:    monster + 0x25   (byte, spell-based damage type)
    Special atk 2:    monster + 0x27   (byte, spell-based damage type)
    Spell ability:    monster + 0x40   (short, spell type for special attack)

```

#### 8.4 Monster Attack Damage Calculation (FUN_0043b403)

Monster base damage is rolled from the monster data table (same dice system as player
weapons):

```text
item_data_offset = monster_type * 0x30
dice_count = DAT_005d2882[item_data_offset]
dice_sides = DAT_005d2883[item_data_offset]
dice_bonus = DAT_005d2884[item_data_offset]

total = dice_bonus
for i in 0..dice_count:
    total += 1 + rand() % dice_sides

```

#### 8.5 Monster Special Abilities and Status Effects

When a monster attacks a player, special abilities can trigger:

```text
# From FUN_00439fee, after damage is applied:
if monster.special_attack_type != 0:
    chance = monster.special_chance * monster.level
    if rand() % 100 < chance:
        apply_special_effect(monster.special_attack_type, target_player)

```

Special effect application is handled by `FUN_0048dcdc`.

#### 8.6 Hostility and Alliance System (FUN_0040104c)

The hostility check function (469 bytes) determines if two actors are hostile:

```text
Parameters:
    param_1: actor A record pointer
    param_2: actor B record pointer

Returns:
    0 = friendly/neutral (no combat)
    4 = hostile (will attack)

Algorithm:
1. For each actor, check if hostility override timer is active:
   - If actor + 0x168 > 0 (or == 0 and +0x164 != 0): return 4 (always hostile)

2. Read base faction from actor + 0x60 (short):
   - faction_group = (faction - 1) / 3 + 1   (maps sprite IDs to faction groups)

3. Check alliance group (actor + 0x2C4):
   - If both actors have same non-zero alliance group: return 0 (friendly)

4. Check hostility override (actor + 0x2C8):
   - If override > 0: use override as faction
   - If override == 9999: treat as neutral (faction = 0)

5. Check charm/summoned status:
   - If actor + 0x198 timer active: faction = 0 (neutral to all)

6. Check specific conditions:
   - If actor has "hostile to all" flag (bit 19 at +0x24): return 4
   - If target's charm timer active and attacker has hostile flag: return 4

7. Faction matrix lookup:
   - Compare faction_group_A vs faction_group_B
   - Ranges [0x27-0x2C], [0x2D-0x32], [0x33-0x3E], [0x4E-0x53] define alliance blocks
   - Same range = allied (return true from FUN_0043abd3)
   - Same faction = allied
   - Otherwise = hostile

```

---

## 9. Knockback and Stun

### 9.1 Knockback Application

When knockback triggers, the monster receives velocity:

```text
# Knockback vector from param_3 (normalized direction)
knockback_ratio = (damage * 20) / monster.max_hp
if knockback_ratio > 10:
    knockback_ratio = 10

if not monster.is_immovable:  # FUN_00438bce returns 0
    # Scale direction vector by knockback ratio
    param_3[0] = (param_3[0] * knockback_ratio) >> 16
    param_3[1] = (param_3[1] * knockback_ratio) >> 16
    param_3[2] = (param_3[2] * knockback_ratio) >> 16

    # Apply to monster velocity (multiplied by 0x32 = 50)
    monster.velocity_x = param_3[0] * 50   # offset +0x6C
    monster.velocity_y = param_3[1] * 50   # offset +0x6E
    monster.velocity_z = param_3[2] * 50   # offset +0x70

```

#### 9.2 Stun Application

Stun is checked via `FUN_0048ea3e` (Player_CheckStunAbility):

```text
Algorithm:
1. Iterate through equipped items (up to 16 slots)
2. For each valid item:
   - Check if item has stun enchantment (enchant ID 0x215 or effect 0x11)
   - If found: return 50 (0x32) as stun power
   - Check if item has slow enchantment (effect 0x18)
   - If found: return 5 as slow power
3. If no stun items: return 0

```

Stun application (from FUN_00439463):

```text
if (has_stun_ability OR force_stun) AND CheckCritical(monster, resist_type):
    if turn_based_mode:
        stun_duration = 20  (0x14)
    else:
        stun_duration = ftol(scaled_real_time_value)

    monster.recovery_timer += stun_duration
    # recovery_timer at DAT_005ff054 + monster_index * 0xD1

```

#### 9.3 Monster Immovability Check (FUN_00438bce)

Certain monster types cannot be knocked back:

```text
Parameters:
    param_1: monster item/type ID
    param_2: check category (1-7)

Returns: 1 if immovable, 0 if can be knocked back

Category checks (by param_2):
    1: Dragon types (0x46-0x48, 0x5B-0x5D, 0xC7-0xC9, 0xD9-0xDB,
                     0xDF-0xE1, 0xE5-0xE7, 0x100-0x102)
    2: Large creatures (0x16-0x18)
    3: Heavy creatures (0x19-0x1B)
    4: Titans/Giants (0x85-0x96, 0x31-0x33, 0x34-0x36)
    5: Constructs (0x2E-0x30)
    6: Undead bosses (0xFD-0xFF)
    7: Elementals (0xD3-0xD5)

```

---

## 10. Death and Unconscious Handling

### 10.1 Monster Death Sequence

When monster HP drops to 0 or below:

```text
Sequence (from FUN_00439463):
1. Bloodsplat rendering (if hardware rendering enabled):
   - FUN_0049b419 called with monster position
   - Only if monster has blood flag (DAT_005cccf6[monster_type * 0x58] & 1)
   - And rendering flag at (DAT_0071fe94 + 0xE24) bit 5 set

2. Play death animation:
   - FUN_00402d6e (Monster_PlayDeathAnim)
   - Clears condition timers via FUN_004585be
   - Updates sprite state via FUN_004597a6
   - Spawns death projectiles if needed (FUN_0042f7c7 -> FUN_0042f5c9)

3. Award experience and loot:
   - FUN_00438ce2 (Monster_OnDeath)
   - XP formula: (level_bonus + monster_level_byte + difficulty_bonus) * 100
   - XP added to DAT_00ae3060 (party experience accumulator)
   - XP clamped: minimum 0, maximum 4,000,000
   - Kill counter incremented (outdoor or indoor counter)
   - Loot generation triggered

4. Update nearby AI:
   - FUN_0043ac68 (Monster_UpdateAI)
   - Nearby monsters within distance threshold switch to Fleeing state (4)
   - If death was violent (param_2 == 1): set alert flag on nearby monsters

5. Trigger event (if monster has event ID):
   - If monster + 0x4C has event trigger: FUN_0042694b fires event

6. Player voice line:
   - 20% chance (rand() % 100 < 20) of victory voice:
     If monster max_hp > 99: voice = 2 ("big kill")
     Else: voice = 1 ("small kill")
   - Otherwise: voice = 3 (generic)
   - FUN_004948a9(voice, 0) plays character sound

```

#### 10.2 Monster Hit Reaction (Non-Lethal)

When damage is dealt but monster survives:

```text
1. Play hit animation:
   - FUN_004030ad (Monster_PlayHitAnim)
   - Clears condition timers
   - Updates animation state

2. Update AI:
   - FUN_0043ac68 to alert nearby monsters

3. Award partial XP if conditions met:
   - If monster has XP timer active (DAT_005ff1f0 + offset, 64-bit > 0):
     Award proportional XP via FUN_0048dc04

```

#### 10.3 Player Damage Reception (FUN_00439fee)

When a monster damages a player:

```text
1. Determine attack variant:
   - If monster has special weapon skill (byte in range 10-11):
     Select from special voices: 0x69, 0x6A, 0x6B, 0x2D
   - Otherwise:
     Select from normal voices: 0x6C, 0x6D, 0x6E, 0x2C
   - Voice selected randomly (rand() % 4)

2. Calculate monster base damage (FUN_0043b403)

3. Apply damage-splitting buffs:
   - If monster has damage split timer active (offset +0x0E0):
     damage = damage / split_value (at offset +0x0E4)

4. Apply player shields:
   - If player has shield buff active (offset +0x1A0):
     damage = damage / 2

5. Apply resistance based on attack type:
   - Type 0: physical melee resistance
   - Type 1: ranged resistance (halved if shield buff active)
   - Type 2-3: elemental (lookup from spell data table)
   - Type 4: special (use monster's base spell damage)
   - Default: fixed at 4

6. Roll for damage reduction:
   - FUN_00427522 with player's resistance

7. Apply to player HP via FUN_0048dc04:
   - GiveExperience: deducts HP, applies conditions
   - If HP <= 0: unconscious/death check

8. Apply special status effect:
   - If monster has special attack and rand() check passes:
     FUN_0048dcdc applies condition to player

```

#### 10.4 Experience Award (FUN_0048dc04)

```text
Parameters:
    param_1: player record pointer (this)
    param_2: damage amount
    param_3: damage type

Algorithm:
1. Look up stat modifier (FUN_0048eaa6)
2. Call experience/stat handler (FUN_00492d5d)
3. Update player skill progress (FUN_0048d499)
4. Check for condition changes (FUN_00492c03)
5. Play appropriate player voice (FUN_004948a9)

```

---

## 11. Combat Encounter Detection

### FUN_0042f4b6 - CheckForCombatEncounter

```text
Distance thresholds:
    Outdoor: 0x1400 (5,120 units)
    Indoor:  0x0A00 (2,560 units)

Algorithm:
1. For each monster (0 to DAT_006650a8):
   a. Skip if AI state is:
      - Dead (5)
      - Fleeing (4)
      - Stunned (0x0B)
      - Paralyzed (0x13)
      - Special (0x11)
   b. Check hostility (FUN_0040104c)
   c. Compute 3D distance using fast approximation:
      max(|dx|, |dy|, |dz|) + min/4 + mid*11/32
   d. If distance < threshold: combat detected
2. Return whether any hostile monster in range

```

---

## 12. Key Constants and Enumerations

### Damage Element Types

| Index | Element    | Monster Resist Offset |
|-------|------------|----------------------|
| 0     | Fire       | +0x50                |
| 1     | Air        | +0x51                |
| 2     | Water      | +0x52                |
| 3     | Earth      | +0x53                |
| 4     | Spirit     | +0x59                |
| 5     | (unused)   | --                   |
| 6     | Body       | +0x55                |
| 7     | Mind       | +0x54                |
| 8     | Light      | +0x56                |
| 9     | Dark       | +0x57                |
| 10    | Physical   | +0x58                |

### Weapon Skill Types (from item data table at DAT_005d2881)

| Value | Type     | Special Properties                        |
|-------|----------|-------------------------------------------|
| 0     | Blade    | Double attack at Expert                   |
| 1     | Axe      | --                                        |
| 2     | Bow      | Ranged, bonus damage at Expert/Master     |
| 3     | Mace     | --                                        |
| 4     | Unarmed  | Roll 1d3 damage                           |
| 6     | Polearm  | Stun at Expert, Knockback at Master       |
| 12    | Wand     | Special attack formula                    |

### Key Magic Numbers

| Value   | Hex      | Meaning                                    |
|---------|----------|--------------------------------------------|
| 0x1A2   | 418      | Monster short-word stride (836/2)          |
| 0x344   | 836      | Monster record stride (bytes)              |
| 0x1B3C  | 6,972    | Player record stride (bytes)               |
| 0xD1    | 209      | Monster dword stride (836/4)               |
| 0x1E    | 30       | Base resistance threshold                  |
| 0x14    | 20       | Base stun duration (turn-based)            |
| 0x32    | 50       | Knockback velocity multiplier              |
| 200     | 0xC8     | Immunity threshold (resistance >= 200)     |
| 9999    |          | Neutral hostility override (no combat)     |
| 4000000 |          | Maximum party XP accumulator               |

### Debug Flags

| INI Key     | Address    | Effect                                |
|-------------|------------|---------------------------------------|
| `nodamage`  | 0x004ea2b0 | `[debug]` section; disables all damage|

Read via: `GetPrivateProfileIntA("debug", "nodamage", 0, ...)`

---

## 13. Function Map

### Primary Combat Functions

| Address      | Size  | Suggested Name                  | Description                                 |
|--------------|-------|---------------------------------|---------------------------------------------|
| `0x00439463` | 2,483 | `DamageMonsterFromParty`        | Main player-attacks-monster pipeline        |
| `0x00439fee` | 2,483 | `DamagePlayerFromMonster`       | Monster-attacks-player pipeline             |
| `0x0043b1d3` | ~500  | `DamageMonsterFromMonster`      | Monster-on-monster damage resolution        |
| `0x0043b07a` | 345   | `DamageMonsterFromSpellObject`  | Spell object hits monster                   |
| `0x00438f7e` | 346   | `ProcessProjectileCollisions`   | Iterates active projectiles, dispatches hits|

### Damage Calculation

| Address      | Size | Suggested Name                   | Description                                |
|--------------|------|----------------------------------|--------------------------------------------|
| `0x0048cdc1` | ~600 | `Player_CalculateMeleeBaseDamage`| Melee weapon dice roll + bonuses           |
| `0x0048d1e4` | 236  | `Player_CalculateRangedDamage`   | Ranged weapon dice roll + bonuses          |
| `0x0048e189` | ~10  | `Player_GetSpellDamageType`      | Returns spell element type from table      |
| `0x0048e19b` | 853  | `Player_CalculateSpellDamage`    | Full spell damage computation              |
| `0x00439e16` | 472  | `CalculateWeaponEnchantDamage`   | Enchantment bonus damage from weapons      |
| `0x0043b403` | 256  | `Monster_CalculateAttackDamage`  | Monster dice roll for attacks              |

### Hit/Miss and Resistance

| Address      | Size | Suggested Name                   | Description                                |
|--------------|------|----------------------------------|--------------------------------------------|
| `0x004272ac` | ~240 | `CheckHitChance`                 | Player attack vs monster AC                |
| `0x00427372` | 242  | `CheckHitChance_MonsterVsMonster`| Monster vs monster hit check               |
| `0x00427464` | 153  | `CheckHitChance_MonsterVsPlayer` | Monster vs player hit check                |
| `0x00427522` | ~280 | `RollDamage_WithResistance`      | Cascading resistance damage reduction      |
| `0x00427619` | 162  | `CheckCriticalEffect`            | Effect penetration vs resistance           |
| `0x0043b006` | ~100 | `CalculateResistanceReduction`   | Computes resistance-based reduction value  |

### Player Accessors

| Address      | Size | Suggested Name                   | Description                                |
|--------------|------|----------------------------------|--------------------------------------------|
| `0x0048d690` | ~50  | `Player_HasWeaponEquipped`       | Check if weapon slot has valid item        |
| `0x0045827d` | ~100 | `Player_GetWeaponSkillLevel`     | Returns skill level for equipped weapon    |
| `0x0048f87a` | ~400 | `Player_GetSkillMastery`         | Returns mastery level for a skill type     |
| `0x0048d09f` | 107  | `Player_GetAttackBonus`          | Computes total attack ability              |
| `0x0048ea3e` | ~100 | `Player_CheckStunAbility`        | Checks if equipment grants stun           |
| `0x0048dc04` | ~400 | `Player_ApplyDamageToHP`         | Deducts HP, handles unconscious/death      |

### Monster State Management

| Address      | Size | Suggested Name                   | Description                                |
|--------------|------|----------------------------------|--------------------------------------------|
| `0x00438bce` | ~200 | `Monster_IsImmovable`            | Checks if monster type resists knockback   |
| `0x00438ce2` | 322  | `Monster_OnDeath`                | XP award, loot, kill counter               |
| `0x0043ac68` | 239  | `Monster_UpdateNearbyAI`         | Alerts/scatters nearby monsters            |
| `0x00402d6e` | ~200 | `Monster_PlayDeathAnimation`     | Death sprite, condition clear              |
| `0x004030ad` | ~100 | `Monster_PlayHitAnimation`       | Hit reaction animation                     |
| `0x0040104c` | 469  | `CheckHostility`                 | Alliance/hostility between two actors      |
| `0x0043abd3` | 149  | `CheckFactionAlliance`           | Checks if two factions are allied          |

### Combat System Management

| Address      | Size | Suggested Name                   | Description                                |
|--------------|------|----------------------------------|--------------------------------------------|
| `0x0042f4b6` | ~200 | `CheckForCombatEncounter`        | Scans for hostile monsters in range        |
| `0x0042f3b2` | ~100 | `Combat_LoadSounds`              | Loads turn-based combat audio              |
| `0x0042f5c9` | ~100 | `Combat_SpawnProjectile`         | Creates missile/projectile entity          |
| `0x004948a9` | ~100 | `Player_PlayVoiceLine`           | Plays character voice during combat        |

---

## Integration notes

### Architecture Mapping

```cpp
Original (Damage.cpp)              RuneHarbor Suggested Structure
-------------------------------    ------------------------------------------
FUN_00439463                   ->  ICombatResolver::resolvePlayerVsMonster()
FUN_00439fee                   ->  ICombatResolver::resolveMonsterVsPlayer()
FUN_0043b1d3                   ->  ICombatResolver::resolveMonsterVsMonster()
FUN_004272ac                   ->  ICombatCalculator::checkHit()
FUN_00427522                   ->  ICombatCalculator::rollResistance()
FUN_0048cdc1                   ->  IPlayer::calculateMeleeDamage()
FUN_0048d1e4                   ->  IPlayer::calculateRangedDamage()
FUN_0043b006                   ->  IMonster::getResistanceReduction()
FUN_0040104c                   ->  IFactionSystem::checkHostility()

```

### Design Considerations

1. **Separate combat logic from rendering**: The original code mixes bloodsplat spawning,
   animation triggers, and damage calculation in one function. Split these into:
   - `ICombatResolver` - pure damage calculation, returns `CombatResult`
   - `ICombatVisualizer` - handles animations, particles, sound
   - `ICombatEventEmitter` - fires events for XP, death, AI updates

2. **Use `std::expected<CombatResult, CombatError>`** for combat resolution results:

   ```cpp
   struct CombatResult {
       int32_t damage_dealt;
       DamageElement element;
       bool is_critical;
       bool caused_stun;
       bool caused_knockback;
       bool target_killed;
       Vec3i knockback_velocity;
   };

```text

3. **Resistance as a standalone system**: The cascading resistance roll is reusable.
   Implement as a pure function:
   ```cpp
   int32_t applyResistance(int32_t damage, uint8_t resistance,
                           uint16_t buff_bonus, RandomEngine& rng);
   ```

4. **Monster immovability via data, not code**: The `FUN_00438bce` function uses hardcoded
   monster ID ranges. Prefer a data-driven flag in the monster definition table.

5. **Skill mastery encoding**: The original packs mastery level and skill points into a
   single 16-bit value (low 6 bits = points, upper bits = mastery tier). Consider using
   a struct:

   ```cpp
   struct SkillValue {
       uint8_t mastery;  // 0=none, 1=normal, 2=expert, 3=master, 4=GM
       uint8_t points;   // 0-63
   };

```cpp

6. **Dice rolling**: Create a reusable `DiceRoll` struct:
   ```cpp
   struct DiceRoll {
       uint8_t count;
       uint8_t sides;
       uint8_t bonus;
       int32_t roll(RandomEngine& rng) const;
   };
   ```

### Testing Strategy

- Unit test the resistance cascade: verify damage reduction follows the
  `full -> half -> quarter -> eighth -> sixteenth` pattern
- Unit test hit chance: verify the `rand() % (AC + 30 + bonus*2)` distribution
- Integration test: full combat round with mock random engine
- Property test: damage is always >= 0 and <= base_damage for resistance rolls
- Edge cases: resistance == 200 (immunity), resistance == 0, monster HP exactly 0

### File Placement

Following RuneHarbor conventions:

- `src/engine/combat/combat_resolver.hpp` / `.cpp`
- `src/engine/combat/damage_calculator.hpp` / `.cpp`
- `src/engine/combat/resistance.hpp` / `.cpp`
- `src/engine/combat/hit_chance.hpp` / `.cpp`
- `src/engine/combat/faction_system.hpp` / `.cpp`
- `tests/engine/combat/combat_resolver_test.cpp`
- `tests/engine/combat/resistance_test.cpp`
