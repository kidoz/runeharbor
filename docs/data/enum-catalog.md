---
title: "Enum & Constants Catalog"
summary: "This catalog groups identified enumeration values and constants by engine subsystem."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Enum & Constants Catalog

This catalog groups identified enumeration values and constants by engine subsystem.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](../contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Table of Contents

1. [Game States & Modes](#1-game-states-modes)
2. [Character System](#2-character-system)
3. [Skill System](#3-skill-system)
4. [Combat System](#4-combat-system)
5. [Spell System](#5-spell-system)
6. [Item System](#6-item-system)
7. [Monster / Actor System](#7-monster-actor-system)
8. [Event Engine](#8-event-engine)
9. [Map & Geometry](#9-map-geometry)
10. [UI Windows](#10-ui-windows)
11. [Time & Calendar](#11-time-calendar)
12. [INI Configuration](#12-ini-configuration)
13. [Pixel Formats](#13-pixel-formats)
14. [NPC System](#14-npc-system)

---

## 1. Game States & Modes

### GameFlowState

Top-level program flow control. Address: `0x006A0BC4` (uint32).

*Source: [architecture.md](../architecture.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | `FLOW_TITLE` | Title screen / idle |
| 1 | `FLOW_NEW_GAME` | Start new game |
| 2 | `FLOW_CREDITS` | Credits sequence |
| 3 | `FLOW_LOAD_FROM_TITLE` | Load game (from title screen) |
| 4 | `FLOW_GAMEPLAY` | Continue to gameplay |
| 5 | `FLOW_RETURN_TO_TITLE` | Return to title screen |
| 6 | `FLOW_LEVEL_TRANSITION` | Level transition in progress |
| 9 | `FLOW_LOAD_OR_EXIT` | Load game (from in-game) / exit |
| 10 | `FLOW_LOAD_FILE_BROWSER` | Load game via file browser (debug) |

### CurrentScreenMode

Controls which UI screen is displayed. Address: `0x004E28D8` (uint32).
Previous screen mode saved at `0x005067F8`.

*Source: [architecture.md](../architecture.md), [ui-windows.md](../ui-windows.md)*

| Value | Hex | Name | Description |
|-------|-----|------|-------------|
| 0 | 0x00 | `SCREEN_GAMEPLAY` | Normal gameplay (exploration / 3D view) |
| 2 | 0x02 | `SCREEN_OPTIONS` | Game options / console |
| 3 | 0x03 | `SCREEN_MODAL` | Modal dialog |
| 4 | 0x04 | `SCREEN_NPC_DIALOG` | NPC dialogue / interaction popup |
| 5 | 0x05 | `SCREEN_CHEST` | Chest / inventory interaction |
| 10 | 0x0A | `SCREEN_CHAR_CREATE` | Character creation |
| 11 | 0x0B | `SCREEN_SAVE` | Save game screen |
| 12 | 0x0C | `SCREEN_LOAD` | Load game screen |
| 13 | 0x0D | `SCREEN_REST` | Rest screen |
| 16 | 0x10 | `SCREEN_MAP` | Map screen |
| 17 | 0x11 | `SCREEN_SPELLBOOK` | Spell book / cast spell |
| 18 | 0x12 | `SCREEN_QUICK_SPELL` | Quick spell selection |
| 19 | 0x13 | `SCREEN_AUTONOTES` | Autonotes / quest log |
| 21 | 0x15 | `SCREEN_AWARDS` | Awards screen |
| 22 | 0x16 | `SCREEN_SCROLL` | Text / scroll screen |

### GameplaySubState

Controls sub-states within the gameplay loop. Address: `0x006A0BC8` (uint32).

*Source: [architecture.md](../architecture.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | `PLAY_IDLE` | Normal / idle |
| 1 | `PLAY_TRANSITIONING` | Transitioning (break out of inner loop) |
| 2 | `PLAY_LEVEL_LOADED` | Level loaded / entering |
| 3 | `PLAY_SAVING` | Saving |
| 4 | `PLAY_LOADING` | Loading |
| 6 | `PLAY_LEVEL_CHANGE` | Level change |
| 7 | `PLAY_RESET` | Reset / restart |
| 8 | `PLAY_SPECIAL_TRANSITION` | Special transition |
| 9 | `PLAY_EXIT_TO_TITLE` | Exit to title |

### MapType

Address: `0x006BE1E0` (uint32).

*Source: [architecture.md](../architecture.md)*

| Value | Name | Description |
|-------|------|-------------|
| 1 | `MAP_INDOOR` | Indoor map (BLV format) |
| 2 | `MAP_OUTDOOR` | Outdoor map (ODM format) |

### CombatMode

Address: `0x00ACD6B4` (uint32).

*Source: [combat-system.md](../combat-system.md), [monster-ai.md](../monster-ai.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | `COMBAT_REALTIME` | Real-time mode |
| 1 | `COMBAT_TURNBASED` | Turn-based mode |

---

## 2. Character System

### CharacterClass

Stored at character offset +0xB9 (uint8). The class field holds the current class ID,
which changes in-place upon promotion (e.g., Knight → Cavalier).
Each base class occupies 4 IDs: base, 1st promotion, 2nd promotion path A, 2nd promotion path B.

*Source: [character-system.md](../character-system.md)*

| ID Range | Base Class | 1st Promotion | 2nd Promotion A | 2nd Promotion B |
|----------|-----------|---------------|-----------------|-----------------|
| 0x00-0x03 | Knight | Cavalier | Champion | Black Knight |
| 0x04-0x07 | Thief | Rogue | Spy | Assassin |
| 0x08-0x0B | Monk | Initiate | Master | Ninja |
| 0x0C-0x0F | Paladin | Crusader | Hero | Villain |
| 0x10-0x13 | Archer | Warrior Mage | Master Archer | Sniper |
| 0x14-0x17 | Ranger | Hunter | Ranger Lord | Bounty Hunter |
| 0x18-0x1B | Cleric | Priest | High Priest | Priest of Dark |
| 0x1C-0x1F | Druid | Great Druid | Arch Druid | Warlock |
| 0x20-0x23 | Sorcerer | Wizard | Archmage | Lich |

Base class = `classId >> 2`. Special note: class `0x23` (Lich) has resistance cap at 200.

### CharacterSex

Stored at character offset +0xB8 (uint8).

*Source: [character-system.md](../character-system.md)*

| Value | Name |
|-------|------|
| 0 | Male |
| 1 | Female |

### CharacterStat

Seven primary stats, each with base (int16) and bonus (int16) at the given offsets.

*Source: [character-system.md](../character-system.md)*

| Index | Stat | Base Offset | Bonus Offset |
|-------|------|-------------|--------------|
| 0 | Might | +0xBC | +0xBE |
| 1 | Intellect | +0xC0 | +0xC2 |
| 2 | Personality | +0xC4 | +0xC6 |
| 3 | Endurance | +0xC8 | +0xCA |
| 4 | Accuracy | +0xD0 | +0xD2 |
| 5 | Speed | +0xCC | +0xCE |
| 6 | Luck | +0xD4 | +0xD6 |

### ConditionIndex

Stored as 64-bit game-time timestamps. Condition array starts at character offset **+0x00**
(the very beginning of the character struct). 18 conditions × 8 bytes = 144 bytes (+0x00 to +0x8F).
Offset formula: `conditionIndex * 8`. A timestamp of 0 means the condition is not active.
Priority order from highest to lowest severity (priority 1 = most severe).

Verified via Ghidra: `FUN_004908a0` takes condition index as parameter; condition 1 (Weak)
accesses `DAT_00acd80c` = charBase + 0x08; condition 16 (Eradicated) accesses charBase + 0x80.
Poison/Disease are **interleaved by tier** (not grouped): Cure Poison (spell 72) clears
indices 6, 8, 10; Cure Disease (spell 74) clears indices 7, 9, 11.

*Source: [character-system.md](../character-system.md)*

| Index | Condition | Priority | Character Offset |
|-------|-----------|----------|-----------------|
| 0 | Cursed | 16 | +0x00 |
| 1 | Weak | 17 | +0x08 |
| 2 | Asleep | 15 | +0x10 |
| 3 | Afraid | 14 | +0x18 |
| 4 | Drunk | 13 | +0x20 |
| 5 | Insane | 12 | +0x28 |
| 6 | Poison (weak) | 11 | +0x30 |
| 7 | Disease (weak) | 10 | +0x38 |
| 8 | Poison (medium) | 9 | +0x40 |
| 9 | Disease (medium) | 8 | +0x48 |
| 10 | Poison (severe) | 7 | +0x50 |
| 11 | Disease (severe) | 6 | +0x58 |
| 12 | Paralyzed | 5 | +0x60 |
| 13 | Unconscious | 4 | +0x68 |
| 14 | Dead | 3 | +0x70 |
| 15 | Stoned (Petrified) | 2 | +0x78 |
| 16 | Eradicated | 1 | +0x80 |
| 17 | Zombie | 0 | +0x88 |

### EquipmentSlot

Equipment slot indices, stored at character offset +0x1948 onward (int32 each).

*Source: [character-system.md](../character-system.md), [item-system.md](../item-system.md)*

| Slot | Offset | Name | Accepted Types |
|------|--------|------|----------------|
| 0 | +0x1948 | Main Hand | Swords, axes, maces, daggers, staves |
| 1 | +0x194C | Off Hand | Shields, daggers (dual-wield) |
| 2 | +0x1950 | Bow / Ranged | Bows, crossbows, blasters |
| 3 | +0x1954 | Body Armor | Leather, chain, plate |
| 4 | +0x1958 | Helmet | Helmets, hats |
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

---

## 3. Skill System

### SkillIndex

37 skills indexed 0-36. Stored as uint16 array at character offset +0x108.

*Source: [character-system.md](../character-system.md)*

| Index | Skill | Category |
|-------|-------|----------|
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

### SkillMastery

Encoded in the upper bits of the 16-bit skill value. Bits 0-5 = level (0-63).

*Source: [character-system.md](../character-system.md), [spell-system.md](../spell-system.md)*

| Bit Pattern | Mastery | Integer Value |
|-------------|---------|---------------|
| `0x00` | None / Novice | 0 or 1 |
| `0x40` (bit 6) | Expert | 2 |
| `0x80` (bit 7) | Master | 3 |
| `0x100` (bit 8) | Grand Master | 4 |

Extraction:

- Level: `skillValue & 0x3F`
- Mastery: check bits 8, 7, 6 in priority order

---

## 4. Combat System

### AttackType

Determined from projectile/source field at offset +0x48.

*Source: [combat-system.md](../combat-system.md)*

| Value | Hex | Name | Description |
|-------|-----|------|-------------|
| 34 | 0x22 | `ATTACK_AUTO_STUN` | No damage roll; forced stun |
| 39 | 0x27 | `ATTACK_SPELL_DIRECT` | Spell damage via CalculateSpellDamage |
| 100 | 0x64 | `ATTACK_RANGED_SPELL` | Ranged + elemental spell damage |
| 101 | 0x65 | `ATTACK_RANGED_PHYSICAL` | Ranged physical projectile |
| 102 | 0x66 | `ATTACK_MELEE` | Melee weapon damage |

### DamageElement

Element type index used for resistance calculations. The enum order does **not** match
the monster struct memory layout -- `FUN_00427522` uses a switch statement to translate
enum values to byte offsets.

*Source: [combat-system.md](../combat-system.md), [spell-system.md](../spell-system.md)*

| Index | Element | Monster Resist Offset | Memory Order |
|-------|---------|----------------------|--------------|
| 0 | Fire | +0x50 | 0 |
| 1 | Air / Electricity | +0x51 | 1 |
| 2 | Water / Cold | +0x52 | 2 |
| 3 | Earth | +0x53 | 3 |
| 4 | Spirit | +0x59 | 9 |
| 5 | (unused -- no resistance) | -- | -- |
| 6 | Body | +0x55 | 5 |
| 7 | Mind | +0x54 | 4 |
| 8 | Light | +0x56 | 6 |
| 9 | Dark | +0x57 | 7 |
| 10 | Physical | +0x58 | 8 |

**Monster resistance byte layout** (10 contiguous bytes at actor +0x50):
`Fire, Air, Water, Earth, Mind, Body, Light, Dark, Physical, Spirit`

**Character resistance array** (9 int16 entries at character +0x1774):

| Array Index | Resistance | Offset |
|-------------|------------|--------|
| 0 | Fire | +0x1774 |
| 1 | Air | +0x1776 |
| 2 | Water | +0x1778 |
| 3 | Earth | +0x177A |
| 4-6 | (unused) | +0x177C-0x1781 |
| 7 | Mind | +0x1782 |
| 8 | Body | +0x1784 |

**DamageElement-to-MagicSchool mapping** (`FUN_0048d499`):
Elements 0-3 map to magic school IDs 0x0A-0x0D (Fire-Earth).
Element 6 (Body) maps to school 0x21 (Dark). Element 7 (Mind) maps to school 0x0E.
Element 8 (Light) maps to school 0x0F (Body). Elements 4 (Spirit) and 9 (Dark)
have no player resistance mapping and always return 0 from this path.

### WeaponSkillType

From the item data table, field at offset 0x21 (byte).

*Source: [combat-system.md](../combat-system.md), [item-system.md](../item-system.md)*

| Value | Hex | Name | Special Properties |
|-------|-----|------|--------------------|
| 0 | 0x00 | Staff | -- |
| 1 | 0x01 | Sword | Double attack at Master |
| 2 | 0x02 | Dagger | -- |
| 3 | 0x03 | Axe | Triple damage critical at high mastery |
| 4 | 0x04 | Spear | Stun at Expert, Knockback at Master |
| 5 | 0x05 | Bow | Ranged, bonus damage at Expert/Master |
| 6 | 0x06 | Mace | -- |
| 7 | 0x07 | Blaster | Special attack formula |
| 8 | 0x08 | Shield | -- |
| 9 | 0x09 | Leather | -- |
| 10 | 0x0A | Chain | -- |
| 11 | 0x0B | Plate | -- |
| 12 | 0x0C | Wand | Special wand attack formula |

### MonsterAttackSelection

Return value from AI_SelectAttackType (`FUN_00427002`).

*Source: [monster-ai.md](../monster-ai.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | Melee (basic) | Physical close-range attack |
| 1 | Ranged (basic) | Physical ranged projectile |
| 2 | Spell Attack 1 | First special/spell ability |
| 3 | Spell Attack 2 | Second special/spell ability |

### SourceType (Attacker Encoding)

Encoded in bits 0-2 of the attack parameter. Bits 3+ encode the source index.

*Source: [combat-system.md](../combat-system.md)*

| Value | Name | Description |
|-------|------|-------------|
| 2 | Projectile/Object | Missile / spell projectile |
| 3 | Monster (AI Actor) | Monster source |
| 4 | Player Character | Player party member |

### HitCheckMode

Parameter for CheckHitChance (`FUN_004272ac`).

*Source: [combat-system.md](../combat-system.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | Normal | Standard attack roll |
| 2 | Spell Bonus | Spell-modified AC comparison |
| 3 | Double AC | Doubled AC threshold |

### HostilityLevel

Return from hostility check (`FUN_0040104c`).

*Source: [combat-system.md](../combat-system.md), [monster-ai.md](../monster-ai.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | Allied / Friendly | Will not attack |
| 4 | Hostile | Will attack |

### Combat Constants

*Source: [combat-system.md](../combat-system.md)*

| Value | Hex | Name | Description |
|-------|-----|------|-------------|
| 30 | 0x1E | `BASE_RESISTANCE_THRESHOLD` | Base threshold for resistance rolls |
| 20 | 0x14 | `STUN_DURATION_TURNBASED` | Stun ticks in turn-based mode |
| 50 | 0x32 | `KNOCKBACK_VELOCITY_MULT` | Knockback velocity multiplier |
| 200 | 0xC8 | `IMMUNITY_THRESHOLD` | Resistance >= 200 means immune |
| 9999 | -- | `NEUTRAL_HOSTILITY_OVERRIDE` | Force-friendly override value |
| 4000000 | -- | `MAX_PARTY_XP` | Maximum party XP accumulator |

---

## 5. Spell System

### SpellSchool

Parsed from `spells.txt` field 3.

*Source: [spell-system.md](../spell-system.md)*

| Value | School | Spell ID Range | Skill ID |
|-------|--------|----------------|----------|
| 0 | Fire | 1-11 | 12 (0x0C) |
| 1 | Air | 12-22 | 13 (0x0D) |
| 2 | Water | 23-33 | 14 (0x0E) |
| 3 | Earth | 34-44 | 15 (0x0F) |
| 4 | (default) | -- | -- |
| 5 | Magic | -- | -- |
| 6 | Spirit | 45-55 | 16 (0x10) |
| 7 | Mind | 56-66 | 17 (0x11) |
| 8 | Body | 67-77 | 18 (0x12) |
| 9 | Light | 78-88 | 19 (0x13) |
| 10 | Dark | 89-99 | 20 (0x14) |

Spell ID 100 maps to skill ID 5 (Blaster / Special).

### SpellTargetFlags

Field 10 of `spells.txt`, parsed as character flags.

*Source: [spell-system.md](../spell-system.md)*

| Char | Bit | Hex | Name | Description |
|------|-----|-----|------|-------------|
| `m` | 0 | 0x01 | `TARGET_MONSTERS` | Targets monsters |
| `e` | 1 | 0x02 | `TARGET_ENVIRONMENT` | Targets environment/objects |
| `c` | 2 | 0x04 | `TARGET_CASTER` | Targets caster/party |
| `x` | 3 | 0x08 | `TARGET_SPECIAL` | Special targeting mode |

### SpellDamageType (Extended)

Observed damage type IDs from spell resistance functions.

*Source: [spell-system.md](../spell-system.md)*

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

### MasteryLevel

Used by the spell system and skill checks.

*Source: [spell-system.md](../spell-system.md)*

| Value | Name |
|-------|------|
| 1 | Novice |
| 2 | Expert |
| 3 | Master |
| 4 | Grandmaster |

---

## 6. Item System

### EquipType

Parsed from `items.txt` "equip type" column, field at item description offset 0x20.

*Source: [item-system.md](../item-system.md)*

| Value | Hex | String | Category |
|-------|-----|--------|----------|
| 0 | 0x00 | `weapon` | One-handed weapon |
| 1 | 0x01 | `weapon2` | Two-handed weapon |
| 2 | 0x02 | `missile` | Ranged weapon (bow, crossbow) |
| 3 | 0x03 | `armor` | Body armor |
| 4 | 0x04 | `shield` | Shield |
| 5 | 0x05 | (helm) | Helmet |
| 6 | 0x06 | (belt) | Belt |
| 7 | 0x07 | `cloak` | Cloak |
| 8 | 0x08 | `gauntlets` | Gauntlets |
| 9 | 0x09 | `boots` | Boots |
| 10 | 0x0A | (ring) | Ring |
| 11 | 0x0B | `amulet` | Amulet |
| 12 | 0x0C | `weaponw` | Wand (uses charges) |
| 13 | 0x0D | `reagent` | Alchemy reagent / herb |
| 14 | 0x0E | `bottle` | Potion bottle |
| 15 | 0x0F | `sscroll` | Spell scroll (single use) |
| 16 | 0x10 | (book) | Spell book |
| 17 | 0x11 | `mscroll` | Message scroll (readable) |
| 18 | 0x12 | (deed) | Deed / document |
| 19 | 0x13 | (goldItem) | Gold pile item |
| 20 | 0x14 | (none) | Non-equippable / quest item |

### ItemClass

Field at item description offset 0x25.

*Source: [item-system.md](../item-system.md)*

| Value | String | Description |
|-------|--------|-------------|
| 0 | (normal) | Standard item, randomly generated |
| 1 | `artifact` | Artifact -- unique, one per game, fixed special enchantment |
| 2 | `relic` | Relic -- powerful unique item |
| 3 | `special` | Special item -- specific enchantment |

### ItemFlags

Bitfield at item instance offset +0x14 (uint32).

*Source: [item-system.md](../item-system.md)*

| Bit | Hex | Name | Description |
|-----|-----|------|-------------|
| 0 | 0x01 | `IDENTIFIED` | Item has been identified |
| 1 | 0x02 | `BROKEN` | Item is broken (needs repair) |
| 2 | 0x04 | `CURSED` | Item is cursed (cannot unequip) |
| 3 | 0x08 | `TEMP_BONUS` | Has temporary enchantment |
| 4 | 0x10 | `AURA` | Visible aura effect |
| 5 | 0x20 | `STOLEN` | Item was stolen |
| 6 | 0x40 | `HARDENED` | Extra durability |
| 7 | 0x80 | `QUEST_BIT` | Quest item tracking |

### SpecialAttackEffect

Condition effects that monsters can inflict on hit via `FUN_0048dcdc`.

*Source: [item-system.md](../item-system.md), [monster-ai.md](../monster-ai.md)*

| Value | Keyword | Effect |
|-------|---------|--------|
| 1 | `curse` | Applies curse condition |
| 2 | (weak) | Applies weakness |
| 3 | `asleep` | Applies sleep |
| 4 | `afraid` | Applies fear |
| 5 | `drunk` | Applies drunk |
| 6 | `insane` | Applies insanity |
| 7 | `poison1` | Poison level 1 |
| 8 | `poison2` | Poison level 2 |
| 9 | `poison3` | Poison level 3 |
| 10 | `disease1` | Disease level 1 |
| 11 | `disease2` | Disease level 2 |
| 12 | `disease3` | Disease level 3 |
| 13 | `paralyze` | Paralysis |
| 14 | `uncon` | Unconsciousness |
| 15 | (dead) | Death |
| 16 | `stone` | Petrification |
| 17 | `errad` | Eradication |
| 18 | `brkitem` | Breaks a random item |
| 19 | `brkarmor` | Breaks equipped armor |
| 20 | `brkweapon` | Breaks equipped weapon |
| 21 | `steal` | Steals an item |
| 22 | (age) | Applies aging |
| 23 | `drainsp` | Drains spell points |

### Gold Item IDs (Treasure Generation)

*Source: [item-system.md](../item-system.md)*

| Treasure Level | Gold Item ID | Gold Range |
|----------------|-------------|------------|
| 1 | 197 (0xC5) | 50 + rand() % 51 |
| 2 | 197 (0xC5) | 100 + rand() % 101 |
| 3 | 198 (0xC6) | 200 + rand() % 301 |
| 4 | 198 (0xC6) | 500 + rand() % 501 |
| 5 | 199 (0xC7) | 1000 + rand() % 1001 |
| 6 | 199 (0xC7) | 2000 + rand() % 3001 |

### Item System Constants

*Source: [item-system.md](../item-system.md)*

| Value | Description |
|-------|-------------|
| 0x24 (36) | Item instance struct size (bytes) |
| 0x30 (48) | Item description table entry stride (bytes) |
| 800 | Maximum item types in description table |
| 138 | Inventory slots per character |
| 500-528 | Artifact item ID range (29 artifacts) |
| 13 | Maximum simultaneous artifacts |
| 24 | Standard enchantment count |
| 72 | Special enchantment count |

---

## 7. Monster / Actor System

### AIState

Stored at actor offset +0x88 (uint16).

*Source: [monster-ai.md](../monster-ai.md), [combat-system.md](../combat-system.md)*

| Value | Hex | Name | Description |
|-------|-----|------|-------------|
| 0 | 0x00 | `AI_STANDING` | Default idle, not engaged |
| 1 | 0x01 | `AI_WANDERING` | Moving randomly within area |
| 2 | 0x02 | `AI_GUARDING` | Patrolling or guarding position |
| 3 | 0x03 | `AI_FIDGETING` | Playing idle animation variation |
| 4 | 0x04 | `AI_FLEEING` | Running away from threat |
| 5 | 0x05 | `AI_DEAD` | Killed, excluded from processing |
| 6 | 0x06 | `AI_PURSUING` | Chasing target to close distance |
| 7 | 0x07 | `AI_ATTACKING` | Melee attack animation |
| 8 | 0x08 | `AI_ATTACKING_RANGED` | Ranged/spell attack |
| 9 | 0x09 | `AI_ATTACKING_MELEE2` | Alternate melee attack |
| 11 | 0x0B | `AI_STUNNED` | Temporarily incapacitated |
| 12 | 0x0C | `AI_CASTING_SPELL1` | Casting first spell ability |
| 13 | 0x0D | `AI_CASTING_SPELL2` | Casting second spell ability |
| 17 | 0x11 | `AI_PARALYZED` | Held by paralysis; returns to Standing on expire |
| 18 | 0x12 | `AI_CASTING_SPELL3` | Casting third spell ability |
| 19 | 0x13 | `AI_STONED` | Petrified; fully immobile |

### AIPersonality

Stored at actor offset +0x13 (uint8).

*Source: [monster-ai.md](../monster-ai.md)*

| Value | Name | Description |
|-------|------|-------------|
| 0 | Normal | Standard balanced behavior |
| 1 | Wimp | More likely to flee when HP is low |
| 2 | Aggressive | Prefers close combat, lower flee threshold |
| 3 | Suicidal | Never flees (unused?) |
| 4 | Additional Hostile | Special hostility rules |
| 5 | Fleeing | Special flee-on-spawn type |

### HostilityAggressionRange

Indexed by hostility level at `DAT_004df380`.

*Source: [monster-ai.md](../monster-ai.md)*

| Hostility Level | Range (units) | Hex |
|-----------------|---------------|-----|
| 0 | 0 | 0x0000 |
| 1 | 1,024 | 0x0400 |
| 2 | 2,560 | 0x0A00 |
| 3 | 5,120 | 0x1400 |
| 4 | 10,240 | 0x2800 |

### TargetID Encoding

Packed format for target references.

*Source: [monster-ai.md](../monster-ai.md)*

| Bits 0-2 | Target Type |
|----------|-------------|
| 3 | Another actor (index in bits 3+) |
| 4 | Party/player |
| 0 | No target |

### Actor Flags

Bitfield at actor offset +0x00 (uint32) and +0x24 (uint32 extended).

*Source: [monster-ai.md](../monster-ai.md)*

| Bit | Field | Description |
|-----|-------|-------------|
| 2 | flags byte+1 | Active/rendered |
| 6 | flags byte+1 | Visible to party |
| 15 | flags | Hostile-flagged |
| 19 | flagsExtended | Always-hostile (permanently hostile to party group) |
| 24 | flags | Combat-engaged |

### Monster Record Strides

*Source: [combat-system.md](../combat-system.md), [monster-ai.md](../monster-ai.md)*

| Stride | Value | Unit |
|--------|-------|------|
| Byte stride | 0x344 (836) | bytes per actor |
| Word stride | 0x1A2 (418) | uint16 per actor |
| DWord stride | 0xD1 (209) | uint32 per actor |

### Combat Encounter Distance Thresholds

*Source: [monster-ai.md](../monster-ai.md)*

| Map Type | Distance | Hex |
|----------|----------|-----|
| Outdoor | 5,120 | 0x1400 |
| Indoor | 2,560 | 0x0A00 |

### Visibility Range Limits

*Source: [monster-ai.md](../monster-ai.md)*

| Map Type | Max Range | Hex |
|----------|-----------|-----|
| Outdoor | 5,631 | 0x15FF |
| Indoor | 10,239 | 0x27FF |

---

## 8. Event Engine

### EventOpcode

Bytecode opcodes for the EVT scripting engine. Command at offset +4 in each command.

*Source: [event-engine.md](../event-engine.md)*

| Op | Hex | Name | Description |
|----|-----|------|-------------|
| 1 | 0x01 | `EVT_EXIT` | Exit event processing immediately |
| 2 | 0x02 | `EVT_NPC_DIALOG` | Open NPC dialog window |
| 3 | 0x03 | `EVT_PLAY_SOUND` | Play sound effect |
| 4 | 0x04 | `EVT_SKIP_NEXT` | Decrement sub-event counter |
| 5 | 0x05 | `EVT_SKIP_NEXT2` | Same as opcode 4 |
| 6 | 0x06 | `EVT_TELEPORT` | Teleport within map or map transition |
| 7 | 0x07 | `EVT_GIVE_GOLD` | Give gold to party |
| 8 | 0x08 | `EVT_SET_PLAYER_VAR` | Set a player stat/variable |
| 9 | 0x09 | `EVT_GIVE_ITEM` | Give item to player |
| 10 | 0x0A | `EVT_SET_FLAG` | Set global flag |
| 11 | 0x0B | `EVT_CHECK_FLAG` | Check global flag; conditional branch |
| 12 | 0x0C | `EVT_CHANGE_MAP` | Full map change with transition |
| 13 | 0x0D | `EVT_MODIFY_OBJECT` | Modify map object properties |
| 14 | 0x0E | `EVT_CHECK_CONDITION` | Check player stat; branch on result |
| 15 | 0x0F | `EVT_DOOR_CONTROL` | Open/close/toggle door |
| 16 | 0x10 | `EVT_ADD_STAT` | Add value to player stat |
| 17 | 0x11 | `EVT_REMOVE_ITEM` | Remove item from player inventory |
| 18 | 0x12 | `EVT_SUBTRACT_STAT` | Subtract value from player stat |
| 19 | 0x13 | `EVT_MODIFY_NPC` | Modify NPC properties (6 params) |
| 21 | 0x15 | `EVT_MODIFY_NPC_EX` | Extended NPC modification (7 params) |
| 22 | 0x16 | `EVT_SHOW_BUILDING` | Show shop/building/guild UI |
| 23 | 0x17 | `EVT_SHOW_EFFECT` | Show visual effect |
| 24 | 0x18 | `EVT_PLAY_ANIMATION` | Play animation sequence |
| 25 | 0x19 | `EVT_RANDOM_GOTO` | Jump to random sub-event |
| 26 | 0x1A | `EVT_SHOW_TEXT` | Display text from string table |
| 29 | 0x1D | `EVT_SET_NPC_PORTRAIT` | Set NPC portrait |
| 30 | 0x1E | `EVT_SET_NPC_NAME` | Set NPC name |
| 32 | 0x20 | `EVT_GIVE_AWARD` | Give award/achievement |
| 33 | 0x21 | `EVT_STATUS_MESSAGE` | Display status bar message |
| 34 | 0x22 | `EVT_SPAWN_ITEM` | Spawn item on map |
| 35 | 0x23 | `EVT_SET_PLAYER_SELECT` | Set active player selection mode |
| 36 | 0x24 | `EVT_JUMP_TO_EVENT` | Jump to different sub-event |
| 39 | 0x27 | `EVT_SET_GLOBAL_VAR` | Set global variable / QBit |
| 40 | 0x28 | `EVT_SET_GLOBAL_VAR2` | Set global variable (alternate) |
| 41 | 0x29 | `EVT_CAST_SPELL` | Cast spell on party |
| 42 | 0x2A | `EVT_MODIFY_DECORATION` | Modify map decoration |
| 43 | 0x2B | `EVT_CHECK_SKILL` | Check player skill + mastery |
| 47 | 0x2F | `EVT_SET_MONSTER_TOPIC` | Set monster dialog topic |
| 48 | 0x30 | `EVT_SET_MONSTER_FIELD` | Set arbitrary monster field |
| 49 | 0x31 | `EVT_SET_MONSTER_HOSTILE` | Set monster hostility by group |
| 50 | 0x32 | `EVT_SET_MONSTER_GROUP` | Set monster group AI state |
| 51 | 0x33 | `EVT_CHECK_MAP_VAR` | Check map-specific variable |
| 54 | 0x36 | `EVT_REPLACE_MONSTER` | Replace all monsters of one type |
| 55 | 0x37 | `EVT_SET_MONSTER_AI` | Set AI behavior by monster type |
| 56 | 0x38 | `EVT_CHECK_TIME` | Check game time condition |
| 57 | 0x39 | `EVT_GIVE_EXPERIENCE` | Give XP to player |
| 58 | 0x3A | `EVT_TAKE_GOLD` | Remove gold from party |
| 59 | 0x3B | `EVT_CURE_CONDITION` | Cure player condition |
| 60 | 0x3C | `EVT_SET_HOSTILE_BY_IDX` | Set hostility by monster index |

**Unused opcodes (no-ops):** 20 (0x14), 27 (0x1B), 28 (0x1C), 37 (0x25), 38 (0x26), 44-46 (0x2C-0x2E), 52-53 (0x34-0x35).

### PlayerSelectionMode

Set by opcode 0x23, used by opcodes that affect players.

*Source: [event-engine.md](../event-engine.md)*

| Mode | Name | Behavior |
|------|------|----------|
| 0 | Player 0 | First party member |
| 1 | Player 1 | Second party member |
| 2 | Player 2 | Third party member |
| 3 | Player 3 | Fourth party member |
| 4 | Active Player | Currently selected player |
| 5 | All Players | Loop through all 4 |
| 6 | Random Player | `rand() % 4` |

### EventTriggerType

First command byte that determines how the event is triggered.

*Source: [event-engine.md](../event-engine.md)*

| Value | Hex | Name | Description |
|-------|-----|------|-------------|
| 0x01 | 0x01 | `TRIGGER_INTERACTION` | Player interacts with object/NPC/face |
| 0x03 | 0x03 | `TRIGGER_AMBIENT_SOUND` | Ambient/looping sound at position |
| 0x05 | 0x05 | `TRIGGER_ON_MAP_LOAD` | Fires once when map loads |
| 0x1F | 0x1F | `TRIGGER_TIMER_ABSOLUTE` | Fire at specific absolute game time |
| 0x25 | 0x25 | `TRIGGER_ON_MAP_ENTER` | Fire immediately on map entry |
| 0x26 | 0x26 | `TRIGGER_TIMER_PERIODIC` | Fire periodically at intervals |

### EventScope

Context variable `DAT_005c32a0`.

*Source: [event-engine.md](../event-engine.md)*

| Value | Scope | Buffer | Index |
|-------|-------|--------|-------|
| 0 | Map events | `DAT_005b33a0` | `DAT_005b6458` |
| 1 | Global events | `DAT_005a53b8` | `DAT_00598570` |

---

## 9. Map & Geometry

### FaceAttributeFlags

Bitfield at BLV face offset 0x2C (uint16).

*Source: [blv-indoor-maps.md](../blv-indoor-maps.md)*

| Bit | Hex | Name | Description |
|-----|-----|------|-------------|
| 0 | 0x0001 | `FACE_PORTAL` | Is portal (sector boundary) |
| 1 | 0x0002 | (unknown) | -- |
| 2 | 0x0004 | `FACE_VISIBLE_ON_MAP` | Visible on automap |
| 3 | 0x0008 | `FACE_PROJECTILE_PASSABLE` | Projectiles pass through |
| 4 | 0x0010 | `FACE_SCROLLED_TEXTURE` | Scrolling texture |
| 5 | 0x0020 | `FACE_ALT_LIGHTING` | Alternate lighting mode |
| 6 | 0x0040 | `FACE_TEXTURE_TYPE` | Texture type flag |
| 13 | 0x2000 | `FACE_CLICKABLE` | Interactive / clickable |
| 14 | 0x4000 | `FACE_PRESSURE_PLATE` | Pressure plate trigger |

### BLV Section Sizes

*Source: [blv-indoor-maps.md](../blv-indoor-maps.md)*

| Section | Record Size | Description |
|---------|-------------|-------------|
| Header | 136 bytes (0x88) | Fixed header block |
| Vertex | 6 bytes | int16 X, Y, Z |
| Face | 96 bytes (0x60) | Geometry + attributes |
| Face Extra | 36 bytes (0x24) | Additional face data |
| Sector | 116 bytes (0x74) | Room definition |
| Decoration | 32 bytes (0x20) | 3D prop |
| Light | 16 bytes | Light source |
| BSP Node | 8 bytes | Binary space partition |
| Map Outline | 12 bytes | Automap line segment |

---

## 10. UI Windows

### WindowType

Field at window offset 0x08 (int32).

*Source: [ui-windows.md](../ui-windows.md)*

| ID | Hex | Name | Description |
|----|-----|------|-------------|
| 1 | 0x01 | Main Game View | Primary 3D viewport |
| 4 | 0x04 | Dialogue | NPC dialogue / conversation |
| 6 | 0x06 | Status Bar | Bottom status text |
| 9 | 0x09 | Special Handler | Parameter-driven special window |
| 10 | 0x0A | Shop/NPC | Shop interface, NPC interaction |
| 12 | 0x0C | Inventory | Character inventory / equipment |
| 16 | 0x10 | Character Screen | Stats and skills display |
| 18 | 0x12 | Spellbook | Spell selection and casting |
| 20 | 0x14 | Map | Automap / world map |
| 26 | 0x1A | Rest Screen | Rest/camp interface |
| 27 | 0x1B | Chest/Container | Chest contents view |
| 31 | 0x1F | Parameter-Driven | Generic window using userData |
| 70 | 0x46 | Title Screen | Main menu overlay |
| 90 | 0x5A | Credits | Credits scroll display |

### UIButtonMessageID

Button action IDs used in the event queue.

*Source: [architecture.md](../architecture.md), [save-load.md](../save-load.md)*

| ID (hex) | Name | Action |
|----------|------|--------|
| 0x36 | `MSG_NEW_GAME` | New Game (title) |
| 0x37 | `MSG_LOAD_GAME` | Load Game (title) |
| 0x38 | `MSG_CREDITS` | Credits (title) |
| 0x39 | `MSG_EXIT_GAME` | Exit Game (title) |
| 0x71 | `MSG_NPC_DIALOG` | NPC dialogue option |
| 0x88 | `MSG_SPELL_SELECT` | Spell/ability selection |
| 0xA2 | `MSG_SCROLL_UP` | Scroll up (save/load list) |
| 0xA3 | `MSG_SCROLL_DOWN` | Scroll down (save/load list) |
| 0xA4 | `MSG_CONFIRM` | Confirm (Load/Save action) |
| 0xA5 | `MSG_SELECT_SLOT` | Select save slot (param = index 0-6) |
| 0xA6 | `MSG_CANCEL` | Cancel |
| 0xA7 | `MSG_DELETE_SAVE` | Delete save |

---

## 11. Time & Calendar

### Calendar Constants

*Source: [time-calendar.md](../time-calendar.md), [event-engine.md](../event-engine.md)*

| Constant | Value | Description |
|----------|-------|-------------|
| `TICKS_PER_SECOND` | 128 | Game ticks per real-time second |
| `TICKS_PER_MINUTE` | 7,680 | 128 * 60 |
| `TICKS_PER_HOUR` | 460,800 | 128 * 3,600 |
| `TICKS_PER_DAY` | 11,059,200 | 128 * 86,400 |
| `SECONDS_PER_MINUTE` | 60 (0x3C) | -- |
| `SECONDS_PER_HOUR` | 3,600 (0xE10) | -- |
| `SECONDS_PER_DAY` | 86,400 (0x15180) | -- |
| `SECONDS_PER_WEEK` | 604,800 (0x93A80) | 7 days |
| `SECONDS_PER_MONTH` | 2,419,200 (0x24EA00) | 28 days |
| `DAYS_PER_WEEK` | 7 | -- |
| `DAYS_PER_MONTH` | 28 | All months are 28 days |
| `MONTHS_PER_YEAR` | 12 | -- |
| `DAYS_PER_YEAR` | 336 | 28 * 12 |
| `BASE_YEAR` | 1168 (0x490) | Added to computed year count |

### DayOfWeek

*Source: [time-calendar.md](../time-calendar.md)*

| Index | Name |
|-------|------|
| 0 | Sunday |
| 1 | Monday |
| 2 | Tuesday |
| 3 | Wednesday |
| 4 | Thursday |
| 5 | Friday |
| 6 | Saturday |

### MonthName

*Source: [time-calendar.md](../time-calendar.md)*

| Index | Name |
|-------|------|
| 0 | January |
| 1 | February |
| 2 | March |
| 3 | April |
| 4 | May |
| 5 | June |
| 6 | July |
| 7 | August |
| 8 | September |
| 9 | October |
| 10 | November |
| 11 | December |

---

## 12. INI Configuration

### EngineFlagsBits

Bitfield at `DAT_006BE1E4`.

*Source: [ini-configuration.md](../ini-configuration.md), [architecture.md](../architecture.md)*

| Bit | Hex | INI Key / CLI Flag | Effect |
|-----|-----|-------------------|--------|
| 2 | 0x04 | `nointro` | Skip intro videos |
| 3 | 0x08 | `nologo` | Skip logo videos |
| 4 | 0x10 | `nosound` / `-nosound` | Disable all sound |
| 5 | 0x20 | `nowalksound` | Disable walk sounds |
| 6 | 0x40 | `-noanim` | Disable animations |

### DebugFlagsBits

Bitfield at `DAT_006BE1E8`.

*Source: [ini-configuration.md](../ini-configuration.md)*

| Bit | Hex | INI Key | Effect |
|-----|-----|---------|--------|
| 0 | 0x01 | `startinwindow` | Windowed mode |
| 1 | 0x02 | `showFR` | Frame rate display |
| 2 | 0x04 | `nomonster` | No monster spawning |
| 3 | 0x08 | `nodecoration` | No decoration rendering |
| 4 | 0x10 | `nodamage` | No damage to party |

### CommandLineFlags

*Source: [architecture.md](../architecture.md), [ini-configuration.md](../ini-configuration.md)*

| Flag | Effect |
|------|--------|
| `-usedefs` | Load text data tables instead of binary |
| `-window` | Force windowed mode |
| `-nosound` | Disable all sound (engine flag 0x10) |
| `-noanim` | Disable animations (engine flag 0x40) |

### DefaultSkyColors

*Source: [ini-configuration.md](../ini-configuration.md), [time-calendar.md](../time-calendar.md)*

| Setting | R | G | B |
|---------|---|---|---|
| Day sky top | 81 | 121 | 236 |
| Day sky bottom | 153 | 193 | 237 |
| Night sky top | 0 | 0 | 0 |
| Night sky bottom | 11 | 41 | 129 |

### DefaultValues

*Source: [ini-configuration.md](../ini-configuration.md)*

| Key | Default | Description |
|-----|---------|-------------|
| `walkspeed` | 384 (0x180) | Party movement speed |
| `GammaPos` | 4 | Gamma correction slider |
| `mixerchannels` | 16 | Audio mixer channels |
| `gridband1` | 10 | Near terrain LOD distance |
| `gridband2` | 15 | Medium terrain LOD distance |
| `gridband3` | 25 | Far terrain LOD distance |
| `startmap` | `out01.odm` | Starting map filename |

---

## 13. Pixel Formats

### PixelFormatDetection

Detected from DirectDraw surface RGB bit masks.

*Source: [architecture.md](../architecture.md)*

| R Mask | G Mask | B Mask | Format | Internal Code |
|--------|--------|--------|--------|---------------|
| 0xF800 | 0x07E0 | 0x001F | RGB565 (16-bit) | 0xC0000000 |
| 0x7C00 | 0x03E0 | 0x001F | RGB555 (15-bit) | 0x80000000 |
| (bpp == 8) | -- | -- | 8-bit paletted | (separate path) |

---

## 14. NPC System

### NPC System Limits

*Source: [npc-dialogue.md](../npc-dialogue.md)*

| Constant | Value | Description |
|----------|-------|-------------|
| Max NPCs | 500 (0x1F5) | Total NPC entry slots |
| Max Names | 540 (0x21C) | Name pool entries |
| Max Greetings | 205 (0xCD) | Greeting text pairs |
| Professions | 59 (0x3B) | Profession types |
| Distribution Areas | 78 (0x4E) | Distribution table columns |
| Groups | 51 (0x33) | NPC group assignments |
| News Entries | 51 (0x33) | News text entries |

### NPC Record Sizes

*Source: [npc-dialogue.md](../npc-dialogue.md)*

| Record | Size (bytes) |
|--------|-------------|
| NPC Entry | 76 |
| Name Entry | 8 |
| Greeting Entry | 8 |
| Profession Entry | 20 |

### NPC Save Data Sizes

*Source: [npc-dialogue.md](../npc-dialogue.md), [save-load.md](../save-load.md)*

| File | Size (hex) | Size (dec) |
|------|------------|------------|
| `npcdata.bin` | 0x94BC | 38,076 |
| `npcgroup.bin` | 0x66 | 102 |

### Party Alignment (NPC Dialog Theme)

Controls NPC dialog frame textures.

*Source: [npc-dialogue.md](../npc-dialogue.md)*

| Value | Alignment | UI Theme Suffix |
|-------|-----------|-----------------|
| 0 | Good | `-B` |
| 1 | Neutral | `-A` (default) |
| 2 | Evil | `-C` |

---

## Record Size Summary

Key data structure sizes used throughout the engine.

| Structure | Size (bytes) | Hex | Source |
|-----------|-------------|-----|--------|
| Player record | 7,004 | 0x1B3C | [character-system.md](../character-system.md) |
| Monster/Actor record | 836 | 0x344 | [monster-ai.md](../monster-ai.md) |
| Item instance | 36 | 0x24 | [item-system.md](../item-system.md) |
| Item description entry | 48 | 0x30 | [item-system.md](../item-system.md) |
| Spell data record | 36 | 0x24 | [spell-system.md](../spell-system.md) |
| Window struct | 84 | 0x54 | [ui-windows.md](../ui-windows.md) |
| BLV header | 136 | 0x88 | [blv-indoor-maps.md](../blv-indoor-maps.md) |
| BLV face | 96 | 0x60 | [blv-indoor-maps.md](../blv-indoor-maps.md) |
| BLV sector | 116 | 0x74 | [blv-indoor-maps.md](../blv-indoor-maps.md) |
| NPC entry | 76 | 0x4C | [npc-dialogue.md](../npc-dialogue.md) |
| Save header.bin | 100 | 0x64 | [save-load.md](../save-load.md) |
| Save party.bin | 90,680 | 0x16238 | [save-load.md](../save-load.md) |
| Save clock.bin | 40 | 0x28 | [save-load.md](../save-load.md) |
| Save timer.bin | 1,008 | 0x3F0 | [save-load.md](../save-load.md) |
| Event index entry | 12 | 0x0C | [event-engine.md](../event-engine.md) |
| Timer entry | 32 | 0x20 | [event-engine.md](../event-engine.md) |
| Global variable entry | 76 | 0x4C | [event-engine.md](../event-engine.md) |
| Monster type table entry | 152 | 0x98 | [monster-ai.md](../monster-ai.md) |
