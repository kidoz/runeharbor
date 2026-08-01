---
title: "Dungeon and Content Generation"
summary: "Content generation places monsters, treasure, and other dynamic objects from map and data inputs."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Dungeon and Content Generation

Content generation places monsters, treasure, and other dynamic objects from map and data inputs.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

The generation system handles runtime content placement: spawning monsters at map
markers, distributing treasure in chests, and populating maps with dynamic content.
The system is driven by data tables (`placemon.txt`, `MapStats.txt`, `rnditems.txt`,
`monlist.txt`) and the spawn markers embedded in map files.

**Source file reference:** `D:\mm7Src_eng\MM7\Code\Generate.cpp` (at `004e8194`)
**Primary function:** `FUN_0044f5a8`

---

## 1. Monster Spawning

### 1.1 Spawn Point Processing

Both BLV and ODM map loaders reference the `"Spawn"` string (at `004ec270`) when
processing map data. Spawn markers are embedded in the map file and define:

- Position (X, Y, Z world coordinates)
- Spawn type identifier (references into monster/NPC tables)
- Group assignment (for coordinated behavior)

### 1.2 Random Monster Creation

The `Generate.cpp` function at `FUN_0044f5a8` creates random monsters based on map
data and placement rules.

Error messages reveal the creation flow:

| Error String | Condition |
|---|---|
| `"Can't create random monster: '%s'! See MapStats.txt and Monsters.txt!"` (at `004e81b8`) | Monster type not found in tables |
| `"Can't create random monster: '%s' See MapStats!"` (at `004e8864`) | Map-specific monster lookup failure |

### 1.3 Placement Data (placemon.txt)

Monster placement rules are loaded from `placemon.txt` (at `004e8844`), parsed by
`FUN_00454f7a`. This table defines:

- Which monsters can appear in which maps
- Encounter difficulty scaling
- Group size ranges
- Spawn probability weights

### 1.4 Monster List (monlist.txt / dmonlist.bin)

The monster definition table defines each monster type:

- Loaded from `dmonlist.bin` (binary) by `FUN_004598e8`, or
- Loaded from `monlist.txt` (text, dev mode) by `FUN_00459935`
- Each entry defines: name, stats, abilities, sprites, sounds

### 1.5 Actor Structure

Spawned monsters become actors in the game world:

```cpp
Actor struct size: 0x14CC = 5,324 bytes
Actor array: DAT_005e4fd0
Actor count: DAT_005fefc0

```

Each actor maintains position, AI state, health, and combat data.

---

## 2. Treasure Generation

### 2.1 Random Item Tables (rnditems.txt)

Random item generation uses probability tables from `rnditems.txt` (loaded at
`0x457800`). These tables define:

- Item rarity tiers (common, uncommon, rare, artifact)
- Per-tier item pools
- Drop probability curves based on dungeon difficulty

### 2.2 Item Enchantments

Two enchantment systems modify generated items:

- **Standard enchantments** from `stditems.txt` (at `0x456d00`): Common modifiers
  like "+1 Might" or "of Fire Resistance"
- **Special enchantments** from `spcitems.txt` (at `0x456d00`): Unique named
  enchantments with powerful effects

### 2.3 Chest Population

Chest definitions are loaded from `dchest.bin` / `chest.def`:

| File            | Loader          | Notes                       |
|-----------------|----------------|-----------------------------|
| `dchest.bin`    | `FUN_00458b88` | Binary format (production)  |
| `chest.def`     | `FUN_00458bd8` | Text format (development)   |

Error messages:

- `"Unable to save dchest.bin!"` (at `004e8cd8`)
- `"ChestDescriptionList::load - Out of Memory!"` (at `004e8d14`)
- `"ChestDescriptionList::load - Unable to open file: %s."` (at `004e8d40`)

Chest data format identifier: `"chest%02d"` (at `004e33ec`) for per-map chest state.
Each chest stores up to 200 bytes of contents in the save file.

---

## 3. Map-Based Content Rules

### 3.1 MapStats.txt Integration

The `MapStats.txt` table (77 maps, 68 bytes per entry) drives generation parameters:

- Monster difficulty tier for the map
- Allowed monster types
- Treasure quality multiplier
- Respawn rules

The generation function cross-references the current map's `MapStats` entry to
determine appropriate content levels.

### 3.2 Per-Map Monster Assignment

Each outdoor map uses a format like `"out%02d.odm"` (at `004eca14`) for
programmatic map referencing. Indoor maps are referenced by their specific names
(e.g., `d05.blv`, `d37.blv`).

Special maps with unique monster handling:

- `d18.blv` and `mdt12.blv` (at `004e6c40`, `004e6c48`) -- referenced in
  `FUN_00444833` for special encounter logic

---

## 4. Object Spawning

### 4.1 Object List (objlist.txt / dobjlist.bin)

Game objects (projectiles, item drops, interactive objects) are defined in the
object list:

- Loaded from `dobjlist.bin` by `FUN_00459115`, or
- Loaded from `objlist.txt` by `FUN_0045915c`
- `"NoPickup"` flag (at `004e8ea0`) marks objects that cannot be picked up

### 4.2 Sprite Objects

Objects in the world are represented as sprite objects:

```cpp
Sprite struct size: 0x70 = 112 bytes
Sprite array: DAT_006650b0
Sprite count: DAT_006650ac

```

The `"explode"` string (at `004e8854`) in `FUN_0045504a` indicates an explosion
effect handler for destructible objects.

---

## 5. Decoration Generation

### 5.1 Decoration List (declist.txt / ddeclist.bin)

Static decorations are defined in the decoration list:

- Loaded from `ddeclist.bin` by `FUN_00458685`, or
- Loaded from `declist.txt` by `FUN_004586e9`
- Each entry: name, sprite reference, flags, sound ID

Maps can optionally disable decorations via the `"nodecorations"` INI flag.

---

## 6. Overlay Generation

Overlay effects (spell visuals, weather, etc.) are defined in:

- `doverlay.bin` by `FUN_00458e41`, or
- `overlay.def` by `FUN_00458e86`

---

## 7. Development Mode

When `DAT_0071fe88 != 0` (set by `-usedefs` command-line flag), the engine loads
text-format tables instead of pre-compiled binary tables. This allows developers to
modify game data without recompilation:

| Binary File      | Text Equivalent | Contents               |
|------------------|-----------------|------------------------|
| `dsft.bin`       | `sft.txt`       | Sprite frame table     |
| `dtft.bin`       | `tft.def`       | Texture frame table    |
| `dtile.bin`      | `tile.def`      | Tile table             |
| `dpft.bin`       | `pft.def`       | Particle frame table   |
| `dift.bin`       | `ift.txt`       | Icon frame table       |
| `ddeclist.bin`   | `declist.txt`   | Decoration list        |
| `dobjlist.bin`   | `objlist.txt`   | Object list            |
| `dmonlist.bin`   | `monlist.txt`   | Monster list           |
| `dchest.bin`     | `chest.def`     | Chest descriptions     |
| `doverlay.bin`   | `overlay.def`   | Overlay definitions    |
| `dsounds.bin`    | `sounds.def`    | Sound definitions      |

---

## 8. Generation Timing

Content generation occurs at specific points:

1. **Map load (first visit)**: Full generation -- spawn all monsters, populate
   chests, place objects
2. **Map load (return visit)**: Restore saved state; respawn only if respawn
   conditions are met
3. **Rest/time passage**: Potential respawn of cleared content based on elapsed
   game time
4. **New game**: Uses `data\new.lod` template which contains pre-generated
   initial state for all maps

---

## Integration notes

- Generation is data-driven: all parameters come from text/binary tables
- The random seed is initialized from `GetTickCount()` at startup
- Monster spawn positions are baked into map files as named markers
- Treasure generation should be deterministic given the same seed (for save
  compatibility)
- The `-usedefs` flag allows hot-reloading data from text files during development
- Error messages indicate the system fails gracefully (skips invalid entries
  rather than crashing)
- Chest state is serialized per-map in save files (200 bytes per chest)
- Actor state is serialized per-map (5,324 bytes per actor)
