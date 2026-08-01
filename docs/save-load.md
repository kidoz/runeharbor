---
title: "Save/Load System"
summary: "Save files use LOD containers to persist party, map, event, and presentation state."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Save/Load System

Save files use LOD containers to persist party, map, event, and presentation state.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte file fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

> Original source file: `D:\mm7Src_eng\MM7\Code\LoadSave.cpp` (string at `004e946c`)

---

## 1. Overview

The save/load system persists the complete game state to disk using the **LOD archive format** --
the same container format used for game data archives (BITMAPS.LOD, ICONS.LOD, etc.).
Each save file is a self-contained LOD archive containing multiple named internal files
that together capture party state, NPC data, game clock, active timers, a screenshot thumbnail,
and the current state of every map the player has visited.

Key properties:

- Save files live in `saves\` relative to the game directory
- Numbered saves: `save000.mm7` through `save039.mm7` (40 slots max)
- Autosave: `saves\autosave.mm7`
- The file `data\new.lod` serves as the template -- it is copied first, then populated

---

## 2. Save File Layout

### 2.1 The .mm7 LOD Container

Each `.mm7` file is a standard LOD archive. The LOD signature is validated on load:

```text
Offset  Size  Value       Description
0x00    4     0x00016741  LOD magic number (decimal 91969)
0x04    4     "mvii"      LOD signature (bytes: 0x6D 0x76 0x69 0x69)

```

If this 8-byte header check fails, the game displays `"Can't load file!"`.

The LOD directory entry format is the same as for data archives (see [lod-archives.md](lod-archives.md)).
Each directory entry is 32 bytes: 16-byte name + offset/size/flags fields.

### 2.2 Internal Files

Each save archive contains the following named entries:

| Internal Filename  | Size (bytes) | Description                               |
|--------------------|--------------|-------------------------------------------|
| `header.bin`       | 100          | Save slot metadata (title, location, time)|
| `image.pcx`        | variable     | Screenshot thumbnail (PCX format)         |
| `party.bin`        | 90,680       | Full party + 4 characters state           |
| `clock.bin`        | 40           | Game clock / calendar state               |
| `timer.bin`        | 1,008        | Timer subsystem state (active timers)     |
| `npcdata.bin`      | 38,076       | NPC data table (all NPCs)                 |
| `npcgroup.bin`     | 102          | NPC group assignment table                |
| `lloyd%d%d.pcx`    | variable     | Lloyd's Beacon screenshot bookmarks       |
| Per-map level data | variable     | Saved state for each visited map          |

These sizes are confirmed in `WriteSaveArchive` (`FUN_0045f4a2` at `0045f4a2`, 3,087 bytes):

- `header.bin` = `0x64` (line 70965)
- `party.bin` = `0x16238` (line 70974)
- `clock.bin` = `0x28` (line 70983)
- `timer.bin` = `0x3F0` (line 70992)
- `npcdata.bin` = `0x94BC` (line 71001)
- `npcgroup.bin` = `0x66` (line 71010)

---

## 3. header.bin Structure (100 bytes)

The header stores save slot display metadata. It is read by the save/load UI to show
the slot name, location, and timestamp without loading the full save.

```cpp
Offset  Size  Type        Field           Description
0x00    20    char[20]    title           Display title (party leader name or user text)
0x14    20    char[20]    locationName    Current map/area name (e.g., "d05.blv")
0x28    4     uint32      gameTimeLow     Game time, low 32 bits (ticks)
0x2C    4     uint32      gameTimeHigh    Game time, high 32 bits (ticks)
0x30    52    byte[52]    reserved        Remaining header space (padding/unused)

```

**Total: 100 bytes (0x64)**

Evidence:

- In `QuickSave` (`FUN_004600b1`), the location name is copied to offset +0x14
  (`&DAT_0069bbfc` = `DAT_0069bbe8 + 0x14`) from the current map name at `DAT_006be1c4`.
- Game time is stored at +0x28 and +0x2C (`DAT_0069bc10`, `DAT_0069bc14`) from
  the global game time at `DAT_00acce64`/`DAT_00acce68`.
- The save slot table stores 45 header entries (40 numbered + autosave + margin),
  each 100 bytes, at `DAT_0069bbe8`.

### 3.1 Save Slot Table

The in-memory slot table uses parallel arrays:

| Global Address  | Type              | Description                          |
|-----------------|-------------------|--------------------------------------|
| `DAT_0069bbe8`  | `char[45][100]`   | Header data per slot (header.bin)    |
| `DAT_0069ce74`  | `char[45][280]`   | Filename per slot (e.g., "save001.mm7") |
| `DAT_0069cda8`  | `uint32[45]`      | Validity flags (0=empty, 1=valid)    |
| `DAT_0069cda4`  | `int`             | Total number of valid slots found    |

Slot entry size: `0x118` = 280 bytes for filename storage.

---

## 4. party.bin Structure (90,680 bytes)

The `party.bin` file is a flat memory dump of the Party structure followed by four
Character structures. It captures the complete player-side game state.

**Total size: 0x16238 = 90,680 bytes**

### 4.1 Layout

```text
Offset      Size      Description
0x0000      varies    Party header (position, gold, food, alignment, etc.)
...         ...       Party-level flags, quest states, autonotes
...         4+4       Game time copy (gameTimeLow + gameTimeHigh at DAT_00acce64..68)
...         varies    Party inventory (shared stash items)
...         varies    Saved position for respawn (posX/Y/Z, facing, pitch)
0x????      28,016    Character 0 data (0x1B3C = 7,004 bytes)
+0x1B3C     28,016    Character 1 data
+0x36F8     28,016    Character 2 data
+0x5234     28,016    Character 3 data

```

The party base address is `DAT_00acce38`. Character data begins at `DAT_00acd804`
and spans to `DAT_00ad44f4` (4 characters x 0x1B3C bytes each).

### 4.2 Party Fields (at DAT_00acce38)

| Offset    | Size  | Field              | Description                            |
|-----------|-------|--------------------|----------------------------------------|
| +0x04     | 4     | height             | Party collision height (default 192)   |
| +0x08     | 4     | currentHeight      | Current effective height                |
| +0x0C     | 4     | eyeLevel           | Eye/camera height (default 160)        |
| +0x10     | 4     | currentEyeLevel    | Current effective eye level             |
| +0x2C     | 4+4   | gameTimeLow/High   | 64-bit game time in ticks              |
| ...       |       |                    | (Additional party fields)              |
| +0x6B4    | 4     | posX               | World X position (at DAT_00acd4ec)     |
| +0x6B8    | 4     | posY               | World Y position                       |
| +0x6BC    | 4     | posZ               | World Z position                       |
| +0x6C0    | 4     | facing             | Yaw angle (0-2047)                     |
| +0x6C4    | 4     | pitch              | Pitch angle                            |
| +0x6C8    | 4     | savedPosX          | Respawn/recall X                       |
| +0x6CC    | 4     | savedPosY          | Respawn/recall Y                       |
| +0x6D0    | 4     | savedPosZ          | Respawn/recall Z                       |
| +0x6D4    | 4     | savedFacing        | Respawn/recall yaw                     |
| +0x6D8    | 4     | savedPitch         | Respawn/recall pitch                   |
| +0x700    | 4     | posZ_saved         | Copy of posZ (at DAT_00acd538)         |

Note: The exact layout of the party header between offsets 0x00 and the character array
is not fully mapped. The offsets above are computed from known global addresses
(`DAT_00acd4ec - DAT_00acce38 = 0x6B4` for posX, etc.).

### 4.3 Character Structure (7,004 bytes each)

Each character occupies **0x1B3C = 7,004 bytes**. Four characters are stored consecutively.

| Offset  | Size    | Field           | Description                          |
|---------|---------|-----------------|--------------------------------------|
| +0x0000 | 16      | name            | Character name (null-terminated)     |
| +0x0011 | 1       | classId         | Class index                          |
| +0x0164 | varies  | skills          | Skill values (uint16 per skill)      |
| +0x01F0 | varies  | inventory       | Inventory item array (36B per item)  |
| +0x0604 | 4       | currentHP       | Current hit points                   |
| +0x0608 | 4       | currentSP       | Current spell points                 |
| +0x0838 | 4       | statHP?         | HP stat or max value                 |
| +0x083C | 4       | statMaxHP?      | Max HP or related                    |
| +0x0840 | 4       | statSP?         | SP stat or max value                 |
| +0x09CC | varies  | spellbook       | Known spells / conditions            |
| +0x1920 | 4       | voiceId         | Voice/portrait set index             |

Each inventory item is **36 bytes** (0x24), accessed at `+0x1F0 + itemIndex * 0x24`.

### 4.4 Save Position Swap

During save, the current position is temporarily replaced with the "saved position"
(respawn point), the save is written, then the original position is restored.
This ensures that loading a save places the party at the designated respawn location
rather than wherever they happened to be during the save operation.

```text
Before write:
  backup current pos (posX/Y/Z, facing, pitch)
  posX/Y/Z = savedPosX/Y/Z
  facing = savedFacing
  pitch = savedPitch

After write:
  posX/Y/Z = backup pos
  facing = backup facing
  pitch = backup pitch

```

This is visible in `WriteSaveArchive` at lines 70920-70930 and 71202-71207.

---

## 5. clock.bin Structure (40 bytes)

The clock stores the game's time state. The primary value is a 64-bit tick counter.

**Total size: 0x28 = 40 bytes**

```cpp
Offset  Size  Type      Field           Description
0x00    4     uint32    gameTimeLow     Low 32 bits of game time (ticks)
0x04    4     uint32    gameTimeHigh    High 32 bits of game time (ticks)
0x08    32    byte[32]  timerState      Additional timer/calendar state

```

### 5.1 Time Conversion

The game time is stored as a 64-bit integer counting **ticks** at **128 ticks per second**
(0x80). The display function (`FUN_004601f6`) converts ticks to calendar values using this
chain (the tick-to-seconds division happens via FPU before `__ftol()`):

```text
seconds = ticks / 128        (0x80)
minutes = seconds / 60       (0x3C)
hours   = minutes / 60       (0x3C)
days    = hours / 24         (0x18)
weeks   = days / 7           (7-day weeks)
months  = weeks / 4          (4-week months)
years   = months / 12        (12-month years)
year    = years + 0x490      (base year 1168)

```

The reconstruction path (`__allmul(total_seconds, 0x80, 0)`) confirms 128 ticks/second.

Display format string at `004e95e4`: `"%s %d:%02d%s\n%d %s %d"`

- Parameters: dayName, hour, minute, amPm, dayOfMonth, monthName, year

The base year **0x490 = 1168** is added to the computed year count. Hours use 12-hour
format with AM/PM conversion (hour 0 displays as 12, hours 13+ subtract 12).

When saving, the game time is also copied into the map's time variable:

- Indoor maps: `DAT_006be534` (map variables, 0x38 bytes)
- Outdoor maps: `DAT_006a1160`

---

## 6. timer.bin Structure (1,008 bytes)

The timer subsystem manages scheduled game events (spell durations, buff expiry,
quest timers, etc.).

**Total size: 0x3F0 = 1,008 bytes**

The timer state is saved from the global at `DAT_0050ba60`. The exact internal layout
of individual timer entries is not fully determined, but the fixed 1,008-byte size
suggests a fixed-capacity array of timer records.

Estimated layout (speculative):

```text
Offset  Size   Description
0x00    4      Active timer count
0x04    varies Timer entries array (fixed capacity)

```

Each timer entry likely contains:

- Event type / callback identifier
- Expiration time (64-bit game tick)
- Associated entity (character index, spell ID, etc.)
- Flags (repeating, one-shot, etc.)

---

## 7. npcdata.bin Structure (38,076 bytes)

The NPC data table stores the state of all NPCs in the game -- hire status, dialogue
state, current location, and other per-NPC variables.

**Total size: 0x94BC = 38,076 bytes**

The data is saved from global address `DAT_0072d50c`. After loading, `FUN_00476c64()`
is called to rebuild NPC runtime indices.

The NPC hire/state data is initialized from `npcdata.txt` and `npcgroup.txt`
(text data files parsed by `FUN_00476cb9`).

### 7.1 NPC Structure (76 bytes)

From cross-reference with NPC dialogue documentation, each NPC record is **76 bytes**.
With 38,076 / 76 = **501 NPC entries** in the table.

### 7.2 npcgroup.bin (102 bytes)

The NPC group assignment table at `DAT_0073bfaa`. Maps NPCs to their group/faction.
With 102 bytes, this likely encodes group IDs for NPC subsets (102 / 2 = 51 uint16 entries,
or 102 single-byte group IDs).

---

## 8. Per-Map Level Data

When saving, the game iterates through all maps the player has visited and writes
their current state into the LOD archive. This preserves monster positions, opened
chests, triggered events, and other per-map modifications.

### 8.1 Map File Naming

Level data files within the save use the original map filename (e.g., `d05.blv`,
`out01.odm`). The save system iterates through maps in two nested loops:

```text
for mapType = 1..4:       (4 map categories)
  for mapIndex = 1..5:    (5 maps per category)
    filename = formatted map name
    if file exists on disk:
      compress and write to LOD

```

This is visible in the nested loops at lines 71018-71045 (outer limit 5, inner limit 6).

### 8.2 Indoor Map State (BLV)

For indoor maps (`DAT_006be1e0 == 1`), the saved data includes:

| Data                  | Size per entry | Count source      | Description                |
|-----------------------|----------------|-------------------|----------------------------|
| Face visibility flags | 0x60 each      | `DAT_006be4c0`    | Face rendering states      |
| Decoration states     | 0x20 each      | `DAT_0069ac50`    | Decoration on/off flags    |
| BSP node data         | 0x344 each     | `DAT_006650a8`    | BSP tree node array        |
| Sprite objects        | 0x70 each      | `DAT_006650ac`    | Sprite object states (112B)|
| Actors/Monsters       | 0x14CC each    | `DAT_005fefc0`    | Actor states (5,324B)      |
| Chest contents        | 200 bytes      | 1                 | Chest item data            |
| Map variables         | 0x38 bytes     | 1                 | Map-specific variables     |

Additional fixed-size blocks are written between the variable-count arrays.

### 8.3 Outdoor Map State (ODM)

For outdoor maps (`DAT_006be1e0 != 1`), a similar set is saved:

| Data                  | Size per entry | Count source      | Description                |
|-----------------------|----------------|-------------------|----------------------------|
| Terrain tile data     | 0x134 each     | per-tile count    | Tile model visibility      |
| Decoration states     | 0x20 each      | `DAT_0069ac50`    | Decoration on/off flags    |
| BSP node data         | 0x344 each     | `DAT_006650a8`    | BSP tree nodes             |
| Sprite objects        | 0x70 each      | `DAT_006650ac`    | Sprite object states       |
| Actors/Monsters       | 0x14CC each    | `DAT_005fefc0`    | Actor states               |
| Chest contents        | 200 bytes      | 1                 | Chest item data            |
| Map variables         | 0x38 bytes     | 1                 | Map-specific variables     |

Outdoor tile data includes an additional summary block at offset +0x3F0:

- 0x3C8 bytes of outdoor-specific data at `DAT_006a1560`
- Tile count fields at `DAT_006a1148`, `DAT_006a1150`, `DAT_006a114c`

### 8.4 Level Data Compression

Each map's state is serialized into the 1,000,000-byte write buffer
(`operator_new(1000000)`) and then written into the LOD archive via `FUN_00461b85`
(LOD file append). The LOD archive may apply zlib compression to individual entries.

---

## 9. Save/Load Procedure

### 9.1 Save Sequence (`DoSaveGame` -- `FUN_0045eec3` at `0045eec3`)

1. **Validate slot**: Check `(&DAT_0069cda8)[slotIndex]` for existing data.
   If slot is empty, display confirmation prompt via `FUN_004aa29b`.

2. **Clean up level data**: Delete existing per-map files from the working LOD
   (nested loop: 4 categories x 5 maps, calling `FUN_004ccc14` to delete each).

3. **Prepare character audio**: For each of 4 characters, stop any playing voice
   sounds (voice ID at character + 0x1920, sound indices calculated as
   `voiceId * 0x32 + offset`).

4. **Copy save to working file**: `CopyFileA(saves\slotFile, data\new.lod, 0)` --
   copies the save file to the working LOD location.

5. **Open LOD for writing**: `FUN_00461a80()` opens the working LOD archive.

6. **Read existing header**: `FUN_004615bd("header.bin", 1)` loads current header
   if present, then decompresses into the slot's header buffer.

7. **Write state files**: Call `WriteSaveArchive` (`FUN_0045f4a2`) which serializes:
   - `header.bin` (100 bytes from `DAT_0069bbe8 + slot * 100`)
   - `party.bin` (90,680 bytes from `DAT_00acce38` region)
   - `clock.bin` (40 bytes from game time globals)
   - `timer.bin` (1,008 bytes from `DAT_0050ba60`)
   - `npcdata.bin` (38,076 bytes from `DAT_0072d50c`)
   - `npcgroup.bin` (102 bytes from `DAT_0073bfaa`)
   - Per-map level data for all visited maps
   - Lloyd's Beacon PCX screenshots (`lloyd%d%d.pcx`)

8. **Play save sound**: If in save mode (`DAT_004e28d8 == 0xB`), plays a UI sound.

9. **Copy autosave**: `CopyFileA("data\new.lod", "saves\autosave.mm7", 0)` --
   the working LOD always becomes the autosave.

10. **Restore state**: Return `DAT_004e28d8 = 0` (normal gameplay),
    set `DAT_00576eac = 1` (redraw flag).

### 9.2 Load Sequence (`InitNewGameFromSave` -- `FUN_004608a7` at `004608a7`)

1. **Stop video**: If video is playing, call `FUN_004beb3a()` (VideoPlayer::StopAll).

2. **Allocate buffer**: `FUN_004266fe(0, 1000000, 0)` -- allocate 1MB working buffer.

3. **Close current LOD**: `FUN_00461f26()` closes any open LOD.

4. **Delete working LOD contents**: `FUN_004ccc14("data\new.lod")` clears the file.

5. **Initialize fresh LOD**: `FUN_004617f3()` sets up a new LOD with:
   - Description: `"MMVII"` (at `004e9654`)
   - Sub-description: `"newmaps for MMVII"` (at `004e9640`)
   - Max entries: 100, base offset: 0

6. **Copy save entries**: For each entry in the second half of the save's directory,
   read the data via `FUN_004615bd`, then write to the working LOD via `FUN_00461fae`.
   This transfers all save data into `data\new.lod`.

7. **Set default header**: Write `"out01.odm"` as the starting map name in the
   header's location field. Write a fresh `header.bin` (100 bytes) to the LOD.

8. **Set spawn position**: Default spawn at X=0x3108 (12,552), Y=0x718 (1,816),
   Z=0, facing=0x200 (512), pitch=0.

9. **Write save archive**: Call `WriteSaveArchive` to finalize the working LOD.

10. **Clean up**: Free the working buffer.

### 9.3 WriteSaveArchive Detail (`FUN_0045f4a2` at `0045f4a2`)

The core serialization function:

1. Allocate 1,000,000-byte write buffer.
2. Close any open LOD handles.
3. **Swap positions**: Save current party position, replace with saved/respawn position.
4. **Save map time**: Copy game time into the current map's time variable
   (indoor: `DAT_006be534`, outdoor: `DAT_006a1160`).
5. **Capture screenshot**: Call `FUN_0045e073()` to render and compress `image.pcx`.
6. Write each named file to the LOD:
   - `header.bin` (100 bytes)
   - `party.bin` (90,680 bytes)
   - `clock.bin` (40 bytes)
   - `timer.bin` (1,008 bytes)
   - `npcdata.bin` (38,076 bytes)
   - `npcgroup.bin` (102 bytes)
7. **Iterate visited maps**: For each map (4 categories x 5 indices), check if the map
   file exists. If so, read it, compress, and write to the LOD.
8. **Write level blob**: If this is not a header-only save (`param_2 == NULL`):
   - Write LOD magic (`0x16741` + `"mvii"`)
   - Serialize current map state (indoor or outdoor) into the buffer
   - Write the blob to the LOD archive
9. **Copy to autosave**: `CopyFileA("data\new.lod", "saves\autosave.mm7", 0)`.
10. **Restore positions**: Put original party position back.

---

## 10. Autosave

### 10.1 When It Triggers

The autosave is updated every time `WriteSaveArchive` completes a full save. After
writing all data to the working LOD (`data\new.lod`), the function copies it:

```text
CopyFileA("data\new.lod", "saves\autosave.mm7", FALSE)

```

This means the autosave reflects the most recent save operation, not a separate
periodic trigger. The autosave is always a copy of the last explicit save.

### 10.2 Autosave Slot

The autosave file is `saves\autosave.mm7`. When enumerating save slots
(`FUN_0045e2d0`), if `param_1 == 0`, the autosave is included in the slot list
as the first entry. If `param_1 != 0`, it is skipped (save mode excludes autosave
from the writable slot list).

### 10.3 QuickSave (`FUN_004600b1`)

QuickSave writes to a specific numbered slot (`saves\save%03d.mm7`):

1. Check map is not the default (`d05.blv` comparison).
2. Call `WriteSaveArchive` to serialize all state.
3. Update the slot header: copy current map name and game time.
4. Write `header.bin` to the LOD.
5. Copy `data\new.lod` to `saves\save%03d.mm7`.
6. Return to normal gameplay (`DAT_004e28d8 = 0`).

---

## 11. Save/Load UI

### 11.1 Screen Mode

The save/load screens use `DAT_004e28d8` (CurrentScreenMode):

- **0x0B (11)**: Save game screen
- **0x0C (12)**: Load game screen

### 11.2 UI Textures

Both screens load from ICONS.LOD:

- `loadsave` -- background panel
- `load_up` / `save_up` -- tab buttons (unselected)
- `LS_loadU` / `LS_saveU` -- tab buttons (unselected variant)
- `LS_loadD` / `LS_saveD` -- tab buttons (selected/pressed)
- `AR_UP_DN` / `AR_DN_DN` -- scroll arrows
- `lsave640.pcx` -- full background (640x480)

### 11.3 Slot Display

The UI displays **7 visible slots** at a time with vertical scrolling:

| Slot | Y Position |
|------|------------|
| 0    | 198 (0xC6) |
| 1    | 219 (0xDB) |
| 2    | 240 (0xF0) |
| 3    | 261 (0x105)|
| 4    | 282 (0x11A)|
| 5    | 303 (0x12F)|
| 6    | 324 (0x144)|

Spacing: 21 pixels between slots.

Each slot displays:

- The header title text (from `header.bin`)
- The screenshot thumbnail (from `image.pcx`)
- The formatted date/time string

### 11.4 UI Button Messages

| Message ID | Action              |
|------------|---------------------|
| 0xA2       | Scroll up           |
| 0xA3       | Scroll down         |
| 0xA4       | Confirm (Load/Save) |
| 0xA5       | Select slot (param = slot 0-6) |
| 0xA6       | Cancel              |
| 0xA7       | Delete save         |

---

## 12. Key Addresses

### Functions

| Address      | Name (inferred)       | Size   | Description                          |
|--------------|-----------------------|--------|--------------------------------------|
| `0045e2d0`   | EnumerateSaveSlots    | 202    | Scan saves\ for .mm7 files          |
| `0045e39a`   | InitLoadGameScreen    | 1,501  | Set up Load Game UI                  |
| `0045e977`   | InitSaveGameScreen    | 1,356  | Set up Save Game UI                  |
| `0045eec3`   | DoSaveGame            | 1,503  | Perform save to selected slot        |
| `0045f4a2`   | WriteSaveArchive      | 3,087  | Serialize all state to LOD           |
| `004600b1`   | QuickSave             | 325    | Quick save to numbered slot          |
| `004601f6`   | RenderSaveLoadScreen  | 1,344  | Draw save/load slot list             |
| `004608a7`   | InitNewGameFromSave   | 526    | Load a save into data\new.lod       |

### Globals

| Address      | Type         | Description                              |
|--------------|--------------|------------------------------------------|
| `DAT_0069bbe8` | char[45][100] | Save slot headers (header.bin contents)|
| `DAT_0069ce74` | char[45][280] | Save slot filenames                    |
| `DAT_0069cda4` | i32          | Number of valid save slots             |
| `DAT_0069cda8` | uint32[45]   | Slot validity flags                    |
| `DAT_006a0b20` | i32          | Currently selected slot index          |
| `DAT_006a0b1c` | i32          | Scroll offset (first visible slot)     |
| `DAT_00acce38` | Party        | Party structure base                   |
| `DAT_00acce64` | u32       | Game time low word                     |
| `DAT_00acce68` | u32       | Game time high word                    |
| `DAT_00acd804` | Player[4]    | Character array base                   |
| `DAT_0050ba60` | byte[1008]   | Timer subsystem state                  |
| `DAT_0072d50c` | byte[38076]  | NPC data table                         |
| `DAT_0073bfaa` | byte[102]    | NPC group table                        |
| `DAT_006be1e0` | i32          | Map type (1=indoor/BLV, other=outdoor) |

### Strings

| Address    | String                           | Referenced by                   |
|------------|----------------------------------|---------------------------------|
| `004e9494` | `save%03d.mm7`                   | EnumerateSaveSlots              |
| `004e94a4` | `saves`                          | EnumerateSaveSlots              |
| `004e94ec` | `header.bin`                     | Multiple save/load functions    |
| `004e94e0` | `image.pcx`                      | InitLoad/SaveGameScreen, Write  |
| `004e9598` | `party.bin`                      | DoSaveGame, WriteSaveArchive    |
| `004e958c` | `clock.bin`                      | DoSaveGame, WriteSaveArchive    |
| `004e9574` | `npcdata.bin`                    | DoSaveGame, WriteSaveArchive    |
| `004e9564` | `npcgroup.bin`                   | DoSaveGame, WriteSaveArchive    |
| `004e95a4` | `data\new.lod`                   | Template LOD for saves          |
| `004e95b4` | `saves\autosave.mm7`             | Autosave destination            |
| `004e95d0` | `saves\save%03d.mm7`             | QuickSave destination           |
| `004e95e4` | `%s %d:%02d%s\n%d %s %d`         | Date/time display format        |
| `004e267c` | `data\lloyd%d%d.pcx`             | Lloyd's Beacon screenshots      |
| `004ec210` | `Can't load file!`               | LOD validation failure          |

---

## Integration notes

### 13.1 For RuneHarbor

- The save format reuses the LOD archive reader/writer -- no separate format needed.
- `party.bin` is the largest and most complex file; serialize the Party and Player
  structs in the same order and at the same offsets as the original.
- The position swap during save ensures loaded games start at the correct respawn point.
- The 1MB write buffer can be replaced with streaming writes in a modern implementation.
- Consider adding save format versioning (the original has none).

### 13.2 Compatibility

To load original MM7 save files:

1. Open the `.mm7` as a LOD archive.
2. Validate the 8-byte LOD header (`0x16741` + `"mvii"`).
3. Extract each internal file by name.
4. Deserialize fixed-size binary blobs at the documented sizes.
5. Per-map level data must be matched by filename to the correct map loader.
