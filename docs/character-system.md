---
title: "Character System"
summary: "Character records combine identity, statistics, skills, conditions, inventory, and progression."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Character System

Character records combine identity, statistics, skills, conditions, inventory, and progression.

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
2. [Character Struct Layout](#2-character-struct-layout)
3. [Stats System](#3-stats-system)
4. [Skill System](#4-skill-system)
5. [Class System](#5-class-system)
6. [Experience and Leveling](#6-experience-and-leveling)
7. [Conditions](#7-conditions)
8. [Equipment Slots](#8-equipment-slots)
9. [Recovery Time](#9-recovery-time)
10. [Key Functions Reference](#10-key-functions-reference)
11. [Key Global Variables](#11-key-global-variables)

---

## 1. Overview

The party consists of exactly **4 player characters** stored contiguously in memory.
Each character occupies **0x1B3C (7,004) bytes**. The four characters together span
27,888 bytes (0x6CF0), starting at global address `DAT_00acd804`.

The full party state (including position, gold, food, alignment, and other shared
data) is serialized into `party.bin` inside save files, totaling **0x16238 (90,680)
bytes**.

The active character index (0--3) is stored in `DAT_00507a6c`. A secondary selection
index is at `DAT_005061c8`.

Character rotation for display uses a circular ordering array starting from
`activeCharacter - 1`, wrapping at index 3 back to 0.

---

## 2. Character Struct Layout

**Total size: 0x1B3C = 7,004 bytes per character.**

Base address of character `i`: `0x00ACD804 + i * 0x1B3C`

Upper bound (exclusive, used in loop comparisons): `0x00AD44F4`

### 2.1 Struct Overview (first 0xBC bytes)

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00 | 144 | int64[18] | `conditions` | Condition timestamps (see 2.2); offset = index * 8 |
| +0x90 | 16 | -- | (unknown) | 16 bytes between conditions and experience |
| +0xA0 | 8 | i64 | `experience` | Experience points (see 2.3) |
| +0xA8 | 16 | char[16] | `name` | Character name (null-terminated) |
| +0xB8 | 1 | u8 | `sex` | 0 = Male, 1 = Female (may also serve as voice set selector) |
| +0xB9 | 1 | u8 | `classId` | Class index (see section 5); changes in-place on promotion |
| +0xBA | 1 | u8 | `portraitIndex` | Face/portrait index (cycles modulo 20) |
| +0xBB | 1 | -- | (padding) | |
| +0xBC | ... | i16[] | `stats` | Base stats (see 2.4) |

### 2.2 Conditions (Timestamps)

Conditions are stored as **64-bit game-time timestamps** indicating when each
condition was inflicted. A value of 0 (both dwords zero) means the condition is
not active. The condition check function `FUN_0048e9ec` iterates offsets in priority
order from a lookup table at `DAT_004edda0`, checking 18 (0x12) conditions.

Offset formula: `conditionIndex * 8` from the character struct base (+0x00).

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00 | 8 | i64 | `condCursed` | Cursed timestamp (index 0) |
| +0x08 | 8 | i64 | `condWeak` | Weak timestamp (index 1) |
| +0x10 | 8 | i64 | `condAsleep` | Asleep timestamp (index 2) |
| +0x18 | 8 | i64 | `condAfraid` | Afraid timestamp (index 3) |
| +0x20 | 8 | i64 | `condDrunk` | Drunk timestamp (index 4) |
| +0x28 | 8 | i64 | `condInsane` | Insane timestamp (index 5) |
| +0x30 | 8 | i64 | `condPoison1` | Poison weak (index 6) |
| +0x38 | 8 | i64 | `condDisease1` | Disease weak (index 7) |
| +0x40 | 8 | i64 | `condPoison2` | Poison medium (index 8) |
| +0x48 | 8 | i64 | `condDisease2` | Disease medium (index 9) |
| +0x50 | 8 | i64 | `condPoison3` | Poison severe (index 10) |
| +0x58 | 8 | i64 | `condDisease3` | Disease severe (index 11) |
| +0x60 | 8 | i64 | `condParalyzed` | Paralyzed (index 12) |
| +0x68 | 8 | i64 | `condUnconscious` | Unconscious (index 13) |
| +0x70 | 8 | i64 | `condDead` | Dead (index 14) |
| +0x78 | 8 | i64 | `condStoned` | Petrified (index 15) |
| +0x80 | 8 | i64 | `condEradicated` | Eradicated (index 16) |
| +0x88 | 8 | i64 | `condZombie` | Zombie (index 17) |

### 2.3 Experience and Level

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0xA0 | 4 | u32 | `experienceLow` | Experience points (low dword) |
| +0xA4 | 4 | u32 | `experienceHigh` | Experience points (high dword) |
| +0xA8 | 16 | char[16] | `name` | Character name (null-terminated) |
| +0xB8 | 1 | u8 | `sex` | 0 = Male, 1 = Female |
| +0xB9 | 1 | u8 | `classId` | Class ID; changes in-place on promotion |

### 2.4 Base Stats

Base stat values are stored as **int16** (2 bytes each). Each stat has a base
value and a corresponding bonus field. The base getter functions read from these
offsets and add results from the buff/equipment bonus accumulator `FUN_0048eaa6`.

| Offset | Size | Type | Field | Stat |
|--------|------|------|-------|------|
| +0xBC | 2 | i16 | `mightBase` | Might (base) |
| +0xBE | 2 | i16 | `mightBonus` | Might (item/magic bonus) |
| +0xC0 | 2 | i16 | `intellectBase` | Intellect (base) |
| +0xC2 | 2 | i16 | `intellectBonus` | Intellect (item/magic bonus) |
| +0xC4 | 2 | i16 | `personalityBase` | Personality (base) |
| +0xC6 | 2 | i16 | `personalityBonus` | Personality (item/magic bonus) |
| +0xC8 | 2 | i16 | `enduranceBase` | Endurance (base) |
| +0xCA | 2 | i16 | `enduranceBonus` | Endurance (item/magic bonus) |
| +0xCC | 2 | i16 | `speedBase` | Speed (base) |
| +0xCE | 2 | i16 | `speedBonus` | Speed (item/magic bonus) |
| +0xD0 | 2 | i16 | `accuracyBase` | Accuracy (base) |
| +0xD2 | 2 | i16 | `accuracyBonus` | Accuracy (item/magic bonus) |
| +0xD4 | 2 | i16 | `luckBase` | Luck (base) |
| +0xD6 | 2 | i16 | `luckBonus` | Luck (item/magic bonus) |
| +0xD8 | 2 | i16 | `armorClassBonus` | Armor class bonus |
| +0xDA | 2 | i16 | `level` | Current character level |
| +0xDC | 2 | i16 | `levelBonus` | Level bonus (from magic) |
| +0xDE | 2 | i16 | `ageModifier` | Age modifier (added to calculated age) |

### 2.5 Skills

Skills are stored as an array of **uint16** values starting at offset +0x108.
Each skill value encodes both the skill level and mastery tier in its bits
(see section 4 for encoding details).

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x108 | 74 | uint16[37] | `skills[37]` | 37 skill slots, 2 bytes each |

The skill array extends from +0x108 through +0x151.

### 2.6 Active Spells / Buffs

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x164 | varies | -- | `spellBuffs` | Active spell buff timestamps and data |

### 2.7 Equipment and Inventory

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x1F0 | 5,040 | InventoryItem[140] | `inventory` | Item array (140 slots x 36 bytes) |
| +0x1948 | 16 | int32[4] | `equipSlotMainhand..` | Equipment slot indices (into inventory) |
| +0x194C | 4 | i32 | `equipSlotOffhand` | Offhand equipment slot index |
| +0x1950 | 4 | i32 | `equipSlotBow` | Ranged weapon slot index |
| +0x1954 | 4 | i32 | `equipSlotArmor` | Body armor slot index |

Each inventory item is **0x24 (36) bytes**:

```cpp
struct InventoryItem {     // 0x24 = 36 bytes
    int32_t  itemId;       // +0x00: Item type ID (into item table at DAT_005d2864)
    int32_t  enchantment1; // +0x04: Standard enchantment
    int32_t  charges;      // +0x08: Charges remaining / enchantment power
    int32_t  enchantment2; // +0x0C: Special enchantment type
    int32_t  enchantment3; // +0x10: Enchantment data / bonus value
    uint32_t flags;        // +0x14: Bit flags (bit 1 = broken, bit 9 = hardened, etc.)
    int32_t  bodySlot;     // +0x18: Which body slot this item occupies
    int32_t  expireTime;   // +0x1C: Expiry time (for temporary items)
    int32_t  maxCharges;   // +0x20: Maximum charges / additional flags
};

```

Item flags (offset +0x14 within InventoryItem):

- Bit 1 (0x02): Broken / unusable
- Bit 9 (0x200): Hardened / indestructible

### 2.8 Resistances

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x1774 | 16 | int16[8] | `resistanceBase[8]` | Base resistance values |
| +0x178A | 16 | int16[8] | `resistanceBonus[8]` | Resistance bonuses (equipment/magic) |

Resistance indices: 0=Fire, 1=Air, 2=Water, 3=Earth, 4=Mind, 5=unused, 6=unused, 7=Body/Spirit

### 2.9 Hit Points and Spell Points

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x1850 | 4 | i32 | `condRelatedLow` | Condition-related timestamp (low) |
| +0x1854 | 4 | i32 | `condRelatedHigh` | Condition-related timestamp (high) |
| +0x1920 | 4 | i32 | `voiceIdAlt` | Alternative voice/portrait ID |
| +0x193C | 4 | i32 | `currentHP` | Current hit points |
| +0x1940 | 4 | i32 | (unknown) | |
| +0x1944 | 4 | i32 | `birthYear` | Birth year (used for age calculation) |

### 2.10 Misc Stat Modifiers

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x1A92 | 1 | i8 | `meleeDmgBonus` | Flat melee damage bonus |
| +0x1A94 | 1 | i8 | `meleeAttackBonus` | Flat melee attack bonus |
| +0x1A96 | 1 | i8 | `rangedAttackBonus` | Flat ranged attack bonus |
| +0x1A98 | 1 | i8 | `hpBonus` | Flat HP bonus |
| +0x1A9A | 1 | i8 | `spBonus` | Flat SP bonus |

---

## 3. Stats System

### 3.1 The Seven Base Stats

| Index | Stat | Base Offset | Bonus Offset | Base Getter | Bonus Getter |
|-------|------|-------------|--------------|-------------|--------------|
| 0 | Might | +0xBC | +0xBE | `FUN_0048c83b` | `FUN_0048c922` |
| 1 | Intellect | +0xC0 | +0xC2 | `FUN_0048c852` | `FUN_0048c9a8` |
| 2 | Personality | +0xC4 | +0xC6 | `FUN_0048c869` | `FUN_0048ca25` |
| 3 | Endurance | +0xC8 | +0xCA | `FUN_0048c880` | `FUN_0048caa2` |
| 4 | Accuracy | +0xD0 | +0xD2 | `FUN_0048c897` | `FUN_0048cb1f` |
| 5 | Speed | +0xCC | +0xCE | `FUN_0048c8ae` | `FUN_0048cb9c` |
| 6 | Luck | +0xD4 | +0xD6 | `FUN_0048c8c5` | `FUN_0048cc19` |

### 3.2 Base Stat Getter Pattern

Each base stat getter (`FUN_0048c83b` through `FUN_0048c8c5`) follows the same pattern:

```c
int GetStatBase(Player* player, int statIndex) {
    int buffBonus = GetActiveBuffBonus(statIndex, 0);  // FUN_0048eaa6
    return buffBonus + player->statBase[statIndex];     // int16 at offset
}

```

The `FUN_0048eaa6` function is a large (2,938 bytes) buff/equipment bonus accumulator.
It iterates over all equipped items and active spell buffs, summing bonuses for the
requested stat type. The `param_2` argument selects the stat category, and `param_3`
selects base (0) vs. full (1) calculation mode.

### 3.3 Full Stat Bonus Getter Pattern

Each full stat bonus getter (`FUN_0048c922` through `FUN_0048cc19`) calculates the
effective stat value including aging, conditions, and equipment:

```c
int GetStatFull(Player* player, int statIndex) {
    int age = GetAge(player);                    // FUN_0048e6d4
    int agePenalty = LookupAgePenalty(age, player->ageModifier, statIndex);
    int condition = GetWorstCondition(player);   // FUN_0048e9ec
    int condMult = conditionStatTable[condition][statIndex];
    int buffBonus = GetActiveBuffBonus(statIndex, 0);
    int equipBonus = GetEquipmentBonus(statIndex);
    return player->statItemBonus + buffBonus + equipBonus +
           ((player->statBase * agePenalty / 100) * condMult / 100);
}

```

Age penalty lookup uses a threshold table at `DAT_004ede78` (4 age thresholds).
For each stat, a byte array provides the percentage multiplier at each age bracket.

### 3.4 Stat Effect Table

The stat modifier lookup at `DAT_004edea4` maps raw stat values to effect bonuses.
The function `FUN_0048ea13` iterates a table of stat thresholds and returns the
corresponding modifier from `DAT_004edee0`:

```cpp
Stat Value Range -> Modifier
Very Low          -> Large negative
Low               -> Small negative
Average (around 14) -> 0
High              -> Small positive
Very High (50+)   -> Large positive

```

### 3.5 Derived Stats

| Derived Stat | Function | Calculation |
|-------------|----------|-------------|
| Max HP | `FUN_0048e4f0` | `(EnduranceBonus + Level) * classHPMultiplier + buffHP + flatHPBonus + classBaseHP` |
| Max SP | `FUN_0048e55d` | Depends on class: caster classes use Intellect or Personality, hybrid classes use both. `(statBonus + level) * classSPMultiplier + buffSP + flatSPBonus + classBaseSP` |
| Armor Class | `FUN_0048e64e` (base), `FUN_0048e687` (total) | `SpeedBonus + buffAC + equipAC + player->armorClassBonus` |
| Age | `FUN_0048e6d4` | `gameTime / (60*60*24*336) - player->birthYear + 0x490` (0x490 = base year offset, 336 days/year) |
| Resistances | `FUN_0048e7c8` | `player->resistanceBase[i] + player->resistanceBonus[i] + buffResist + equipResist + classBonus` |

### 3.6 HP and SP Class Multiplier Tables

The HP multiplier per class is stored at `DAT_004ed610` (indexed by classId).
The SP multiplier per class is at `DAT_004ed634`.
Base HP per class tier is at `DAT_004ed5f8` (indexed by `classId >> 2`).
Base SP per class tier is at `DAT_004ed604`.

SP stat dependency varies by class (from `FUN_0048e55d` switch):

- Classes 5--7, 0x10--0x13, 0x20--0x23: Use **Intellect** bonus
- Classes 9--0x0F, 0x18--0x1B: Use **Personality** bonus
- Classes 0x15--0x17, 0x1C--0x1F: Use **both Intellect and Personality** bonuses

---

## 4. Skill System

### 4.1 Skill Encoding

Each skill is stored as a **uint16** value. The encoding packs both the skill
level and the mastery tier into a single 16-bit word:

```text
Bits  0-5  (0x003F): Skill level (0-63)
Bits  6-7  (0x00C0): Mastery tier
Bit   8+   (0xFF00): Additional flags / extended mastery

Mastery encoding in upper bits:
  0x00 = None (skill not learned)
  0x01 = Novice      (when skill level > 0 and no mastery bits set)
  0x40 = Expert       (bit 6 set)
  0x80 = Master       (bit 7 set)
  0xC0 = Grand Master (bits 6+7 set)

```

To extract components:

```c
uint16_t skillValue = player->skills[skillIndex];
int level   = skillValue & 0x3F;        // Skill level 0-63
int mastery = (skillValue >> 6) & 0x03; // 0=Novice, 1=Expert, 2=Master, 3=GM

```

The function `FUN_0048f87a` (894 bytes) computes effective skill bonuses,
factoring in mastery tier, class promotions, and NPC party member bonuses.
For example, having certain NPC companions adds flat skill bonuses
(via `FUN_00476399` checks).

### 4.2 Skill List

Skills are indexed 0--36 (37 total). The skill names are loaded from `skilldes.txt`
at `DAT_005c89dc`.

| Index | Skill Name | Category |
|-------|-----------|----------|
| 0 | Staff | Weapon |
| 1 | Sword | Weapon |
| 2 | Dagger | Weapon |
| 3 | Axe | Weapon |
| 4 | Spear | Weapon |
| 5 | Bow | Weapon |
| 6 | Mace | Weapon |
| 7 | Blaster | Weapon |
| 8 | Shield | Armor |
| 9 | Leather | Armor |
| 10 | Chain | Armor |
| 11 | Plate | Armor |
| 12 | Fire | Magic |
| 13 | Air | Magic |
| 14 | Water | Magic |
| 15 | Earth | Magic |
| 16 | Spirit | Magic |
| 17 | Mind | Magic |
| 18 | Body | Magic |
| 19 | Light | Magic |
| 20 | Dark | Magic |
| 21 | Item Identification | Misc |
| 22 | Merchant | Misc |
| 23 | Repair | Misc |
| 24 | Body Building | Misc |
| 25 | Meditation | Misc |
| 26 | Perception | Misc |
| 27 | Diplomacy | Misc |
| 28 | Thievery | Misc |
| 29 | Disarm Trap | Misc |
| 30 | Dodging | Misc |
| 31 | Unarmed | Misc |
| 32 | Monster ID | Misc |
| 33 | Arms Master | Misc |
| 34 | Stealing | Misc |
| 35 | Alchemy | Misc |
| 36 | Learning | Misc |

### 4.3 Skill Mastery Checks

The function `FUN_0045827d` extracts the mastery tier from a skill value and
returns it as an integer (0--4 corresponding to None/Novice/Expert/Master/GM).
This is used extensively in combat to determine weapon-specific abilities:

- **Sword** (skill type 1): Mastery >= 3 (Master) allows double attack roll
- **Spear** (skill type 4): Mastery >= 3 allows stun, Mastery >= 4 (GM) allows knockback
- **Axe** (skill type 3): High mastery allows chance of triple damage on critical

### 4.4 Skill Bonus from Equipment

The `FUN_0048eaa6` accumulator function checks equipped items for skill-boosting
enchantments. Item enchantment type at item offset +0x0C is compared against
skill-related enchantment IDs (e.g., enchantment 0x20 boosts a specific skill
by the value stored in the item's bonus field).

### 4.5 Class Skill Restrictions

Skill availability per class is loaded from `class.txt` (string at `004e8a58`).
The data file defines which skills each class can learn and to what mastery level.
At runtime, the skill check functions validate whether a character's class permits
a given skill before allowing training.

---

## 5. Class System

### 5.1 Base Classes and Promotions

There are **9 base classes**, each with **two promotion tiers** (first and second
promotion, sometimes called "path" promotions). The class ID at offset +0x11 and
the promoted class ID at +0xB9 encode the current class state.

| Base ID | Base Class | 1st Promotion | 2nd Promotion |
|---------|-----------|---------------|---------------|
| 0 | Knight | Cavalier | Champion / Black Knight |
| 1 | Thief | Rogue | Spy / Assassin |
| 2 | Monk | Initiate | Master / Ninja |
| 3 | Paladin | Crusader | Hero / Villain |
| 4 | Archer | Warrior Mage | Master Archer / Sniper |
| 5 | Ranger | Hunter | Ranger Lord / Bounty Hunter |
| 6 | Cleric | Priest | High Priest / Priest of Dark |
| 7 | Druid | Great Druid | Arch Druid / Warlock |
| 8 | Sorcerer | Wizard | Archmage / Lich |

### 5.2 Class ID Encoding

The class ID is a single byte. The base class is `classId >> 2` (integer division
by 4, giving the class tier). Promotions are distinguished by the lower bits and
the value at offset +0xB9:

```text
classId values (observed from code patterns):
  0x00-0x03: Knight tiers
  0x04-0x07: Thief tiers
  0x08-0x0B: Monk tiers
  0x0C-0x0F: Paladin tiers
  0x10-0x13: Archer tiers
  0x14-0x17: Ranger tiers
  0x18-0x1B: Cleric tiers
  0x1C-0x1F: Druid tiers
  0x20-0x23: Sorcerer tiers

```

Each group of 4 IDs represents: base, 1st promotion, 2nd promotion path A,
2nd promotion path B.

### 5.3 Class Icons (Character Creation)

Icons loaded during character creation (`FUN_004968e2`):

| Icon Name | Class |
|-----------|-------|
| `IC_KNIGHT` | Knight |
| `IC_THIEF` | Thief |
| `IC_MONK` | Monk |
| `IC_RANGER` | Ranger |
| `IC_DRUID` | Druid |

(Additional class icons are loaded but not all are named in string references.)

### 5.4 Class Effect on SP Calculation

The SP function (`FUN_0048e55d`) uses a switch on the class byte at +0xB9 to
determine which stat governs spell points:

| Class IDs | SP Stat | Description |
|-----------|---------|-------------|
| 5-7, 0x10-0x13, 0x20-0x23 | Intellect | Sorcerer-line and Archer-line casters |
| 9-0x0F, 0x18-0x1B | Personality | Cleric-line casters |
| 0x15-0x17, 0x1C-0x1F | Intellect + Personality | Druid-line and Ranger-line (both) |
| 0-4, 8 | 0 (none) | Non-caster classes (Knight, Thief, Monk base) |

### 5.5 Class Promotion Effects

Promoted classes receive:

- Access to higher skill mastery tiers
- Higher HP and SP multipliers (from class tables at `DAT_004ed610`/`DAT_004ed634`)
- Better base HP/SP per level (from tables at `DAT_004ed5f8`/`DAT_004ed604`)
- Resistance bonuses (second promotion grants class-specific resistance boost)
- Special abilities (e.g., Lich promotion: classId `0x23` with special cap at 200 for resistances)

The code at `FUN_0048e7c8` checks `player->classPromotion == 0x23` (Lich) and
caps resistance to 200 when that class is active.

---

## 6. Experience and Leveling

### 6.1 Experience Points

Experience is stored as a **64-bit integer** at offsets +0xA0 (low dword) and
+0xA4 (high dword). The function `FUN_0048d440` checks whether the character
has enough XP to level up.

### 6.2 Level-Up XP Requirements

The XP requirement follows a triangular number formula:

```c
bool CanLevelUp(Player* player) {
    int level = player->level;
    int xpNeeded = 0;
    for (int i = 0; i < level; i++) {
        xpNeeded += (i + 1);
    }
    xpNeeded *= 1000;  // Each level costs 1000 * triangle(level)
    return player->experience >= xpNeeded;
}

```

This means:

| Level | XP Required | Cumulative Formula |
|-------|-------------|-------------------|
| 1 | 0 | Start |
| 2 | 1,000 | 1 * 1000 |
| 3 | 3,000 | (1+2) * 1000 |
| 4 | 6,000 | (1+2+3) * 1000 |
| 5 | 10,000 | (1+2+3+4) * 1000 |
| N | N*(N-1)/2 * 1000 | Triangle(N-1) * 1000 |

### 6.3 XP Awards

Experience is granted through:

- Combat kills: `FUN_0048dc04` (Player_GiveExperience) -- called with XP amount and distribution flag
- Event scripts: opcode 0x39 (`EVT_GIVE_EXPERIENCE`)
- Quest completions: via event system

The Learning skill provides an XP multiplier bonus.

### 6.4 Level-Up Effects

On level up, the character gains:

- HP increase based on class HP multiplier and Endurance bonus
- SP increase based on class SP multiplier and relevant stat bonus
- A stat point (or multiple) to distribute (handled by the level-up UI)
- Access to new spell tiers (every few levels for caster classes)

---

## 7. Conditions

### 7.1 Condition Priority

Conditions are checked in **priority order** by `FUN_0048e9ec`. The function
iterates through 18 condition indices using a priority lookup table at
`DAT_004edda0`. The first active condition (non-zero timestamp) is returned
as the "worst" condition, which affects stat calculations and character behavior.

Priority order (highest to lowest severity):

| Priority | Index | Condition | Sound/Effect |
|----------|-------|-----------|--------------|
| 1 | 16 | Eradicated | -- |
| 2 | 15 | Stoned (Petrified) | -- |
| 3 | 14 | Dead | -- |
| 4 | 13 | Unconscious | -- |
| 5 | 12 | Paralyzed | `paralyze` |
| 6 | 10 | Disease (severe) | `disease3` |
| 7 | 9 | Disease (medium) | `disease2` |
| 8 | 8 | Disease (weak) | `disease1` |
| 9 | 7 | Poison (severe) | `poison3` |
| 10 | 6 | Poison (medium) | `poison2` |
| 11 | 5 | Poison (weak) | `poison1` |
| 12 | 4 | Insane | `insane` |
| 13 | 3 | Drunk | `drunk` |
| 14 | 2 | Afraid | `afraid` |
| 15 | 1 | Asleep | `asleep` |
| 16 | 0 | Cursed | -- |
| 17 | 17 | Weak | -- |
| -- | 18 | (none) | Healthy |

### 7.2 Condition Effects on Stats

Each condition applies a percentage multiplier to base stats. The multiplier
table is stored as byte arrays at `DAT_004eddf0` through `DAT_004ede62`, one
array per stat (7 stats), with 19 entries per array (18 conditions + healthy).

For example, being Dead (condition 14) typically sets stat multipliers to 0%,
while Weak might reduce stats to 50%.

### 7.3 Condition Infliction

Conditions are set by writing the current game time (`DAT_00acce64/68`) to the
corresponding condition timestamp field. They are cleared by zeroing both dwords.

The damage function (`FUN_0048dc04`) inflicts conditions when HP drops:

- HP reaches 0: **Unconscious** (condition 0x0D)
- HP drops below -10: **Dead** (condition 0x0E)
- If dead and no protection: equipment may break (flag bit 1 set on item)

### 7.4 Condition Curing

Conditions are cured through:

- Temple healing (NPC services)
- Spells (Cure Disease, Remove Curse, Raise Dead, Resurrection, etc.)
- Event opcode 0x3B (`EVT_CURE_CONDITION`)
- Rest (some minor conditions like Weak clear after resting)

---

## 8. Equipment Slots

### 8.1 Equipment Slot Layout

Equipment slot indices are stored starting at offset +0x1948 in the character
structure. Each slot holds an **int32** index into the character's inventory
array (0 = no item equipped).

| Slot Index | Offset | Slot Name | Item Types |
|-----------|--------|-----------|------------|
| 0 | +0x1948 | Main Hand | Swords, axes, maces, daggers, staves |
| 1 | +0x194C | Off Hand | Shields, daggers (dual-wield) |
| 2 | +0x1950 | Bow / Ranged | Bows, crossbows, blasters |
| 3 | +0x1954 | Armor (Body) | Leather, chain, plate |
| 4 | +0x1958 | Helm | Helmets, hats |
| 5 | +0x195C | Belt | Belts |
| 6 | +0x1960 | Cloak | Cloaks |
| 7 | +0x1964 | Gauntlets | Gauntlets, bracers |
| 8 | +0x1968 | Boots | Boots |
| 9 | +0x196C | Ring 1 | Rings |
| 10 | +0x1970 | Ring 2 | Rings |
| 11 | +0x1974 | Ring 3 | Rings |
| 12 | +0x1978 | Ring 4 | Rings |
| 13 | +0x197C | Ring 5 | Rings |
| 14 | +0x1980 | Ring 6 | Rings |
| 15 | +0x1984 | Amulet | Amulets, necklaces |

The function `FUN_0048d690` checks whether a given equipment slot has a valid,
non-broken item: it returns 1 if the slot index is non-zero and the item's
broken flag (bit 1 at item offset +0x14) is not set.

### 8.2 Item Type Categories

The item type is used to determine which equipment slot an item can occupy.
The item data table (loaded from `stditems.txt`) at `DAT_005d2864` uses a
stride of **0x30 (48) bytes** per item type, with the following key fields:

| Offset in Item Type | Size | Field |
|--------------------|------|-------|
| +0x00 | 1 | Equip type (0=weapon, 1=armor, 2=misc, 3=shield, 4=wand) |
| +0x01 | 1 | Skill required (weapon/armor skill index) |
| +0x02 | 1 | Damage dice count |
| +0x03 | 1 | Damage dice sides |
| +0x04 | 1 | Damage bonus |

### 8.3 Two-Handed vs. One-Handed

The equip type byte determines single/dual-wield behavior:

- Type 0 (one-handed weapon): Can dual-wield with off-hand
- Type 1 (two-handed weapon): Off-hand slot is blocked
- Type 4 (wand): Treated as ranged, uses bow slot logic

The function `FUN_0048d65c` checks dual-wield status by examining both main-hand
and off-hand slot occupancy.

---

## 9. Recovery Time

### 9.1 Attack Recovery

After performing an attack action, a character enters a recovery period during
which they cannot act. The recovery time depends on:

1. **Base recovery**: Weapon speed value from the item type table
2. **Speed stat bonus**: Higher Speed stat reduces recovery
3. **Haste spell**: Reduces recovery time
4. **Arms Master skill**: Reduces recovery at higher mastery levels
5. **Weapon skill mastery**: Higher mastery in the weapon's skill type reduces recovery

The recovery timer is decremented each game tick. When it reaches 0, the
character can act again.

### 9.2 Weapon Speed Values

The weapon speed is read from the item data table (at item type offset in the
global item table). Faster weapons have lower speed values. The recovery formula
combines the weapon's base speed with the character's Speed stat modifier.

### 9.3 Spell Recovery

Spell casting also incurs recovery time. The base recovery for spells depends on
the spell school and level. The Meditation skill can reduce spell recovery time.

### 9.4 Haste Effect

The Haste spell buff (tracked in the character's active buff array) halves the
effective recovery time. When checking recovery, the code tests for an active
Haste buff and applies a divisor.

---

## 10. Key Functions Reference

### 10.1 Stat Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| `0048c83b` | `Player_GetMightBase` | 23 | Returns Might base + buff bonus |
| `0048c852` | `Player_GetIntellectBase` | 23 | Returns Intellect base + buff bonus |
| `0048c869` | `Player_GetPersonalityBase` | 23 | Returns Personality base + buff bonus |
| `0048c880` | `Player_GetEnduranceBase` | 23 | Returns Endurance base + buff bonus |
| `0048c897` | `Player_GetAccuracyBase` | 23 | Returns Accuracy base + buff bonus |
| `0048c8ae` | `Player_GetSpeedBase` | 23 | Returns Speed base + buff bonus |
| `0048c8c5` | `Player_GetLuckBase` | 23 | Returns Luck base + buff bonus |
| `0048c922` | `Player_GetMightFull` | 134 | Full Might with aging/conditions |
| `0048c9a8` | `Player_GetIntellectFull` | 125 | Full Intellect with aging/conditions |
| `0048ca25` | `Player_GetPersonalityFull` | 125 | Full Personality with aging/conditions |
| `0048caa2` | `Player_GetEnduranceFull` | 125 | Full Endurance with aging/conditions |
| `0048cb1f` | `Player_GetAccuracyFull` | 125 | Full Accuracy with aging/conditions |
| `0048cb9c` | `Player_GetSpeedFull` | 125 | Full Speed with aging/conditions |
| `0048cc19` | `Player_GetLuckFull` | 194 | Full Luck with aging/conditions (extra NPC checks) |
| `0048c8dc` | `Player_GetLevelBase` | 23 | Returns level base + buff |
| `0048c8f3` | `Player_GetLevelFull` | 47 | Returns full level with bonuses |

### 10.2 Derived Stat Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| `0048e4f0` | `Player_GetMaxHP` | 109 | Max HP from class, level, endurance |
| `0048e55d` | `Player_GetMaxSP` | 194 | Max SP from class, level, int/pers |
| `0048e64e` | `Player_GetACBase` | 57 | Base armor class |
| `0048e687` | `Player_GetACFull` | 77 | Full AC with equipment |
| `0048e6d4` | `Player_GetAge` | 80 | Calculate age from game time |
| `0048e724` | `Player_GetEffectiveAge` | 19 | Age + age modifier |
| `0048e7c8` | `Player_GetResistance` | 293 | Resistance for element type |
| `0048e9ec` | `Player_GetWorstCondition` | 39 | Check conditions in priority order |
| `0048ea13` | `Player_StatToModifier` | 43 | Convert raw stat to modifier value |

### 10.3 Combat Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| `0048cdc1` | `Player_CalcMeleeBaseDamage` | 734 | Base melee damage from weapon(s) |
| `0048d09f` | `Player_CalcMeleeAttackBonus` | 107 | Melee attack bonus |
| `0048d10a` | `Player_CalcRangedAttackBase` | 109 | Ranged attack base bonus |
| `0048d177` | `Player_CalcRangedDamageBase` | -- | Ranged damage base |
| `0048d1e4` | `Player_CalcRangedDamage` | 236 | Full ranged damage with bonuses |
| `0048d2d0` | `Player_GetMeleeDamageString` | 172 | Format melee damage as "d - d" |
| `0048d37c` | `Player_GetRangedDamageString` | 196 | Format ranged damage as "d - d" |
| `0048d440` | `Player_CanLevelUp` | 62 | Check if XP meets level requirement |
| `0048d499` | `Player_CalcDamageResistance` | 377 | Reduce incoming damage by resistance/luck |
| `0048d690` | `Player_HasEquippedItem` | 38 | Check if equipment slot is valid |
| `0048d752` | `Player_CalcMeleeHitChance` | 287 | Full melee hit calculation |
| `0048d871` | `Player_CalcAttackVsMonster` | 814 | Full attack resolution vs monster |
| `0048dc04` | `Player_TakeDamage` | 216 | Apply damage, check death conditions |
| `0048dcdc` | `Player_ApplySpecialAttack` | 1013 | Apply spell/enchantment effects |

### 10.4 Skill and Buff Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| `0048eaa6` | `Player_AccumulateBuffBonus` | 2938 | Sum all equipment/buff bonuses for stat |
| `0048f734` | `Player_GetEquipmentBonus` | -- | Get bonus from equipped items for stat |
| `0048f87a` | `Player_GetSkillMastery` | 894 | Get effective skill level with mastery |
| `0048fbf8` | `Player_GetActiveBuffBonus` | -- | Get bonus from active spell buffs |
| `0045827d` | `Player_GetWeaponSkillLevel` | -- | Extract mastery tier from weapon skill |

### 10.5 Display and UI Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| `004184ba` | `DrawCharacterStatsScreen` | 3055 | Render the full character stat page |
| `004968e2` | `CharacterCreation_LoadIcons` | -- | Load class icons for creation screen |
| `00456dbe` | `GameTables_LoadAll` | 4893 | Load all game data tables (items, stats, skills) |

---

## 11. Key Global Variables

### 11.1 Party and Character Data

| Address | Type | Description |
|---------|------|-------------|
| `DAT_00acd804` | base | Player 0 start (each player = 0x1B3C bytes) |
| `DAT_00ad44f4` | -- | Upper bound (past Player 3's last byte) |
| `DAT_00507a6c` | i32 | Active character index (0-3) |
| `DAT_005061c8` | i32 | Secondary character index |
| `DAT_00acce64` | u32 | Game time (low dword) |
| `DAT_00acce68` | u32 | Game time (high dword) |

### 11.2 Data Tables

| Address | Type | Description |
|---------|------|-------------|
| `DAT_005d2864` | base | Item type table (0x30 bytes per entry) |
| `DAT_005d2880` | u8[] | Item type -> equip category |
| `DAT_005d2881` | u8[] | Item type -> skill type |
| `DAT_005d2882` | u8[] | Item type -> damage dice count |
| `DAT_005d2883` | u8[] | Item type -> damage dice sides |
| `DAT_005d2884` | u8[] | Item type -> damage bonus |
| `DAT_004ed610` | u8[] | Class -> HP multiplier per level |
| `DAT_004ed634` | u8[] | Class -> SP multiplier per level |
| `DAT_004ed5f8` | u8[] | Class tier -> base HP |
| `DAT_004ed604` | u8[] | Class tier -> base SP |
| `DAT_004edea4` | u16[] | Stat value -> modifier threshold table |
| `DAT_004edee0` | i8[] | Stat threshold -> modifier value |
| `DAT_004ede78` | i32[] | Age thresholds (4 entries) |
| `DAT_004edda0` | i32[] | Condition priority order (18 entries) |
| `DAT_004eddf0` | u8[] | Condition -> Might multiplier table |
| `DAT_004ede03` | u8[] | Condition -> Intellect multiplier table |
| `DAT_004ede16` | u8[] | Condition -> Personality multiplier table |
| `DAT_004ede29` | u8[] | Condition -> Endurance multiplier table |
| `DAT_004ede3c` | u8[] | Condition -> Accuracy multiplier table |
| `DAT_004ede4f` | u8[] | Condition -> Speed multiplier table |
| `DAT_004ede62` | u8[] | Condition -> Luck multiplier table |
| `DAT_005c89dc` | ptr | Skill description text (from `skilldes.txt`) |
| `DAT_00ae3070` | char*[] | Class name string array |
| `DAT_00ae3100` | char*[] | Condition name string array |

### 11.3 Related Source Files

| File | Address | Description |
|------|---------|-------------|
| `Party.cpp` | `004ee5d4` | Party and character data structures |
| `Damage.cpp` | `004e4ab4` | Combat damage calculations |
| `class.txt` | `004e8a58` | Class definition data |
| `stats.txt` | `004e8a64` | Stat tables |
| `skilldes.txt` | `004e8a70` | Skill descriptions |
| `stditems.txt` | `004e8bcc` | Standard item definitions |
| `spcitems.txt` | `004e8bbc` | Special item definitions |
