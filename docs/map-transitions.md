---
title: "Map Transitions and Level Loading"
summary: "Map transitions preserve map state while switching between indoor and outdoor levels."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Map Transitions and Level Loading

Map transitions preserve map state while switching between indoor and outdoor levels.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

Map transitions handle loading new levels (indoor BLV or outdoor ODM), preserving
state for previously visited maps, selecting spawn points, and managing the loading
screen display. The system is driven by the game state machine with dedicated state
values for transitions.

**Key globals:**

- `DAT_006be1e0` -- Map type flag: `1` = indoor (BLV), `2` = outdoor (ODM)
- `DAT_006be1c4` -- Current map filename (e.g., `"out01.odm"` or `"d05.blv"`)
- `DAT_006a0bc4` -- Game flow state (value `6` = level transition)
- `DAT_006a0bc8` -- Gameplay sub-state (value `6` = level change)

---

## 1. Map Type Detection

The engine determines indoor vs. outdoor mode from the filename extension:

- Files ending in `.blv` set `DAT_006be1e0 = 1` (indoor mode)
- Files ending in `.odm` set `DAT_006be1e0 = 2` (outdoor mode)

Specific maps are checked by name for special-case handling:

| Map Check                        | Purpose                       |
|----------------------------------|-------------------------------|
| `stricmp(mapName, "d05.blv")`    | Special indoor map handling   |
| `stricmp(mapName, "out15.odm")`  | Special outdoor map handling  |
| `stricmp(mapName, "d10.blv")`    | Transition target check       |
| `stricmp(mapName, "d11.blv")`    | Transition target check       |
| `stricmp(mapName, "d23.blv")`    | Memory limit check            |

---

## 2. Level Loading Sequence

### 2.1 Pre-Load Phase

1. **Save current map state**: If a level is already loaded, the engine serializes
   the current map's dynamic state (actors, items, chest contents, modified faces)
   into the save LOD so it can be restored if the player returns.

2. **Determine target map**: The target map filename is read from the transition
   trigger (face event, teleport, or script command).

3. **Display loading screen**: The engine loads and renders `lsave640.pcx` from
   BITMAPS.LOD as a loading screen background. For numbered loading screens, the
   format `loading%d.pcx` is used.

### 2.2 Map Load Phase

4. **Look up map in LOD**: The target map is searched in `data\games.lod` (for saved
   state) or `data\new.lod` (for first visit). Error `"Unable to find %s in
   Games.LOD"` on failure.

5. **Detect format**: Based on file extension:
   - `.blv` -> Call BLV loader at `0x00498d93` (indoor, 136-byte header)
   - `.odm` -> Call ODM loader at `0x0047d0aa` (outdoor, height/tile maps)

6. **Parse map data**: The loader reads all sections sequentially (see
   [blv-indoor-maps.md](blv-indoor-maps.md) or [odm-outdoor-maps.md](odm-outdoor-maps.md)).

7. **Resolve textures**: Texture names from faces and decorations are looked up in
   BITMAPS.LOD and resolved to runtime handles via `FUN_0040fb2c`.

### 2.3 Post-Load Phase

8. **Select spawn point**: Based on the transition direction, the engine searches
   for the appropriate named spawn point:

   | Direction | Spawn Point Name | Index |
   |-----------|------------------|-------|
   | Default   | `"Party Start"`  | 0     |
   | North     | `"North Start"`  | 1     |
   | South     | `"South Start"`  | 2     |
   | East      | `"East Start"`   | 3     |
   | West      | `"West Start"`   | 4     |

   The function at `0x004498f8` iterates spawn points and sets the party position
   from the matching marker's coordinates.

9. **Set party position**: Party world coordinates are written:
   - `DAT_00acd4ec` = X position
   - `DAT_00acd4f0` = Y position
   - `DAT_00acd4f4` = Z position
   - `DAT_00acd4f8` = Facing (yaw angle)
   - `DAT_00acd4fc` = Pitch

10. **Initialize subsystems**: Lighting, audio reverb (from MapStats.txt), monster
    AI, and decoration animations are initialized for the new map.

11. **Return to gameplay**: `DAT_006a0bc8` is set to `2` (level loaded / entering),
    and the game loop resumes normal rendering.

---

## 3. State Machine Transitions

### Game Flow State (`DAT_006a0bc4`)

| Transition                      | State Value | Next Action              |
|---------------------------------|-------------|--------------------------|
| New game start                  | 1           | Load `out01.odm`         |
| Level transition (in-game)      | 6           | Load target map          |
| Return to title                 | 5 or 9      | Cleanup and show title   |

### Gameplay Sub-State (`DAT_006a0bc8`)

| Sub-State | Value | Description                              |
|-----------|-------|------------------------------------------|
| Normal    | 0     | Regular gameplay                         |
| Transitioning | 1 | Break out of current loop               |
| Entering  | 2     | Level loaded, entering gameplay          |
| Level change | 6  | Initiating level change                  |
| Reset     | 7     | Full reset via `FUN_004ab69f(-1, -1)`    |
| Special   | 8     | Special transition handling              |
| Exit      | 9     | Exit to title screen                     |

---

## 4. Loading Screen

The loading screen (`lsave640.pcx`) is displayed during transitions:

1. Load `lsave640.pcx` from BITMAPS.LOD (24-bit PCX, 640x480)
2. Render to back buffer
3. Flip to display
4. Perform map loading while screen is visible
5. After load completes, transition to 3D viewport

For numbered loading screens (used during specific story transitions), the format
`loading%d.pcx` is used with a numeric index.

---

## 5. Map Data Preservation

When leaving a map, the engine preserves:

- **Actor state**: Positions, HP, AI state, death flags (0x14CC bytes per actor)
- **Sprite objects**: Dropped items, projectiles (0x70 bytes per sprite)
- **Chest contents**: Modified chest inventories (200 bytes per chest)
- **Map variables**: Script state variables (0x38 bytes)
- **Face state**: Modified face flags (opened doors, destroyed walls)

This data is written to the current save LOD (either `data\new.lod` during gameplay
or the active save file), keyed by the map filename. When the player returns to a
previously visited map, the preserved state is restored, and the map appears as the
player left it.

### Indoor-Specific Preservation

For indoor maps (`DAT_006be1e0 == 1`):

- BSP node state (0x344 bytes per node, expanded runtime format)
- Door open/close state
- Face modification data
- Sector light state

### Outdoor-Specific Preservation

For outdoor maps:

- Terrain object state (0xBC bytes per entry)
- Actor positions across the terrain grid
- Decoration state changes

---

## 6. New Game Initialization

The new game function at `0x004608a7` (526 bytes):

1. Creates/copies `data\new.lod` as the working save template
2. Opens `data\new.lod` with `FUN_00461a80`
3. Sets the starting map to `"out01.odm"` (Emerald Island)
4. Loads the starting map via the normal map loading sequence
5. Sets initial party position from the `"Party Start"` marker

The starting map can be overridden via the INI setting `[outdoor] startmap=out01.odm`.

---

## 7. Lloyd's Beacon

Lloyd's Beacon is a spell that saves/restores map positions across transitions:

- Screenshot saved as `lloyd%d%d.pcx` (character index, beacon index)
- Data includes map filename, position, and facing
- On recall, triggers a full map transition to the saved location
- File paths use `data\lloyd%d%d.pcx` format

---

## 8. Special Map Transitions

Certain maps have hardcoded transition logic:

| Source           | Target          | Condition                      |
|------------------|-----------------|--------------------------------|
| `out15.odm`      | (various)       | Multiple special-case handlers |
| `d10.blv`        | (connected)     | Checked in transition function |
| `d11.blv`        | (connected)     | Checked in transition function |

The transition text system displays a message when entering certain areas. The error
`"No transition text found!"` (at `004e7c60`) indicates a missing transition text
entry.

---

## 9. Error Messages

| Error String                                | Condition                    |
|---------------------------------------------|------------------------------|
| `"Unable to find %s in Games.LOD"`          | Target map not in archive    |
| `"Out of memory loading indoor level"`      | BLV allocation failure       |
| `"Can't load file!"`                        | Invalid map file header      |
| `"No transition text found!"`               | Missing transition text      |
| `"Attempt to open new level before..."`     | Invalid state for load       |

---

## Integration notes

- Map type detection is purely extension-based (`.blv` vs `.odm`)
- The loading screen should be displayed before map parsing begins
- Map state preservation must use the same serialization format as save files
- Spawn point selection depends on the direction the party entered from
- The starting map `"out01.odm"` is a constant that can be overridden by INI
- Lloyd's Beacon requires saving both position data and a screenshot thumbnail
- Special-case map handling should be data-driven rather than hardcoded
