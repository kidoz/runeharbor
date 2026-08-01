---
title: "NPC Dialogue"
summary: "The NPC system loads character records and presents dialogue, hiring, and profession effects."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# NPC Dialogue

The NPC system loads character records and presents dialogue, hiring, and profession effects.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

The NPC system manages 500 non-player characters with associated names, professions,
dialogue topics, greeting texts, news texts, and per-area distribution probabilities.
NPCs can be interacted with through a 2D dialog interface that displays a portrait,
greeting text, and up to four response buttons. Some NPCs can be hired to join the
party, providing passive skill bonuses based on their profession.

The system is loaded at startup from a suite of tab-separated text files and stored
in a large manager object. At runtime, NPC state is persisted in `npcdata.bin` and
`npcgroup.bin` within save files.

Source references:

- `D:\mm7Src_eng\MM7\Code\Events.cpp` (dialog trigger opcodes)
- NPC data files: `npcdata.txt`, `npcnames.txt`, `npcprof.txt`, `npctext.txt`,
  `npctopic.txt`, `npcgreet.txt`, `npcgroup.txt`, `npcnews.txt`, `npcdist.txt`

---

## Data Structures

### NPC Entry (76 bytes, 0x13 dword-aligned fields)

Each NPC occupies 76 bytes in the runtime table. There are 500 NPC entries
(`0x1F5`), stored at an offset of `+0x17FC4` within the NPC manager object.

Parsed from `npcdata.txt` with the following field mapping:

```cpp
struct NPCEntry {              // 76 bytes total
    // Negative offsets from the base pointer are common in Ghidra output
    // due to how the parser iterates; logical layout:

    char*    name;             // Field 0:  Pointer to name string
    int32_t  flags;            // Field 1:  NPC flags / behavioral bits
    int32_t  reserved_2;      //           (fields 2-4 parsed but usage unclear)
    int32_t  reserved_3;
    int32_t  reserved_4;
    int32_t  professionId;    // Field 5:  Index into profession table (0-58)
    int32_t  topic1;          // Field 6:  First dialogue topic ID
    int32_t  topic2;          // Field 7:  Second dialogue topic ID
    int32_t  joinsParty;      // Field 8:  'y' or 'n' -> 1 or 0 (hireable?)
    int32_t  padding_9;       //           (4 bytes gap before greeting)
    int32_t  greetingGroup;   // Field 9:  Index into greeting text table (0-204)
    int32_t  dialogAction1;   // Field 10: Response button 1 event ID
    int32_t  dialogAction2;   // Field 11: Response button 2 event ID
    int32_t  dialogAction3;   // Field 12: Response button 3 event ID
    int32_t  dialogAction4;   // Field 13: Response button 4 event ID
    int32_t  dialogAction5;   // Field 14: Response button 5 event ID
};

```

### NPC Name Entry (8 bytes)

Names are stored separately from NPC entries, in a name pool beginning at
offset `+0x12978` in the NPC manager. Up to 540 entries (0x21C).

```cpp
struct NPCName {               // 8 bytes
    char*    firstName;        // +0x00: Pointer to first name string
    char*    lastName;         // +0x04: Pointer to last name string
};

```

The male/female name split count is tracked at `+0x17FD4` in the manager.

At runtime, the NPC name string pool is referenced via `DAT_00f79be0`, used
for display in dialogue windows and the party roster.

### NPC Greeting Entry (8 bytes)

Greetings are stored at offset `+0x1788C` in the NPC manager. There are 205
entries (0xCD), each with two variant texts.

```cpp
struct NPCGreeting {           // 8 bytes
    char*    greeting1;        // +0x00: Primary greeting text
    char*    greeting2;        // +0x04: Alternate greeting text
};

```

The greeting group field in the NPC entry selects which pair to use. The
variant chosen depends on context (first meeting vs. subsequent, or
alignment-based selection).

### NPC Profession Entry (20 bytes)

Professions are stored at offset `+0x13A78` in the NPC manager. There are
59 entries (0x3B), loaded from `npcprof.txt`.

```cpp
struct NPCProfession {         // 20 bytes
    int32_t  cost;             // Field 2: Hiring cost (gold)
    char*    benefitText;      // Field 3: Description of passive benefit
    char*    joinText;         // Field 4: Text when hired ("I'll join you...")
    char*    actionText;       // Field 5: Text for profession-specific action
    char*    dismissText;      // Field 6: Text when dismissed
};

```

Professions provide passive bonuses to the party while the NPC is hired.
Examples from string references include roles like scouts, healers, merchants,
and teachers.

### NPC Text/Topic Arrays

Two large arrays store dialogue text and topic text, indexed by ID:

**Topic text** (`npctopic.txt`):

- Base address: `DAT_007214E8`
- Entry size: 8 bytes (4 unused + 4 text pointer)
- Range: up to `0x722700` (~2,308 entries)

**Dialog text** (`npctext.txt`):

- Base address: `DAT_007214EC`
- Entry size: 8 bytes (4 unused + 4 text pointer)
- Range: up to `0x722D94` (~3,120 entries)

### NPC Distribution Table

The distribution table (`npcdist.txt`) controls NPC spawn probability per area:

```text
Dimensions: 78 columns (areas/zones) x 59 rows (professions)
Cell type:  byte (0-255), representing spawn probability weight
Row totals: precomputed at manager offset +0x16544 (stride 0x40 = 64 bytes)

```

When generating wandering NPCs for a zone, the engine samples from this table.
Each column represents a map area, each row a profession type. The probability
weight determines how likely that profession is to appear in that area.

### NPC Group Table

Group assignments (`npcgroup.txt`) associate NPCs with behavioral groups.
There are 51 groups (0x33). Saved/loaded as `npcgroup.bin` (0x66 = 102 bytes)
in save files.

### NPC News Table

News text by NPC (`npcnews.txt`): 51 entries (0x33). Provides area-specific
rumor text that NPCs can share during dialogue.

---

## Key Algorithms

### NPC Data Loading

The master loader is `FUN_00477033` (NPCManager_LoadAll, 567 bytes). It calls
sub-loaders in sequence:

1. `FUN_00477033` loads `npcnames.txt` and `npcprof.txt`
2. `FUN_00476cb9` loads `npcdata.txt`, `npcgreet.txt`, `npcgroup.txt`, `npcnews.txt`
3. `FUN_0047697b` loads `npctext.txt`, `npctopic.txt`, `npcdist.txt`

Additional data files loaded by related functions:

- `FUN_00476594` loads `merchant.txt` (merchant inventory data)
- `FUN_00476754` loads `teacher` data (training/skill teaching)

All text files are tab-separated values. The parser (`FUN_004cbb55` / `FUN_004cc17b`)
tokenizes lines and fills fields sequentially. The "joins party" field converts
`'y'`/`'n'` characters to integer 1/0.

### Dialogue Flow

The dialogue system follows this sequence:

1. **Trigger**: Player clicks on an NPC (or an event trigger fires opcode 0x02).

2. **Dialog creation**: Event processor (opcode `EVT_SHOW_NPC_DIALOG`, 0x02) creates
   a dialog window via `FUN_0041c3db`. The game screen mode `DAT_004e28d8` is set
   to `0x04` (NPC dialogue/interaction popup).

3. **Portrait display**: NPC portrait loaded using the format `NPC%03d` (string at
   `004e2d58`), e.g., `NPC001`, `NPC042`. The portrait is retrieved from the icons
   archive and displayed in the dialog panel.

4. **Greeting text**: The NPC's `greetingGroup` field indexes into the greeting table.
   One of two greeting variants is selected and displayed.

5. **Topic buttons**: Up to 4 response buttons are created based on the NPC's
   `dialogAction1`--`dialogAction4` fields. Button message ID is `0x71` (NPC
   dialogue option). Each button maps to a sub-event ID.

6. **Player response**: Clicking a button triggers the corresponding event script.
   Text display opcodes (`EVT_SHOW_TEXT` 0x1A, `EVT_SET_NPC_NAME` 0x1E,
   `EVT_SHOW_MESSAGE` 0x21) present the NPC's speech from the string tables.

7. **Topic navigation**: Event opcode `0x2F` (`EVT_SET_NPC_TOPIC`) can dynamically
   change an NPC's available topics, enabling branching conversation trees.

8. **Dialog close**: When the player exits, `FUN_004ab69f` closes the dialog and
   restores the previous screen mode from `DAT_005067f8`.

### NPC Dialog UI (2D Talk Interface)

The dialog window layout varies by party alignment (good/neutral/evil), loading
different frame textures:

| Alignment | Left Panel     | Right Panel    | Background    |
|-----------|---------------|----------------|---------------|
| Good      | `IB-NPCLD-B`  | `IB-NPCRD-B`  | `evtnpc-b`   |
| Neutral   | `IB-NPCLD-A`  | `IB-NPCRD-A`  | `evtnpc`     |
| Evil      | `IB-NPCLD-C`  | `IB-NPCRD-C`  | `evtnpc-c`   |

Additional UI elements: `IB-NPCLD-a`/`IB-NPCRD-a` (lowercase variant textures)
are also loaded, suggesting state-dependent rendering (hover/pressed).

These textures are loaded by `FUN_00422698` (4,628 bytes), which is the main
interface rendering function responsible for the entire game HUD.

### NPC Portrait Resolution

Two functions handle NPC ID validation and portrait lookup:

- `FUN_00445a1c` (307 bytes): Validates NPC ID, shows `"NPC id exceeds MAX_DATA!"`
  if out of range. Calls `FUN_004ca62e`, `FUN_0040e2d4`, `FUN_00466d0d` for
  error handling.

- `FUN_00445b4f` (351 bytes): Same validation, called from different code paths
  (portrait rendering vs. event processing).

Portrait sprites use the format string `npc%03u` (at `004e7cc8`) for runtime
lookup, and `NPC%03d` (at `004e2d58`) for icon loading.

### NPC Dialog Initialization (FUN_00445d6d)

`FUN_00445d6d` (755 bytes) sets up an NPC conversation:

1. Loads NPC portrait via `FUN_0040fb2c` using `npc%03u` format
2. Creates event-specific textures via `evt%02d` format
3. Builds the dialog window (`FUN_0041c3db`)
4. Creates response buttons (`FUN_0041d0d8`)
5. Validates NPC ID via `FUN_00445a1c`
6. Calls `FUN_004ab69f` to close any existing dialogs first
7. Sets voice/sound via `FUN_004948a9` and visual indicator via `FUN_004262f2`

### Hiring Mechanics

When an NPC's `joinsParty` field is set to 1 (parsed from `'y'` in `npcdata.txt`):

1. The dialog presents a "hire" option among the response buttons
2. Hiring costs the amount specified in the NPC's profession entry (`cost` field)
3. Gold is deducted from the party
4. The NPC is added to the party's hired NPC roster
5. While hired, the NPC provides a passive bonus determined by their profession
6. The NPC can be dismissed, triggering the profession's `dismissText`
7. Hired NPC state is persisted in `npcdata.bin` within save files

The party can have a limited number of hired NPCs (tracked in the party state).
Attempting to hire beyond the limit shows a refusal message.

### NPC Distribution / Spawning

When the player enters a zone, wandering NPCs are generated:

1. Zone index identifies the column in the distribution table
2. For each profession row, the spawn weight is checked
3. A weighted random selection determines which professions appear
4. Selected NPCs are assigned random names from the name pool
   (respecting male/female split at `+0x17FD4`)
5. NPCs are placed at valid positions in the zone

### NPC Modification via Events

Several event opcodes modify NPC state at runtime:

| Opcode | Name                  | Effect                              |
|--------|-----------------------|-------------------------------------|
| 0x02   | `EVT_SHOW_NPC_DIALOG` | Opens NPC dialog (reads string index at +5) |
| 0x13   | `EVT_MODIFY_NPC`      | Modify NPC (6 params: flags, position, etc.) |
| 0x15   | `EVT_MODIFY_NPC_EX`   | Extended NPC modification (7 params)  |
| 0x1D   | `EVT_SET_NPC_PORTRAIT` | Change NPC portrait (indoor/outdoor differs) |
| 0x1E   | `EVT_SET_NPC_NAME`    | Change NPC display name               |
| 0x2F   | `EVT_SET_NPC_TOPIC`   | Set/change NPC dialogue topic          |

---

## Constants & Enums

### NPC System Limits

| Constant    | Value | Description                            |
|-------------|-------|----------------------------------------|
| Max NPCs    | 500   | Total NPC entry slots (0x1F5)          |
| Max Names   | 540   | Name pool entries (0x21C)              |
| Max Greetings | 205 | Greeting text pairs (0xCD)             |
| Professions | 59    | Profession types (0x3B)                |
| Dist Areas  | 78    | Distribution columns (0x4E)            |
| Groups      | 51    | NPC group assignments (0x33)           |
| News Entries| 51    | News text entries (0x33)               |
| NPC Entry Size | 76 | Bytes per NPC data record              |
| Name Entry Size | 8 | Bytes per name record                  |
| Greeting Entry Size | 8 | Bytes per greeting record          |
| Profession Entry Size | 20 | Bytes per profession record      |

### NPC Save Data Sizes

| File          | Size (hex) | Size (dec) | Description           |
|---------------|------------|------------|-----------------------|
| `npcdata.bin` | 0x94BC     | 38,076     | All 500 NPC entries   |
| `npcgroup.bin`| 0x66       | 102        | Group assignments     |

Verification: 500 NPCs x 76 bytes = 38,000 bytes. The extra 76 bytes (38,076 - 38,000)
likely include a header or count prefix.

### Screen Mode

| Value | Mode                      |
|-------|---------------------------|
| 0x04  | NPC dialogue / interaction popup |

### Message IDs

| ID   | Purpose                       |
|------|-------------------------------|
| 0x71 | NPC dialogue option button    |

### NPC Sprite Format Strings

| Address    | Format     | Usage                        |
|------------|-----------|------------------------------|
| `004e2d58` | `NPC%03d` | Icon/portrait loading        |
| `004e7cc8` | `npc%03u` | Runtime sprite reference     |

---

## Integration Points

### Event System

The NPC dialog system is tightly coupled with the event engine:

- **Trigger**: Event opcode 0x02 initiates NPC dialog
- **Topics**: Opcode 0x2F dynamically sets NPC conversation topics
- **Text display**: Opcodes 0x1A, 0x1E, 0x21 show dialogue text
- **NPC modification**: Opcodes 0x13, 0x15, 0x1D modify NPC properties at runtime
- **Dialog responses**: Each response button fires a sub-event with the action ID
  from the NPC entry's `dialogAction1`--`dialogAction5` fields

See [event-engine.md](event-engine.md) for the full event opcode reference.

### Save/Load System

- `npcdata.bin` (38,076 bytes) serializes all 500 NPC entries
- `npcgroup.bin` (102 bytes) serializes group assignments
- Both are written by `FUN_0045f4a2` (WriteSaveArchive) and read during load
- NPC state changes (modified topics, names, portraits) persist across saves

### Party System

- Hired NPCs consume party roster slots
- Hiring deducts gold from the party
- Profession bonuses affect party-wide gameplay (combat, travel, etc.)
- Current NPC indices tracked at `DAT_00f8b068` (topic/portrait ID) and
  `DAT_00590FE0` (profession index)

### UI System

- Dialog frame textures vary by party alignment (good/neutral/evil)
- The dialog window is a modal overlay (screen mode 0x04)
- Previous screen mode saved to `DAT_005067f8` for restoration
- NPC portraits are loaded from ICONS.LOD via `FUN_0040fb2c`

### 2D Events (Buildings)

NPC interactions in towns are driven by `2dEvents.txt` (loaded by `FUN_00443824`),
which maps building IDs to types. Building types that involve NPCs include:

| Building Type     | Examples                                    |
|------------------|---------------------------------------------|
| Tavern           | Human Tavern01, Elf Tavern, Wizard Tavern   |
| Temple           | Human Temple01, Elf Temple, Warlock Temple   |
| Weapon Smith     | Human Weapon Smith01, Elf Weapon Smith       |
| Magic Shop       | Human Magic Shop01, Wizard Magic Shop        |
| Bank             | Dwarven Bank, Elf Bank, Warlock Bank         |
| Stables          | Human Stables01, Necromancer Stables         |

Each building type has a specialized shop UI (screen mode 0x17 for shops)
with NPC shopkeeper dialog integrated.

---

## Integration notes

### Key Functions

| Address      | Suggested Name              | Size  | Purpose |
|-------------|----------------------------|-------|---------|
| `00477033`  | `NPCManager_LoadAll`        | 567   | Master loader: calls all sub-loaders |
| `00476cb9`  | `NPCManager_LoadNPCData`    | 830   | Parses npcdata.txt, npcgreet.txt, npcgroup.txt, npcnews.txt |
| `0047697b`  | `NPCManager_LoadTextTopics` | 745   | Parses npctext.txt, npctopic.txt, npcdist.txt |
| `0047726a`  | `NPCManager_FreeAll`        | varies| Frees all NPC text buffers |
| `00477310`  | `NPCManager_ValidateParams` | varies| Validates NPC action parameters |
| `00477330`  | `NPCManager_ProcessAction`  | varies| Processes NPC interaction |
| `00476c64`  | `NPCManager_CopyNames`      | varies| Copies NPC names to runtime buffer |
| `00476594`  | `NPCManager_LoadMerchants`   | varies| Parses merchant.txt |
| `00476754`  | `NPCManager_LoadTeachers`    | varies| Loads teacher/training data |
| `00445a1c`  | `NPC_ValidateId`            | 307   | Validates NPC ID, error on overflow |
| `00445b4f`  | `NPC_ValidateId2`           | 351   | Same validation, alternate call site |
| `00445d6d`  | `NPC_InitDialog`            | 755   | Sets up NPC conversation UI |
| `0044608d`  | `NPC_LoadPortrait`          | 431   | Loads NPC portrait sprite |
| `00422698`  | `UI_RenderMainInterface`    | 4,628 | Renders full HUD including NPC panels |
| `00416aaa`  | `NPC_DrawPortrait`          | 609   | Draws NPC portrait in dialog |
| `0044686d`  | `EventProcessor_Execute`    | 7,134 | Main event interpreter (NPC opcodes) |
| `004ab69f`  | `Dialog_CloseActive`        | varies| Closes active dialog, restores screen |

### Global Variables

| Address        | Name                 | Type   | Description |
|----------------|----------------------|--------|-------------|
| `DAT_007214E8` | NPCTopicTextBase     | ptr    | NPC topic text array start |
| `DAT_007214EC` | NPCDialogTextBase    | ptr    | NPC dialog text array start |
| `DAT_00f8b068` | CurrentNPCTopicId    | i32  | Current NPC topic/portrait ID |
| `DAT_00590FE0` | CurrentNPCProfession | i32  | Current NPC profession index |
| `DAT_00f79be0` | NPCNamePool          | ptr    | NPC name string pool |
| `DAT_004e28d8` | CurrentScreenMode    | i32  | 0x04 = NPC dialog active |
| `DAT_005067f8` | PreviousScreenMode   | i32  | Saved mode for dialog return |
| `DAT_005c3438` | DialogModeFlag       | i32  | Dialog active indicator |
| `DAT_00507a40` | ActiveDialogHandle   | ptr    | Active dialog/window handle |

### Data File Format Reference

All NPC data files are tab-separated text. The first line(s) may be headers
(skipped by the parser). Field counts per file:

| File            | Fields/Entry | Entries | Notes |
|-----------------|-------------|---------|-------|
| `npcdata.txt`   | 15 (0x0F)  | 500     | 0x13 dword fields when expanded |
| `npcnames.txt`  | 2           | ~540    | First name + last name |
| `npcprof.txt`   | 5-7         | 59      | Cost + 4 text fields |
| `npctext.txt`   | 1           | ~3,120  | Dialog text strings |
| `npctopic.txt`  | 1           | ~2,308  | Topic text strings |
| `npcgreet.txt`  | 2           | 205     | Two greeting variants |
| `npcgroup.txt`  | varies      | 51      | Group assignments |
| `npcnews.txt`   | varies      | 51      | News/rumor text |
| `npcdist.txt`   | 78          | 59      | Probability per area x profession |

### Clean-Room Implementation Guidance

1. **NPC Manager**: Implement as a service class (`INPCManager`) that loads all data
   files at startup and provides lookup methods by NPC ID, profession, and area.

2. **Dialogue state machine**: The dialog flow is naturally modeled as a state machine
   with states: CLOSED -> GREETING -> TOPICS -> (action) -> CLOSED. Each state
   transition is driven by player input or event script execution.

3. **Distribution table**: Store as a 2D array (78 x 59). Implement weighted random
   selection for NPC spawning. Cache row totals for efficient sampling.

4. **Text tables**: The topic and dialog text arrays are large but simple indexed
   lookup tables. Load them into vectors of strings at startup.

5. **Profession effects**: Each profession should map to a concrete gameplay modifier
   (e.g., reduced travel time, bonus to perception, etc.). Implement as a
   `ProfessionEffect` interface with per-profession specializations.

6. **Save/load**: Serialize the 500-entry NPC table and 51-entry group table as
   flat binary blobs matching the original sizes (38,076 and 102 bytes) for
   compatibility with original save files.

7. **Portrait resources**: NPC portraits use a zero-padded 3-digit format (`NPC001`
   through `NPC500`). Load from ICONS.LOD using the standard icon loading path.
