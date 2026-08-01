---
title: "Struct Catalog"
summary: "This catalog consolidates identified record layouts and sizes across engine subsystems."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Struct Catalog

This catalog consolidates identified record layouts and sizes across engine subsystems.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](../contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

## Overview

This catalog documents **40+ data structures** identified across 16 game subsystems. Structures range from small 6-byte vertex records to the massive 7,004-byte character struct. All offsets are from the start of each record unless noted otherwise.

---

## Table of Contents

1. [Archive System](#1-archive-system)
2. [Indoor Maps (BLV)](#2-indoor-maps-blv)
3. [Outdoor Maps (ODM)](#3-outdoor-maps-odm)
4. [Character System](#4-character-system)
5. [Item System](#5-item-system)
6. [Monster / Actor System](#6-monster-actor-system)
7. [NPC System](#7-npc-system)
8. [Sprite / Billboard System](#8-sprite-billboard-system)
9. [Spell System](#9-spell-system)
10. [Event Engine](#10-event-engine)
11. [Save/Load System](#11-saveload-system)
12. [Lighting System](#12-lighting-system)
13. [Combat System](#13-combat-system)

---

## 1. Archive System

### LOD File Header (256 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x000 | 4 | u32 | signature | Magic number: `0x00016741` (91,969) |
| 0x004 | 4 | char[4] | identTag | ASCII tag: `"mvii"` |
| 0x008 | 80 | char[80] | description | Human-readable archive description |
| 0x058 | 80 | char[80] | chapterName | Default chapter name (e.g., `"chapter"`) |
| 0x0A8 | 4 | u32 | fileSize | Total archive file size (optional, may be 0) |
| 0x0AC | 4 | u32 | dataStart | Byte offset where data section begins |
| 0x0B0 | 4 | u32 | numDirectoryEntries | Total top-level directory entries |
| 0x0B4 | 76 | byte[76] | reserved | Padding / reserved (zeroed) |

### LOD Directory Entry (32 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 16 | char[16] | name | Null-terminated filename (case-insensitive) |
| 0x10 | 4 | u32 | dataOffset | Byte offset to file data |
| 0x14 | 4 | u32 | dataSize | Compressed data size in bytes |
| 0x18 | 4 | u32 | decompressedSize | Uncompressed size (0 = not compressed) |
| 0x1C | 2 | u16 | numSubItems | Sub-item / chapter page count |
| 0x1E | 2 | u16 | flags | Padding / flags |

### ImageFileHeader (48 bytes)

Used for texture/image entries in external-only archives (ICONS.LOD, SPRITES.LOD).

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 16 | char[16] | name | Texture name (null-terminated) |
| 0x10 | 4 | u32 | size | Total entry size |
| 0x14 | 4 | u32 | dataSize | Compressed pixel data size |
| 0x18 | 2 | u16 | width | Image width in pixels |
| 0x1A | 2 | u16 | height | Image height in pixels |
| 0x1C | 2 | u16 | widthLn2 | Log2 of width |
| 0x1E | 2 | u16 | heightLn2 | Log2 of height |
| 0x20 | 4 | u32 | paletteId1 | Primary palette index |
| 0x24 | 4 | u32 | paletteId2 | Secondary palette index |
| 0x28 | 4 | u32 | decompressedSize | Uncompressed size (0 = raw) |
| 0x2C | 4 | u32 | flags | Image flags / attributes |

---

## 2. Indoor Maps (BLV)

### BLV Header (136 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | u32 | version | Format version number |
| 0x04 | 76 | char[76] | levelName | Level display name (null-terminated) |
| 0x50 | 16 | char[16] | skyTexture | Sky texture name (null-terminated) |
| 0x60 | 8 | byte[8] | reserved1 | Reserved / unused |
| 0x68 | 4 | u32 | faceDataSize | Total size of face auxiliary data (bytes) |
| 0x6C | 4 | u32 | sectorRDataSize | Total size of sector runtime data (bytes) |
| 0x70 | 4 | u32 | sectorLRDataSize | Total size of sector light data (bytes) |
| 0x74 | 4 | u32 | doorsDataSize | Total size of door data (bytes) |
| 0x78 | 16 | byte[16] | reserved2 | Reserved / padding (zeroed) |

### BLV Vertex (6 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 2 | i16 | x | X coordinate |
| 0x02 | 2 | i16 | y | Y coordinate |
| 0x04 | 2 | i16 | z | Z coordinate |

### BLV Face (96 bytes)

Contains two plane representations: float (for rendering) and fixed-point (for collision).

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | f32 | normalX_f | Plane normal X (IEEE 754 float) |
| 0x04 | 4 | f32 | normalY_f | Plane normal Y (float) |
| 0x08 | 4 | f32 | normalZ_f | Plane normal Z (float) |
| 0x0C | 4 | f32 | distance_f | Plane distance (float) |
| 0x10 | 4 | i32 | normalX_i | Plane normal X (fixed-point, x65536) |
| 0x14 | 4 | i32 | normalY_i | Plane normal Y (fixed-point) |
| 0x18 | 4 | i32 | normalZ_i | Plane normal Z (fixed-point) |
| 0x1C | 4 | i32 | distance_i | Plane distance (fixed-point) |
| 0x20 | 4 | i32 | zCalc1 | Z-calculation coefficient 1 |
| 0x24 | 4 | i32 | zCalc2 | Z-calculation coefficient 2 |
| 0x28 | 4 | i32 | zCalc3 | Z-calculation coefficient 3 |
| 0x2C | 2 | u16 | attributes | Face attribute flags (bitfield) |
| 0x2E | 2 | u16 | vertexCount | Number of vertices (low byte used) |
| 0x30 | 4 | ptr | vertexListPtr | Pointer to vertex index list (runtime) |
| 0x34 | 4 | ptr | xInterceptPtr | Pointer to X intercepts (runtime) |
| 0x38 | 4 | ptr | yInterceptPtr | Pointer to Y intercepts (runtime) |
| 0x3C | 4 | ptr | zInterceptPtr | Pointer to Z intercepts (runtime) |
| 0x40 | 4 | ptr | uCoordPtr | Pointer to U texture coords (runtime) |
| 0x44 | 4 | ptr | vCoordPtr | Pointer to V texture coords (runtime) |
| 0x48 | 2 | u16 | sectorIndex | Sector this face belongs to |
| 0x4A | 2 | u16 | textureIndex | Texture handle (resolved at load) |
| 0x4C | 1 | u8 | lightLevel | Face light level |
| 0x4D | 3 | byte[3] | reserved | Reserved bytes |
| 0x50 | 4 | i32 | bboxMinX | Bounding box minimum X |
| 0x54 | 4 | i32 | bboxMinY | Bounding box minimum Y |
| 0x58 | 4 | i32 | bboxMinZ | Bounding box minimum Z |
| 0x5C | 1 | u8 | polygonType | Polygon type / orientation |
| 0x5D | 1 | u8 | numVertices | Vertex count (duplicate) |
| 0x5E | 2 | u16 | padding | Alignment padding |

**Face Attribute Flags (offset 0x2C):**

| Bit | Hex | Meaning |
|-----|-----|---------|
| 0 | 0x0001 | Is portal (sector boundary) |
| 2 | 0x0004 | Visible on automap |
| 3 | 0x0008 | Projectile-passable |
| 4 | 0x0010 | Scrolled texture |
| 5 | 0x0020 | Alternate lighting mode |
| 11 | 0x0800 | No dynamic lighting |
| 13 | 0x2000 | Clickable / interactive |
| 14 | 0x4000 | Pressure plate |

### BLV Face Extra (36 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 2 | u16 | flags | Extra attribute flags |
| 0x02 | 2 | u16 | faceIndex | Index of the parent face |
| 0x04 | 32 | byte[32] | data | Extended face properties |

### BLV Sector (116 bytes)

Room/area definitions with runtime pointer fields on disk.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | varies | mixed | counts | Floor/wall/ceiling/portal face counts |
| varies | varies | ptr[] | faceLists | Pointers to floor/wall/ceiling/portal face arrays (runtime) |
| varies | 4 | u32 | decorCount | Number of decorations in sector |
| varies | 4 | ptr | decorListPtr | Pointer to decoration index list (runtime) |
| varies | varies | mixed | lightData | Sector ambient light and light counts |

*Note: The 116-byte sector struct contains multiple count+pointer pairs distributed from the flat `sectorData[]` and `sectorLightData[]` arrays during load.*

### BLV Decoration (32 bytes)

3D props and objects placed within indoor maps.

### BLV Light (16 bytes)

Indoor light source definition.

### BLV BSP Node (8 bytes)

Binary space partition tree node for indoor rendering.

### BLV Map Outline (12 bytes)

Automap line segment for the minimap display.

---

## 3. Outdoor Maps (ODM)

### ODM Structure Init

| Offset | Size | Type | Field | Default |
|--------|------|------|-------|---------|
| 0x00 | 32 | char[32] | mapName | `"blank"` |
| 0x20 | 32 | char[32] | formatId | `"i6.odm"` |
| 0x40 | 32 | char[32] | versionString | `"MM6 Outdoor v1.00"` |
| 0x60 | 32 | char[32] | skyTexture | (empty) |
| 0x80 | 32 | char[32] | groundTexture | (empty) |
| 0xB0 | 4 | u32 | modelCount | 0 |
| 0xD4 | 4 | ptr | heightMapPtr | -> 32,768 bytes allocated |
| 0xE4 | 4 | ptr | tileMapPtr | -> 65,536 bytes allocated, zeroed |

### ODM Height Map

128x128 grid, 2 bytes per cell = 32,768 bytes total. `height = heightMap[y * 128 + x]`

### ODM Tile Map

128x128 grid, 4 bytes per cell = 65,536 bytes total. Each entry encodes texture index and tile attributes.

### ODM Outdoor Face (188 bytes)

Outdoor faces are larger than indoor faces (96B vs 188B), with additional fields for outdoor-specific rendering (lighting direction, distance fog, terrain blending).

### MapStats Entry (68 bytes)

Map metadata from `MapStats.txt` (77 maps):

| Column | Offset | Type | Purpose |
|--------|--------|------|---------|
| 1 | +0x00 | ptr | Map display name |
| 2 | +0x04 | ptr | Map filename (.blv / .odm) |
| 3 | +0x14 | i32 | Map attribute 1 |
| 4 | +0x18 | i32 | Map attribute 2 |
| 5 | +0x28 | i32 | Map attribute 3 |

---

## 4. Character System

### Character Struct (7,004 bytes)

Base address: `DAT_00ACD804 + i * 0x1B3C` for character `i` (0-3).

#### Overview (first 0xBC bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x00 | 144 | i64[18] | conditions | Condition timestamps; offset = index * 8 |
| +0x90 | 16 | -- | (unknown) | 16 bytes between conditions and experience |
| +0xA0 | 8 | i64 | experience | Experience points |
| +0xA8 | 16 | char[16] | name | Character name (null-terminated) |
| +0xB8 | 1 | u8 | sex | 0 = Male, 1 = Female (may also serve as voice set selector) |
| +0xB9 | 1 | u8 | classId | Class index; changes in-place on promotion |
| +0xBA | 1 | u8 | portraitIndex | Face/portrait index (cycles modulo 20) |
| +0xBB | 1 | -- | (padding) | Alignment padding |

#### Conditions (18 timestamp slots)

Offset formula: `conditionIndex * 8` from character struct base.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x00 | 8 | i64 | condCursed | Cursed (index 0) |
| +0x08 | 8 | i64 | condWeak | Weak (index 1) |
| +0x10 | 8 | i64 | condAsleep | Asleep (index 2) |
| +0x18 | 8 | i64 | condAfraid | Afraid (index 3) |
| +0x20 | 8 | i64 | condDrunk | Drunk (index 4) |
| +0x28 | 8 | i64 | condInsane | Insane (index 5) |
| +0x30 | 8 | i64 | condPoison1 | Poison weak (index 6) |
| +0x38 | 8 | i64 | condDisease1 | Disease weak (index 7) |
| +0x40 | 8 | i64 | condPoison2 | Poison medium (index 8) |
| +0x48 | 8 | i64 | condDisease2 | Disease medium (index 9) |
| +0x50 | 8 | i64 | condPoison3 | Poison severe (index 10) |
| +0x58 | 8 | i64 | condDisease3 | Disease severe (index 11) |
| +0x60 | 8 | i64 | condParalyzed | Paralyzed (index 12) |
| +0x68 | 8 | i64 | condUnconscious | Unconscious (index 13) |
| +0x70 | 8 | i64 | condDead | Dead (index 14) |
| +0x78 | 8 | i64 | condStoned | Petrified (index 15) |
| +0x80 | 8 | i64 | condEradicated | Eradicated (index 16) |
| +0x88 | 8 | i64 | condZombie | Zombie (index 17) |

#### Experience and Level

Fields +0xA0 through +0xB9 are covered in the Overview table above.

#### Base Stats (7 stats x 2 fields each)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0xBC | 2 | i16 | mightBase | Might (base) |
| +0xBE | 2 | i16 | mightBonus | Might (item/magic bonus) |
| +0xC0 | 2 | i16 | intellectBase | Intellect (base) |
| +0xC2 | 2 | i16 | intellectBonus | Intellect (bonus) |
| +0xC4 | 2 | i16 | personalityBase | Personality (base) |
| +0xC6 | 2 | i16 | personalityBonus | Personality (bonus) |
| +0xC8 | 2 | i16 | enduranceBase | Endurance (base) |
| +0xCA | 2 | i16 | enduranceBonus | Endurance (bonus) |
| +0xCC | 2 | i16 | speedBase | Speed (base) |
| +0xCE | 2 | i16 | speedBonus | Speed (bonus) |
| +0xD0 | 2 | i16 | accuracyBase | Accuracy (base) |
| +0xD2 | 2 | i16 | accuracyBonus | Accuracy (bonus) |
| +0xD4 | 2 | i16 | luckBase | Luck (base) |
| +0xD6 | 2 | i16 | luckBonus | Luck (bonus) |
| +0xD8 | 2 | i16 | armorClassBonus | AC bonus |
| +0xDA | 2 | i16 | level | Current level |
| +0xDC | 2 | i16 | levelBonus | Level bonus (magic) |
| +0xDE | 2 | i16 | ageModifier | Age modifier |

#### Skills

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x108 | 74 | u16[37] | skills | 37 skill slots, mastery+level encoded |

#### Equipment and Inventory

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x1F0 | 5,040 | Item[140] | inventory | 140 item slots x 36 bytes |
| +0x1948 | 4 | i32 | equipMainhand | Mainhand equipment slot index |
| +0x194C | 4 | i32 | equipOffhand | Offhand equipment slot index |
| +0x1950 | 4 | i32 | equipBow | Bow equipment slot index |
| +0x1954 | 4 | i32 | equipArmor | Body armor slot index |

#### Resistances

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x1774 | 16 | i16[8] | resistanceBase | Base resistances (Fire/Air/Water/Earth/Mind/x/x/Body) |
| +0x178A | 16 | i16[8] | resistanceBonus | Resistance bonuses |

#### Hit Points and Misc

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x193C | 4 | i32 | currentHP | Current hit points |
| +0x1944 | 4 | i32 | birthYear | Birth year (for age calculation) |
| +0x1A92 | 1 | i8 | meleeDmgBonus | Flat melee damage bonus |
| +0x1A94 | 1 | i8 | meleeAttackBonus | Flat melee attack bonus |
| +0x1A96 | 1 | i8 | rangedAttackBonus | Flat ranged attack bonus |
| +0x1A98 | 1 | i8 | hpBonus | Flat HP bonus |
| +0x1A9A | 1 | i8 | spBonus | Flat SP bonus |

---

## 5. Item System

### Item Instance (36 bytes)

Each item in world/inventory/chest. Accessed at `character_base + 0x1F0 + slot * 0x24`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | i32 | itemId | Index into item description table (1-based; 0 = empty) |
| 0x04 | 4 | i32 | stdEnchantId | Standard enchantment ID (from stditems.txt) |
| 0x08 | 4 | i32 | stdEnchantPower | Standard enchantment bonus value |
| 0x0C | 4 | i32 | spcEnchantId | Special enchantment ID (from spcitems.txt) |
| 0x10 | 4 | i32 | numCharges | Remaining charges (wands, artifacts) |
| 0x14 | 4 | u32 | flags | Bitfield (see below) |
| 0x18 | 4 | i32 | bodyPartIndex | Equipment body slot index |
| 0x1C | 4 | i32 | maxCharges | Maximum charges |
| 0x20 | 4 | i32 | ownerPlayer | Owning player index or extra data |

**Item Flags:**

| Bit | Hex | Name | Description |
|-----|-----|------|-------------|
| 0 | 0x01 | IDENTIFIED | Shows full name/stats |
| 1 | 0x02 | BROKEN | Needs repair |
| 2 | 0x04 | CURSED | Cannot be unequipped |
| 3 | 0x08 | TEMP_BONUS | Temporary enchantment |
| 4 | 0x10 | AURA | Visible aura effect |
| 5 | 0x20 | STOLEN | Shops may refuse |
| 6 | 0x40 | HARDENED | Extra durability |
| 7 | 0x80 | QUEST_BIT | Quest item tracking |

### Item Description Table Entry (48 bytes)

Static item type definitions loaded from `items.txt`. Base: `DAT_005d2864`, up to 800 entries.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | ptr | iconName | Icon texture name |
| 0x04 | 4 | ptr | name | Item name |
| 0x08 | 4 | ptr | unidentName | Unidentified item name |
| 0x0C | 4 | ptr | description | Item description text |
| 0x10 | 4 | i32 | value | Base gold value |
| 0x14 | 2 | i16 | spriteId | Sprite ID for world display |
| 0x18 | 2 | i16 | flags2 | Additional type flags |
| 0x1C | 1 | u8 | enchantChance | Random enchant chance |
| 0x1D | 1 | u8 | weaponSkill | Weapon skill type |
| 0x1E | 1 | u8 | damageDice | Number of damage dice |
| 0x1F | 1 | u8 | damageSides | Sides per damage die |
| 0x20 | 1 | u8 | equipType | Equipment category (0-20) |
| 0x21 | 1 | u8 | materialType | Material/skill type |
| 0x24 | 1 | u8 | mod1 | AC bonus for armor |
| 0x25 | 1 | u8 | itemClass | 0=normal, 1=artifact, 2=relic, 3=special |
| 0x28 | 4 | i32 | valueMultiplier | Gold value modifier |
| 0x2C | 2 | i16 | idSkillReq | Skill required to identify |

---

## 6. Monster / Actor System

### Actor Struct (836 bytes)

Array at `DAT_005feffc`, count at `DAT_006650a8`. Stride: 836 bytes = 418 shorts = 209 ints.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x00 | 4 | u32 | flags | Bit 2=active, 6=visible, 15=hostile, 19=always-hostile, 24=combat |
| +0x08 | 2 | u16 | spriteType | Monster type / sprite ID |
| +0x12 | 1 | u8 | canFly | Non-zero if can fly |
| +0x13 | 1 | u8 | aiType | 0=Normal, 1=Wimp, 2=Aggressive, 4=AdditionalHostile, 5=Fleeing |
| +0x14 | 1 | u8 | moveSpeed | Movement type category |
| +0x15 | 1 | u8 | hostilityLevel | 0=friendly, 4=hostile |
| +0x1D | 1 | u8 | hasSpell1 | Non-zero if has first spell ability |
| +0x1E | 1 | u8 | statusFlags | Bit 4 = invisible |
| +0x23 | 1 | u8 | spellType1 | First spell attack type ID |
| +0x24 | 4 | u32 | flagsExtended | Bit 19 = permanently hostile (0x80000) |
| +0x25 | 1 | u8 | spellType2 | Second spell attack type ID |
| +0x27 | 1 | u8 | spellType3 | Third spell attack type ID |
| +0x34 | 4 | u32 | groupId | Alliance group ID |
| +0x38 | 2 | u16 | monsterTypeId | Index into monster type table |
| +0x3C | 2 | u16 | attack1Spell | First spell for spell attacks |
| +0x3E | 2 | u16 | attack2Spell | Second spell for spell attacks |
| +0x44 | 4 | i32 | maxHP | Maximum hit points |
| +0x54 | 4 | i32 | recoveryTimer | Counts down; cannot act while > 0 |
| +0x60 | 2 | u16 | allianceGroup | Alliance/faction for hostility |
| +0x62 | 2 | u16 | height | Actor sprite height |
| +0x64 | 2 | u16 | moveSpeedValue | Numeric movement speed |
| +0x66 | 2 | i16 | positionX | World position X |
| +0x68 | 2 | i16 | positionY | World position Y |
| +0x6A | 2 | i16 | positionZ | World position Z |
| +0x6C | 2 | i16 | velocityX | Current velocity X |
| +0x6E | 2 | i16 | velocityY | Current velocity Y |
| +0x70 | 2 | i16 | velocityZ | Current velocity Z |
| +0x72 | 2 | u16 | facingAngle | Actor yaw / facing direction |
| +0x74 | 2 | u16 | pitchAngle | Actor pitch angle |
| +0x78 | 2 | u16 | movementSpeed | Movement speed per tick |
| +0x7A | 2 | u16 | sectorId | Indoor sector index (BLV) |
| +0x88 | 2 | u16 | aiState | Current AI state (see state machine) |
| +0x8C | 2 | i16 | currentHP | Current hit points |
| +0x90 | 4 | i32 | aiTimer | Time in current AI state |
| +0xE4 | 8 | i64 | charmTime | Charm/berserk condition time |
| +0xF0 | 8 | i64 | stoneTime | Stoned condition time |
| +0x124 | 2 | u16 | eventTriggerId | Event to fire on death |
| +0x164 | 8 | i64 | deadTime | Dead condition time |
| +0x194 | 8 | i64 | paralyzeTime | Paralysis condition time |
| +0x1FC | 4 | i32 | summonGroup1 | First summoned monster group |
| +0x29C | 4 | i32 | targetId | Current target (packed: bits 0-2=type, 3+=index) |
| +0x2C4 | 4 | i32 | teamId | Team membership ID |
| +0x2C8 | 4 | i32 | hostilityOverride | Overridden hostility group; 9999=friendly |
| +0x308 | 4 | i32 | aggroTarget | Current aggro target reference |

**AI State Values:**

| Value | Name | Description |
|-------|------|-------------|
| 0 | Standing | Default idle |
| 1 | Wandering | Moving randomly |
| 2 | Guarding | Patrolling |
| 3 | Fidgeting | Idle animation |
| 4 | Fleeing | Running away |
| 5 | Dead | Excluded from AI |
| 6 | Pursuing | Moving toward target |
| 7 | Attacking | Melee attack |
| 8 | AttackingRanged | Ranged/spell attack |
| 9 | AttackingMelee2 | Alternate melee |
| 11 | Stunned | Temporarily incapacitated |
| 12 | CastingSpell1 | First spell ability |
| 13 | CastingSpell2 | Second spell ability |
| 17 | Paralyzed | Held by paralysis |
| 18 | CastingSpell3 | Third spell ability |
| 19 | Stoned | Petrified |

**Target ID Encoding:** `(actorIndex << 3) | type` where type: 3=another actor, 4=party/player.

---

## 7. NPC System

### NPC Entry (76 bytes)

500 entries at NPC manager offset `+0x17FC4`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x00 | 4 | ptr | name | Name string pointer |
| +0x04 | 4 | i32 | flags | NPC behavioral flags |
| +0x08 | 12 | i32[3] | reserved | Fields 2-4 (parsed, usage unclear) |
| +0x14 | 4 | i32 | professionId | Index into profession table (0-58) |
| +0x18 | 4 | i32 | topic1 | First dialogue topic ID |
| +0x1C | 4 | i32 | topic2 | Second dialogue topic ID |
| +0x20 | 4 | i32 | joinsParty | Hireable? (1=yes, 0=no) |
| +0x24 | 4 | i32 | padding | Gap before greeting |
| +0x28 | 4 | i32 | greetingGroup | Index into greeting table (0-204) |
| +0x2C | 20 | i32[5] | dialogActions | Response button event IDs (5 slots) |

### NPC Name Entry (8 bytes)

540 entries at NPC manager offset `+0x12978`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | ptr | firstName | First name string |
| 0x04 | 4 | ptr | lastName | Last name string |

### NPC Greeting Entry (8 bytes)

205 entries at NPC manager offset `+0x1788C`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | ptr | greeting1 | Primary greeting text |
| 0x04 | 4 | ptr | greeting2 | Alternate greeting text |

### NPC Profession Entry (20 bytes)

59 entries at NPC manager offset `+0x13A78`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | i32 | cost | Hiring cost (gold) |
| 0x04 | 4 | ptr | benefitText | Passive benefit description |
| 0x08 | 4 | ptr | joinText | Text when hired |
| 0x0C | 4 | ptr | actionText | Profession action text |
| 0x10 | 4 | ptr | dismissText | Text when dismissed |

---

## 8. Sprite / Billboard System

### Sprite Frame Entry (60 bytes)

Animation frame definitions loaded from `dsft.bin` / `sft.txt`.

| Array | Entry Size | Purpose |
|-------|-----------|---------|
| S_Frames | 60 bytes (0x3C) | Sprite frame data |
| E_Frames | 2 bytes | End frame indices |
| P_Frames | 4 bytes | Frame pointer table |

### Billboard Render Entry (52 bytes)

Render list at `DAT_005120C0`, count at `DAT_00518660`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 2 | i16 | screenXLeft | Left screen X |
| 0x02 | 2 | i16 | screenXRight | Right screen X |
| 0x04 | 2 | i16 | screenYTop | Top screen Y |
| 0x06 | 2 | i16 | screenYBottom | Bottom screen Y |
| 0x08 | 2 | i16 | depthOrZ | View-space depth (for sorting) |
| 0x0A | 2 | u16 | flags | Bit 0x02=translucent, 0x40=mirror, 0x80=oscillate |
| 0x0C | 4 | ptr | texturePtr | Texture/palette pointer |
| 0x18 | 4 | ptr | spriteFrameData | Frame table reference |
| 0x22 | 2 | i16 | referenceSpriteIndex | Source sprite index |
| 0x2C | 2 | i16 | facingDirection | 8-directional facing |

---

## 9. Spell System

### Spell Data Record (36 bytes)

Loaded from `spells.txt`, stride 0x24. Base offset: SpellManager + 0x40.

| Field Index | Offset | Size | Type | Description |
|-------------|--------|------|------|-------------|
| 2 | -0x1C | 4 | ptr | Normal description text |
| 3 | +0x00 | 1 | u8 | Spell school (0-10) |
| 4 | -0x18 | 4 | ptr | Expert description |
| 5 | -0x14 | 4 | ptr | Master description |
| 6 | -0x10 | 4 | ptr | Grandmaster description |
| 7 | -0x0C | 4 | i32 | Mana cost / parameter A |
| 8 | -0x08 | 4 | i32 | Casting delay / parameter B |
| 9 | -0x04 | 4 | i32 | Recovery time / parameter C |
| 10 | (flags) | 1 | u8 | Target flags bitmask |

**Target Flags:**

| Char | Bit | Hex | Meaning |
|------|-----|-----|---------|
| `m` | 0 | 0x01 | Targets monsters |
| `e` | 1 | 0x02 | Targets environment/objects |
| `c` | 2 | 0x04 | Targets caster/party |
| `x` | 3 | 0x08 | Special targeting mode |

---

## 10. Event Engine

### EVT Command (variable length)

Event bytecode command from `.evt` files:

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 1 | u8 | cmdSize | Command size (N), excluding this byte |
| 0x01 | 2 | u16 | eventId | Event ID (little-endian) |
| 0x03 | 1 | u8 | subIndex | Sub-event index (sequence within event) |
| 0x04 | 1 | u8 | opcode | Command type |
| 0x05 | N-4 | u8[] | params | Opcode-dependent parameters |

Total bytes: N + 1. Commands with same eventId form a logical event.

### Event Index Entry (12 bytes)

Index table at `DAT_00598570` (global) / `DAT_005b6458` (map). Up to ~4,400 entries.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | u32 | eventId | Event ID (zero-extended from u16) |
| 0x04 | 4 | u32 | subIndex | Sub-event index (zero-extended from u8) |
| 0x08 | 4 | u32 | byteOffset | Offset into raw bytecode buffer |

---

## 11. Save/Load System

### Save Header (100 bytes)

Internal file `header.bin` in each `.mm7` save archive.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 20 | char[20] | title | Display title (party leader name) |
| 0x14 | 20 | char[20] | locationName | Current map name (e.g., `"d05.blv"`) |
| 0x28 | 4 | u32 | gameTimeLow | Game time ticks (low 32 bits) |
| 0x2C | 4 | u32 | gameTimeHigh | Game time ticks (high 32 bits) |
| 0x30 | 52 | byte[52] | reserved | Padding / unused |

### Party Fields (at DAT_00acce38)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x04 | 4 | i32 | height | Party collision height (default 192) |
| +0x0C | 4 | i32 | eyeLevel | Eye/camera height (default 160) |
| +0x2C | 8 | i64 | gameTime | 64-bit game time in ticks |
| +0x6B4 | 4 | i32 | posX | World X position |
| +0x6B8 | 4 | i32 | posY | World Y position |
| +0x6BC | 4 | i32 | posZ | World Z position |
| +0x6C0 | 4 | i32 | facing | Yaw angle (0-2047) |
| +0x6C4 | 4 | i32 | pitch | Pitch angle |
| +0x6C8 | 12 | i32[3] | savedPos | Respawn/recall X, Y, Z |
| +0x6D4 | 4 | i32 | savedFacing | Respawn/recall yaw |
| +0x6D8 | 4 | i32 | savedPitch | Respawn/recall pitch |

**party.bin total size: 0x16238 = 90,680 bytes** (party header + 4 x 7,004-byte characters)

### Clock (40 bytes)

Internal file `clock.bin`:

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | u32 | gameTimeLow | Low 32 bits of game time |
| 0x04 | 4 | u32 | gameTimeHigh | High 32 bits of game time |
| 0x08 | 32 | byte[32] | timerState | Additional timer/calendar state |

**Time conversion:** ticks / 128 (0x80) = seconds, / 60 = minutes, / 60 = hours, / 24 = days, / 7 = weeks, / 4 = months, / 12 = years. Base year = 0x490 (1168).

### NPC Data (38,076 bytes)

Internal file `npcdata.bin` at `DAT_0072d50c`. 501 entries x 76 bytes each.

### NPC Group (102 bytes)

Internal file `npcgroup.bin` at `DAT_0073bfaa`. Group/faction assignments.

### Timer (1,008 bytes)

Internal file `timer.bin` at `DAT_0050ba60`. Fixed-capacity timer record array.

---

## 12. Lighting System

### Indoor Ambient Light Globals

| Address | Type | Channel |
|---------|------|---------|
| `DAT_00ae3064` | i32 | Ambient Blue |
| `DAT_00ae3068` | i32 | Ambient Green |
| `DAT_00ae306c` | i32 | Ambient Red |

### Outdoor Shading Thresholds

| Parameter | Default | Description |
|-----------|---------|-------------|
| dist_shade | 0x800 (2048) | Shading begins |
| dist_shademist | 0x1000 (4096) | Mist blending starts |
| dist_mist | 0x2000 (8192) | Full fog distance |

---

## 13. Combat System

### Attack Type Enumeration

| Value | Hex | Name | Source | Uses Resistance? |
|-------|-----|------|--------|------------------|
| 34 | 0x22 | Auto-Stun | No damage; forced stun | No |
| 39 | 0x27 | Spell Direct | CalculateSpellDamage | Yes |
| 100 | 0x64 | Ranged Spell | CalculateRangedDamage + element | Yes |
| 101 | 0x65 | Ranged Physical | CalculateRangedDamage | No |
| 102 | 0x66 | Melee | CalculateMeleeBaseDamage | No |

### Sprite Object (112 bytes)

In-flight projectile / world object. Array count at `DAT_006650ac`, stride 0x70.

---

## Cross-Reference

| Struct | Size | System | Doc Reference |
|--------|------|--------|---------------|
| LOD File Header | 256 | Archive | [lod-archives](../lod-archives.md) |
| LOD Directory Entry | 32 | Archive | [lod-archives](../lod-archives.md) |
| ImageFileHeader | 48 | Archive | [lod-archives](../lod-archives.md) |
| BLV Header | 136 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Vertex | 6 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Face | 96 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Face Extra | 36 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Sector | 116 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Decoration | 32 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Light | 16 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV BSP Node | 8 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| BLV Map Outline | 12 | Indoor Maps | [blv-indoor-maps](../blv-indoor-maps.md) |
| ODM Outdoor Face | 188 | Outdoor Maps | [odm-outdoor-maps](../odm-outdoor-maps.md) |
| MapStats Entry | 68 | Maps | [odm-outdoor-maps](../odm-outdoor-maps.md) |
| Character | 7,004 | Character | [character-system](../character-system.md) |
| Item Instance | 36 | Items | [item-system](../item-system.md) |
| Item Description | 48 | Items | [item-system](../item-system.md) |
| Actor | 836 | Monster AI | [monster-ai](../monster-ai.md) |
| NPC Entry | 76 | NPC | [npc-dialogue](../npc-dialogue.md) |
| NPC Name | 8 | NPC | [npc-dialogue](../npc-dialogue.md) |
| NPC Greeting | 8 | NPC | [npc-dialogue](../npc-dialogue.md) |
| NPC Profession | 20 | NPC | [npc-dialogue](../npc-dialogue.md) |
| Sprite Frame | 60 | Sprites | [sprite-billboard](../sprite-billboard.md) |
| Billboard Entry | 52 | Sprites | [sprite-billboard](../sprite-billboard.md) |
| Spell Data Record | 36 | Spells | [spell-system](../spell-system.md) |
| EVT Command | variable | Events | [event-engine](../event-engine.md) |
| Event Index Entry | 12 | Events | [event-engine](../event-engine.md) |
| Save Header | 100 | Save/Load | [save-load](../save-load.md) |
| Party | 90,680 | Save/Load | [save-load](../save-load.md) |
| Clock | 40 | Save/Load | [save-load](../save-load.md) |
| Sprite Object | 112 | Combat | [combat-system](../combat-system.md) |
