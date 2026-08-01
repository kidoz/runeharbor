---
title: "Event Scripting Engine"
summary: "The event engine interprets global and per-map bytecode that drives interactive game behavior."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Event Scripting Engine

The event engine interprets global and per-map bytecode that drives interactive game behavior.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte file fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

> Source file reference: `D:\mm7Src_eng\MM7\Code\Events.cpp`

## Overview

The event system is the central scripting engine of Might and Magic VII. It operates
as a bytecode virtual machine that processes variable-length commands from `.evt` files.
Events drive nearly every interactive element in the game: NPC conversations, map
transitions, quest state changes, item distribution, spell effects, monster manipulation,
door control, and UI presentation.

Two event scopes exist simultaneously at runtime:

- **Global events** (`global.evt`): Persistent across all maps; handle quest logic,
  global NPC interactions, and cross-map state.
- **Map events** (`<mapname>.evt`): Specific to the currently loaded map; handle
  local triggers, area transitions, decorations, and map-specific scripting.

Each scope has its own bytecode buffer, index table, and event count. The interpreter
selects the appropriate scope based on a context variable before execution.

---

## Event File Format

### Files Involved

| File | Purpose | Storage |
|------|---------|---------|
| `global.evt` | Global event bytecode (always loaded at startup) | `data\events.lod` |
| `<mapname>.evt` | Per-map event bytecode (loaded on map change) | `data\events.lod` |
| `<mapname>.str` | Per-map event string table (null-separated text) | `data\events.lod` |
| `2dEvents.txt` | 2D event definitions (building/shop type table) | `data\events.lod` |

### EVT Bytecode Format

An `.evt` file is a flat byte stream of variable-length commands packed sequentially
with no alignment padding. Each command has the following layout:

```cpp
Offset  Size     Field
0x00    1 byte   Command size (N), excluding this byte itself
0x01    2 bytes  Event ID (uint16, little-endian)
0x03    1 byte   Sub-event index (command sequence number within event)
0x04    1 byte   Opcode (determines the command type)
0x05    N-4      Parameters (opcode-dependent, variable length)

```

The total command occupies `N + 1` bytes in the stream. The next command begins
immediately at offset `current + N + 1`.

Multiple commands with the same Event ID form a single logical event. The sub-event
index (byte at offset 0x03) sequences commands within that event, starting from 0
and incrementing. The interpreter walks commands in order, matching both Event ID
and the current sub-event counter.

### STR String Table Format

The `.str` file is a flat buffer of null-terminated strings packed sequentially.
At load time, the engine scans the buffer counting null terminators and building
an offset index. Strings are referenced by index (1-based) from event opcodes.

Constraints:

- Maximum 500 string entries (index table has 500 slots)
- Maximum individual string length: 800 bytes (validated at load time with error
  `"MAX_EVENT_TEXT_LENGTH needs to be increased to %lu"`)
- Leading whitespace is stripped from each string via a normalization pass

---

## Event Data Buffers

### Global Event Storage

| Variable | Address | Description |
|----------|---------|-------------|
| Global bytecode buffer | `DAT_005a53b8` | Raw bytecode from `global.evt` (max 0xB400 = 46,080 bytes) |
| Global event index | `DAT_00598570` | Index table (12-byte entries, up to ~4,400 entries; 0xCE40 bytes) |
| Global event count | `DAT_005a53b0` | Number of indexed global events |
| Global buffer size | `DAT_005a53b4` | Actual loaded size of `global.evt` data |

### Map Event Storage

| Variable | Address | Description |
|----------|---------|-------------|
| Map bytecode buffer | `DAT_005b33a0` | Raw bytecode from `<map>.evt` (max 0x2400 = 9,216 bytes) |
| Map event index | `DAT_005b6458` | Index table (12-byte entries; 0xCE40 bytes reserved) |
| Map event count | `DAT_005b0f90` | Number of indexed map events |
| Map buffer size | `DAT_005b0f94` | Actual loaded size of `<map>.evt` data |
| Map string buffer | `DAT_005b0fa0` | Raw string data from `<map>.str` (max 0x2400 = 9,216 bytes) |
| Map string count | `DAT_005b0f88` | Number of strings in string table |
| Map string index | `DAT_00597da0` | Byte offsets into string buffer (500 int entries max) |
| Map string buffer size | `DAT_005b0f8c` | Actual loaded size of `<map>.str` data |

---

## Event Index Structure

When an `.evt` file is loaded, the engine builds an index by scanning all commands
sequentially. Each unique (EventID, SubIndex) pair produces one index entry.

### Index Entry Format (12 bytes)

```cpp
Offset  Size     Field
0x00    4 bytes  Event ID (uint32, zero-extended from uint16 in command)
0x04    4 bytes  Sub-event index / command count (zero-extended from byte)
0x08    4 bytes  Byte offset into the raw bytecode buffer

```

The index is stored as a flat array of 12-byte entries. The global index at
`DAT_00598570` and map index at `DAT_005b6458` each reserve 0xCE40 bytes
(enough for ~4,400 entries). Before building, the index memory is initialized
to 0x80 (sentinel value indicating unused slots).

### Index Building Algorithm

```text
offset = 0
event_count = 0
while offset < buffer_size:
    cmd_size = buffer[offset]          # byte 0: command length
    event_id = read_u16(offset + 1)    # bytes 1-2: event ID
    sub_index = buffer[offset + 3]     # byte 3: sub-event index

    index[event_count].event_id = event_id
    index[event_count].sub_index = sub_index
    index[event_count].offset = offset
    event_count += 1

    offset = offset + 1 + cmd_size     # advance past this command

```

---

## Interpreter Loop

The main event processor is a 7,134-byte function at `FUN_0044686d`. It accepts
three parameters:

1. `param_1` (int): The Event ID to execute (0 = re-entry / continuation)
2. `param_2`: Event parameter (stored in `DAT_005b57a0`)
3. `param_3` (int): Context flag (affects NPC dialog behavior)

### Execution Flow

```text
1. Set abort flag (DAT_005b6444) to 0
2. Store event parameter in DAT_005b57a0
3. If param_1 == 0: handle dialog continuation, return
4. Determine player selection mode:
   - If active player (DAT_00507a6c) is 0: mode = 6 (random)
   - Otherwise: mode = 4 (active player)
5. Save current sub-event index (DAT_00597d98) to local
6. Select event scope:
   - If DAT_005c32a0 == 0: use map events (buffer, index, count)
   - If DAT_005c32a0 != 0: use global events (buffer, index, count)
7. Copy index table to working buffer at DAT_005840b8 (0xCE40 bytes)
8. Iterate over all index entries:
   a. Check abort flag - break if set
   b. Match entry where: entry.event_id == param_1
                      AND entry.sub_index == current_sub_counter
   c. Read opcode at buffer[entry.offset + 4]
   d. Execute opcode via switch statement
   e. Increment sub_counter (unless opcode changes it)
9. After loop, if a map transition occurred, run OnMapLoad events

```

### Scope Selection

The event context variable `DAT_005c32a0` determines which event buffer is active:

| Value | Scope | Buffer | Index |
|-------|-------|--------|-------|
| 0 | Map events | `DAT_005b33a0` | `DAT_005b6458` |
| 1 | Global events | `DAT_005a53b8` | `DAT_00598570` |

The runtime pointer `DAT_00590efc` is set to the active buffer, and `DAT_00590ef8`
is set to the active event count, so opcode handlers can access data uniformly.

### Abort Mechanism

Any opcode can set `DAT_005b6444` to a non-zero value to abort the current event.
The interpreter checks this flag at the top of each iteration and breaks out of the
loop if it is set. Opcode 0x01 (Exit) jumps directly past the loop.

---

## Opcode Table

All opcodes operate on the command buffer starting at offset +5 (after the 4-byte
header of event_id + sub_index + opcode). Parameter offsets below are relative to
the command start (byte 0 = size).

### Complete Opcode Reference

| Op | Hex | Name | Parameters | Description |
|----|-----|------|------------|-------------|
| 1 | 0x01 | `EVT_EXIT` | None | Exit event processing immediately |
| 2 | 0x02 | `EVT_NPC_DIALOG` | +5: NPC text index (u32) | Open NPC dialog window with 4 response buttons |
| 3 | 0x03 | `EVT_PLAY_SOUND` | +5: sound ID (u32), +9: param2 (u32), +0xD: param3 (u32) | Play sound effect |
| 4 | 0x04 | `EVT_SKIP_NEXT` | None | Decrement sub-event counter (skip next command) |
| 5 | 0x05 | `EVT_SKIP_NEXT2` | None | Same as opcode 4 |
| 6 | 0x06 | `EVT_TELEPORT` | +5: X (u32), +9: Y (u32), +0xD: Z (u32), +0x11: yaw (u32), +0x15: pitch (u32), +0x19: viewZ (u32), +0x1D: flags (2B), +0x1F: map name (string) | Teleport within map or transition to new map |
| 7 | 0x07 | `EVT_GIVE_GOLD` | (calls gold handler) | Give gold/treasure to party; returns early if handler fails |
| 8 | 0x08 | `EVT_SET_PLAYER_VAR` | +5: player selector (u8), +6: variable ID (u8) | Set a player stat/variable |
| 9 | 0x09 | `EVT_GIVE_ITEM` | +5: player selector (u8), +6: sub-param (u8), +7: item data (u32) | Give item to specified player |
| 10 | 0x0A | `EVT_SET_FLAG` | +5: flag type (u8), +6: flag value (u8) | Set global flag (e.g., `DAT_006bdea4`) |
| 11 | 0x0B | `EVT_CHECK_FLAG` | (calls flag checker) | Check global flag; conditional branch |
| 12 | 0x0C | `EVT_CHANGE_MAP` | +5: exit direction (u8), +6: param (u8), +7: map name (string) | Full map change with movie/transition |
| 13 | 0x0D | `EVT_MODIFY_OBJECT` | +10: data (variable) | Modify map object properties |
| 14 | 0x0E | `EVT_CHECK_CONDITION` | +5: condition type (u16), +7: value (u32) | Check player stat/condition; branch on result |
| 15 | 0x0F | `EVT_DOOR_CONTROL` | (calls door handler) | Open, close, or toggle door |
| 16 | 0x10 | `EVT_ADD_STAT` | +5: stat type (u16), +7: value (u32) | Add value to player stat; triggers redraw for stats 0x15-0x18 |
| 17 | 0x11 | `EVT_REMOVE_ITEM` | +5: item type (u16), +7: item ID (u32) | Remove item from player inventory (searches 126 slots + 16 equipped) |
| 18 | 0x12 | `EVT_SUBTRACT_STAT` | +5: stat type (u16), +7: value (u32) | Subtract value from player stat |
| 19 | 0x13 | `EVT_MODIFY_NPC` | +7: NPC type (u8), +8: param1 (u32), +0xC: param2 (u32), +0x10: param3 (u32), +0x14: param4 (u32), +0x18: param5 (u32) | Modify NPC properties (6 parameters) |
| 21 | 0x15 | `EVT_MODIFY_NPC_EX` | +7: type (u8), +8..+0x1C: 7 params (u32 each) | Extended NPC modification (7 parameters) |
| 22 | 0x16 | `EVT_SHOW_BUILDING` | +5: building ID (u32) | Show shop/building/guild UI; behavior differs if param_3 == 0 |
| 23 | 0x17 | `EVT_SHOW_EFFECT` | +0xD: effect ID (u8) | Show visual effect |
| 24 | 0x18 | `EVT_PLAY_ANIMATION` | +0xD: animation ID (u8) | Play animation sequence |
| 25 | 0x19 | `EVT_RANDOM_GOTO` | +5..+10: up to 6 target sub-IDs (u8 each) | Jump to random event command from non-zero targets |
| 26 | 0x1A | `EVT_SHOW_TEXT` | +5: string index (u32); +9: alt string (u32); +0xD: alt2 (u32); +0x11: jump target (u8) | Display text from map string table; may chain to alt strings |
| 29 | 0x1D | `EVT_SET_NPC_PORTRAIT` | +5: portrait index (u32) | Set NPC portrait; uses `DAT_007214E4` portrait lookup for global events |
| 30 | 0x1E | `EVT_SET_NPC_NAME` | +5: name string index (u32) | Set NPC name from string table (map) or portrait table (global) |
| 32 | 0x20 | `EVT_GIVE_AWARD` | (calls award handler) | Give award/achievement to player |
| 33 | 0x21 | `EVT_STATUS_MESSAGE` | None | Display status bar message; exits event after display |
| 34 | 0x22 | `EVT_SPAWN_ITEM` | +0xD: X (u32), +0x11: Y (u32), +0x15: Z (u32), +0x19: type (u8), +0x1A: count (u8) | Spawn item object on the map at coordinates |
| 35 | 0x23 | `EVT_SET_PLAYER_SELECT` | +5: mode (u8) | Set active player selection mode (see table below) |
| 36 | 0x24 | `EVT_JUMP_TO_EVENT` | +5: target sub-ID (u8) | Jump to a different sub-event within the same event |
| 39 | 0x27 | `EVT_SET_GLOBAL_VAR` | +5: var index (u32), +9: subfield (u8), +10: value (u32) | Set global variable / QBit; 6 subfields per variable entry |
| 40 | 0x28 | `EVT_SET_GLOBAL_VAR2` | +5: var index (u32), +9: value (u32) | Set global variable (alternate field); may trigger dialog refresh |
| 41 | 0x29 | `EVT_CAST_SPELL` | +5: spell school (u8), +6: spell index (u8), +7: power (u32) | Cast spell on party via `FUN_0045664c` then `FUN_004936d9` |
| 42 | 0x2A | `EVT_MODIFY_DECORATION` | +5: action (u32) | Modify map decoration; action 0 = disable, else set property |
| 43 | 0x2B | `EVT_CHECK_SKILL` | +5: skill ID (u8), +6: mastery (u8), +7: required level (u32), +0xB: jump target (u8) | Check player skill level and mastery; branch if met |
| 47 | 0x2F | `EVT_SET_MONSTER_TOPIC` | +5: monster index (u32), +9: low byte (u8), +10: high byte (u8) | Set monster dialog topic (stored as u16 = high*256 + low) |
| 48 | 0x30 | `EVT_SET_MONSTER_FIELD` | +5: monster ID (u24), +8: field index (u8), +9: value (u32) | Set arbitrary field in monster record |
| 49 | 0x31 | `EVT_SET_MONSTER_HOSTILE` | +0xD: hostility param | Set monster hostility by group/type matching |
| 50 | 0x32 | `EVT_SET_MONSTER_GROUP` | +5: group index (u32), +9: AI state (u32) | Set monster group AI state; clears bottom 2 bits of flags |
| 51 | 0x33 | `EVT_CHECK_MAP_VAR` | +10: value (u8) | Check map-specific variable; branch to +0xB target if true |
| 54 | 0x36 | `EVT_REPLACE_MONSTER` | +5: old type (u32), +9: new type (u32) | Replace all monsters of one type with another (iterates all monsters) |
| 55 | 0x37 | `EVT_SET_MONSTER_AI` | +5: monster type (u32), +9: AI value (u32) | Set AI behavior for all monsters matching type |
| 56 | 0x38 | `EVT_CHECK_TIME` | +6: jump target (u8) | Check game time condition; branch if condition met |
| 57 | 0x39 | `EVT_GIVE_EXPERIENCE` | +0xD: XP data | Give experience points to player |
| 58 | 0x3A | `EVT_TAKE_GOLD` | +0xD: gold data | Remove gold from party |
| 59 | 0x3B | `EVT_CURE_CONDITION` | +5: player selector (u8), +6: condition ID (u8) | Cure player condition (uses player selection mode) |
| 60 | 0x3C | `EVT_SET_HOSTILE_BY_IDX` | +0xD: hostility param | Set monster hostility by direct index |

### Gaps in Opcode Space

Opcodes 20 (0x14), 27 (0x1B), 28 (0x1C), 37 (0x25), 38 (0x26), 44-46 (0x2C-0x2E),
52-53 (0x34-0x35) have no corresponding case in the switch statement and are treated
as no-ops (fall through without action).

---

## Player Selection Modes

Many opcodes that affect players use a player selector byte at offset +5. The mode
is set by opcode 0x23 (`EVT_SET_PLAYER_SELECT`) and persists across commands within
the same event execution.

| Mode | Meaning | Behavior |
|------|---------|----------|
| 0 | Player 0 (slot 0) | Operate on first party member |
| 1 | Player 1 (slot 1) | Operate on second party member |
| 2 | Player 2 (slot 2) | Operate on third party member |
| 3 | Player 3 (slot 3) | Operate on fourth party member |
| 4 | Active player | Operate on currently selected player (`DAT_00507a6c`); if none selected, behaves as mode 0 or skips |
| 5 | All players | Loop through all 4 players (stride 0x1B3C bytes per player record, base `DAT_00acd804`, end `0x00AD44F4`) |
| 6 | Random player | Select random player via `rand() % 4` |

The default mode at event start is determined by the active player: if `DAT_00507a6c`
is 0, mode defaults to 6 (random); otherwise mode defaults to 4 (active).

---

## Trigger System

Events are triggered by various game systems. The trigger type is encoded in the
first command of an event (the opcode byte at offset +4 of the initial command in
the sequence).

### Trigger Types

| Type Byte | Name | Description |
|-----------|------|-------------|
| 0x01 | `TRIGGER_INTERACTION` | Player interacts with object, NPC, or face |
| 0x03 | `TRIGGER_AMBIENT_SOUND` | Play ambient/looping sound at position |
| 0x05 | `TRIGGER_ON_MAP_LOAD` | Fire once when map loads (character '5' check in `FUN_00443fb8`) |
| 0x1F | `TRIGGER_TIMER_ABSOLUTE` | Fire at a specific absolute game time |
| 0x25 | `TRIGGER_ON_MAP_ENTER` | Fire immediately on map entry (calls `EventProcessor(0)`) |
| 0x26 | `TRIGGER_TIMER_PERIODIC` | Fire periodically at defined intervals |

### Trigger Processing Order

1. **Map load**: `FUN_00443fb8` (RunOnMapLoadEvents) iterates all map event index
   entries. For each entry whose command at `buffer[offset + 4]` equals '5' (0x35,
   which is ASCII '5'), it fires the event with `param_1 = 0`.

2. **Timer setup**: `FUN_00443fff` (ProcessTimerEvents) iterates all map events and:
   - Type 0x03: Registers ambient sound via `FUN_004a99f7`
   - Type 0x1F: Creates a one-shot timer entry
   - Type 0x25: Fires event immediately with sub-index from index entry
   - Type 0x26: Creates a periodic timer entry

3. **Interaction**: When the player clicks on a face, decoration, or NPC that has an
   event trigger ID, the game invokes `FUN_0044686d` with the trigger's event ID.

### Map Event Trigger IDs

Map faces, decorations, and monsters can have event trigger IDs assigned:

- **Face trigger**: stored at offset +0x124 in the face record
- **Monster trigger**: stored at offset +0x124 in the monster record (836-byte stride)
- **Decoration trigger**: associated via decoration type lookup

---

## Timer System Integration

Timer events use the game's 64-bit game clock (`DAT_00acce64` + `DAT_00acce68`),
where each tick represents 1/128th of a real-time second.

### Timer Entry Structure (32 bytes)

Stored at `DAT_005b57A8` with stride 0x20 (32 bytes), up to `DAT_005b6448` entries:

```text
Offset  Size   Field
0x00    4      Next fire time (low dword)
0x04    4      Next fire time (high dword)
0x08    2      Source event ID
0x0A    2      Target sub-event ID
0x0C    2      Timer type (0x1F or 0x26)
0x0E    2      Reserved
0x10    2      Year interval
0x12    2      Month interval
0x14    2      Week interval
0x16    2      Day interval
0x18    2      Hour interval
0x1A    2      Minute interval
0x1C    2      Second interval (low)
0x1E    2      Second interval (high)

```

### Timer Parameter Layout in Command

Timer commands (0x1F and 0x26) encode their schedule in the command body:

```text
+5:  years interval (u8)
+6:  months interval (u8)
+7:  weeks interval (u8)
+8:  days interval (u8)
+9:  hours interval (u8)
+10: minutes interval (u8)
+11: seconds interval (u16)

```

### Timer Calculation

The next fire time is computed from the current game time:

1. Convert current game time to calendar components (seconds, minutes, hours,
   days, weeks, months, years) using the standard time constants.
2. Add the interval to the appropriate component.
3. Reassemble the absolute time:

```text
   total_seconds = years * 12 * 2419200
                 + months * 2419200
                 + weeks * 604800
                 + days * 86400
                 + hours * 3600
                 + minutes * 60
                 + seconds
   fire_time_ticks = total_seconds * 128
   ```

4. For periodic timers (0x26): if all intervals are zero, the event fires once and
   the fire time is set to 0.

### Calendar Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| 0x3C | 60 | Seconds per minute |
| 0xE10 | 3,600 | Seconds per hour |
| 0x15180 | 86,400 | Seconds per day |
| 0x93A80 | 604,800 | Seconds per week (7 days) |
| 0x24EA00 | 2,419,200 | Seconds per month (28 days = 4 weeks) |
| 0x80 | 128 | Game ticks per real-time second |

The MM7 calendar has 12 months of 28 days each, giving 336 days per year.

---

## Global Variable System (QBits)

The event engine uses a global variable table for quest state tracking, NPC state,
and general-purpose flags. This table persists across map changes and is saved with
the game.

### Variable Table Structure

The table is stored starting at `DAT_0072d50c`, with each entry having a stride of
0x4C (76 bytes). This yields a structure with multiple sub-fields per variable:

```text
Base:  DAT_0072d50c (entry array, stride 0x13 dwords = 0x4C bytes)
Flags: DAT_0072d514 (offset +8 from base, per-entry flags byte)

```

### Subfields per Variable Entry (accessed by opcode 0x27)

Each variable entry at `DAT_0072d50c + index * 0x4C` has at least 6 writable
subfields, selected by the subfield byte at command offset +9:

| Subfield | Offset within Entry | Address Pattern |
|----------|-------------------|-----------------|
| 0 | +0x28 | `DAT_0072d534 + index * 0x4C` |
| 1 | +0x2C | `DAT_0072d538 + index * 0x4C` |
| 2 | +0x30 | `DAT_0072d53c + index * 0x4C` |
| 3 | +0x34 | `DAT_0072d540 + index * 0x4C` |
| 4 | +0x38 | `DAT_0072d544 + index * 0x4C` |
| 5 | +0x3C | `DAT_0072d548 + index * 0x4C` |

### Opcode 0x28 (SET_GLOBAL_VAR2)

An alternate setter that writes to a different field:

- Offset +0x14 within entry: `DAT_0072d520 + index * 0x4C`

### Flags Field

The per-entry flags byte at offset +8 (`DAT_0072d514 + index * 0x4C`) uses bit fields:

- **Bit 7 (0x80)**: Quest/variable active/completed flag (set by monster AI and
  condition checks)
- **Bits 0-1 (0x03)**: Cleared by opcode 0x32 when setting monster group state
- **Bit 0 (0x01)**: General-purpose flag

### Special Variable Checks

Setting variable index 8, subfield 0, to value 0x4E (78) triggers a special
sequence: closes the current dialog, clears UI state, and opens a new NPC dialog
window (NPC ID 0xAA). This appears to be a hardcoded quest milestone trigger.

---

## Map Events vs Global Events

### Scope Resolution

The context variable `DAT_005c32a0` selects which event pool the interpreter uses:

| Context | Value | Buffer | Index | Count |
|---------|-------|--------|-------|-------|
| Map | 0 | `DAT_005b33a0` | `DAT_005b6458` | `DAT_005b0f90` |
| Global | 1 | `DAT_005a53b8` | `DAT_00598570` | `DAT_005a53b0` |

### Behavioral Differences by Scope

Several opcodes behave differently depending on the scope:

- **Opcode 0x1D (SET_NPC_PORTRAIT)**: In map scope (context 0), calls a UI refresh
  function. In global scope (context 1), sets the portrait index from the NPC text
  lookup table (`DAT_007214E4 + index * 8`).

- **Opcode 0x1E (SET_NPC_NAME)**: In map scope, copies the string from the map
  string table to the NPC name buffer (`DAT_005b07b8`). In global scope, sets the
  portrait from the NPC text table and clears the name buffer.

- **Opcode 0x06 (TELEPORT)**: When performing a full map change, computes the
  indoor/outdoor flag as `(DAT_005c32a0 == 0) + 1` -- meaning map events produce
  indoor type (1) and global events produce outdoor type (2) as the default.

- **Opcode 0x1A (SHOW_TEXT)**: Only processes text from the map string table when
  `DAT_00597d98 == 0` (sub-event counter at 0). Otherwise uses alternate string
  lookup logic with comparison checking.

### Loading Sequence

1. **Startup**: `FUN_00443dc4` loads `global.evt` into the global buffer and builds
   the global event index.

2. **Map change**: `FUN_00444383` loads `<map>.evt` and `<map>.str`. It formats the
   filename as `"%s.evt"` and `"%s.str"` from the map name. Then:
   - `FUN_00443f1b` builds the map event index
   - `FUN_00443e54` parses the string table
   - `FUN_00443fff` processes timer/trigger events
   - `FUN_00443fb8` runs OnMapLoad events (type '5')

---

## String System

### Map String Table Loading

The string table (`<map>.str`) is loaded by `FUN_00443e54` (199 bytes):

1. Clear the string index array at `DAT_00597da0` (2,000 bytes = 500 int entries).
2. Scan the raw string buffer `DAT_005b0fa0` byte by byte.
3. Each null terminator (`\0`) marks the end of a string. Record the offset of the
   byte after the null as the start of the next string.
4. String indices are 1-based (index 0 is unused; string 1 starts at offset 0).
5. Track the maximum string length; error if any string exceeds 800 bytes.
6. After indexing, normalize each string by calling `FUN_00452c5c` to strip leading
   whitespace. If the normalized start differs from the recorded offset, adjust
   the index entry forward by 1 byte.

### String Access from Opcodes

Event opcodes reference strings by their 1-based index. The string content is
accessed as:

```text
string_ptr = &DAT_005b0fa0 + DAT_00597da0[string_index]

```

For display, the text is typically copied to a presentation buffer at `DAT_005c32a8`
(opcode 0x1A) or `DAT_005b07b8` (opcode 0x1E).

---

## Opcode Details: Teleport (0x06)

The teleport opcode is the most complex command, handling both intra-map teleportation
and full map transitions.

### Teleport command layout

```text
+5:   X coordinate (int32)
+9:   Y coordinate (int32)
+0xD: Z coordinate (int32)
+0x11: Yaw / facing direction (int32), -1 = don't change
+0x15: Pitch (int32)
+0x19: View Z offset (int32)
+0x1D: Flag byte 1
+0x1E: Flag byte 2
+0x1F: Map name (null-terminated string, up to ~32 chars)

```

### Intra-Map Teleport (flag bytes both 0, map name starts with '0')

When the map name field starts with '0' (ASCII 0x30) and coordinates are non-zero:

1. Set party position: X, Y, Z, yaw (if not -1), pitch, viewZ
2. Play teleport sound (sound ID 0xE8 = 232)
3. Clear all teleport override variables

### Full Map Transition (map name present)

When the map name is a non-empty string:

1. Flush any pending 3D rendering
2. Call `FUN_0044485c` to prepare transition with coordinates
3. Save current event state (event ID + sub-index) for potential resume
4. Determine map type: `(context == map_events) + 1` -> 1=indoor, 2=outdoor
5. Execute map transition via `FUN_0044989e`
6. Handle special UI state (mode 0xD = menu context)
7. If in menu mode and map load pending, close all dialogs and reset UI

### Transition Override Variables

```text
DAT_005b6428: Override X
DAT_005b642c: Override Y
DAT_005b6430: Override Z
DAT_005b6434: Override yaw (-1 = none)
DAT_005b6438: Override pitch
DAT_005b643c: Override viewZ
DAT_005b6440: Override active flag (any non-zero coordinate sets this)

```

---

## Opcode Details: Change Map (0x0C)

A simpler map change that supports alignment-specific transitions:

### Change-map command layout

```text
+5:   Exit direction (u8)
+6:   Transition parameter (u8)
+7:   Map name (null-terminated string)

```

### Special Map Names

| Name | Effect |
|------|--------|
| `arbiter good` | Set party alignment to Good (0), play end sequence |
| `arbiter evil` | Set party alignment to Evil (2), play end sequence |
| `pcout01` | Exit to outdoor; reset combat state |

### Processing

1. Copy map name, trim trailing spaces
2. Clean up any active video playback
3. Look up map name and load corresponding movie via `FUN_004be671`
4. Check for special alignment transitions
5. If transition parameter is 0 or UI mode is combat (3), maintain current UI state
6. If in dialog mode (0xD), play building-specific music

---

## Opcode Details: Conditional Check (0x0E)

Conditional branching checks a player stat or condition and jumps to a different
sub-event if the check succeeds.

### Conditional-check command layout

```text
+5:   Condition type (u16: low byte = type, high byte = subtype)
+7:   Comparison value (u32)
+0xB: Jump target sub-event ID (u8)

```

### Evaluation

1. Determine target player based on current selection mode
2. Call `FUN_00449bd7` with condition type and value
3. If check returns non-zero (true): jump to sub-event target at +0xB
4. If check returns zero (false): continue to next command
5. For mode 5 (all players): check passes only if ALL players pass

---

## Opcode Details: Check Skill (0x2B)

Checks whether a player has sufficient skill level and mastery.

### Skill-check command layout

```text
+5:   Skill ID (u8)
+6:   Required mastery (u8): 0=any, 1=normal, 2=expert, 3=master, 4=grandmaster
+7:   Required level (u32): minimum skill points
+0xB: Jump target sub-event ID (u8) on success

```

### Skill Value Encoding

Player skills are stored as 16-bit values where:

- **Bits 0-5 (mask 0x3F)**: Skill level (0-63)
- **Bit 6 (0x40)**: Expert mastery
- **Bit 7 (0x80)**: Master mastery
- **Bit 8 (0x100)**: Grandmaster mastery

The mastery check uses a lookup table:

```text
mastery_table[0] = 1        (always true - any mastery)
mastery_table[1] = skill & 0x40   (expert flag)
mastery_table[2] = skill & 0x80   (master flag)
mastery_table[3] = skill & 0x100  (grandmaster flag)

```

The check passes when `skill_level >= required_level AND mastery_table[required_mastery] != 0`.

---

## NPC Dialog Flow (Opcode 0x02)

When a dialog event triggers:

1. Verify dialog is allowed via `FUN_00446251`
2. Flush 3D renderer if hardware rendering active
3. Clear all active sounds
4. Create dialog window: 640x480 (0x280 x 0x1E0), type 0x19
5. If current NPC profession is 0xA7 (167), use default dialog ID 0xBB
6. Create 4 response buttons at fixed positions:
   - Button 1: X=0x3D (61), Y=0x1A8 (424), size 0x1F x 0x5E (31x94)
   - Button 2: X=0xB1 (177)
   - Button 3: X=0x124 (292)
   - Button 4: X=0x197 (407)
7. Create dismiss button: full-width, height 0xB0 (176)
8. Button click IDs map to sub-events 0x31-0x34

---

## Event System State Variables

| Variable | Address | Description |
|----------|---------|-------------|
| `DAT_005c32a0` | Event context | 0 = map events, 1 = global events |
| `DAT_005b6444` | Abort flag | Non-zero = stop processing current event |
| `DAT_005b57a0` | Event parameter | Passed value from caller |
| `DAT_00597d98` | Sub-event counter | Current command sequence position |
| `DAT_00590ef8` | Active event count | Number of events in current scope |
| `DAT_00590efc` | Active buffer pointer | Pointer to current bytecode buffer |
| `DAT_005c3298` | Saved event ID | For resuming after map transition |
| `DAT_005c329c` | Saved sub-event ID | For resuming after map transition |
| `DAT_005c3438` | Dialog mode flag | Non-zero when in dialog state |
| `DAT_00507a6c` | Active player index | Currently selected party member |
| `DAT_00507a40` | Active dialog handle | Pointer to current dialog/window |
| `DAT_005c3444` | 2D event parameter | Building/shop type for 2D events |
| `DAT_005b6448` | Timer entry count | Number of active timer entries |
| `DAT_005b57a4` | Transition pending | Set to 1 when map transition text shown |
| `DAT_006bdea4` | Global flag store | Written by opcode 0x0A |

---

## Key Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| `FUN_00443d04` | `LoadFileFromLOD` | 192 B | Generic LOD file loader with size validation |
| `FUN_00443dc4` | `LoadGlobalEvents` | 144 B | Load `global.evt`, build global index |
| `FUN_00443e54` | `BuildStringTable` | 199 B | Parse null-separated `.str` into indexed strings |
| `FUN_00443f1b` | `BuildMapEventIndex` | 157 B | Build map event index from loaded `.evt` |
| `FUN_00443fb8` | `RunOnMapLoadEvents` | 71 B | Fire all type-5 events (map load triggers) |
| `FUN_00443fff` | `ProcessTimerEvents` | 900 B | Set up timers, fire immediate triggers |
| `FUN_00444383` | `LoadMapEvents` | 117 B | Load `<map>.evt` and `<map>.str` |
| `FUN_004443f8` | `GetMapDescription` | 399 B | Get description text for current location |
| `FUN_00444755` | `GetTransitionText` | ~200 B | Get transition prompt text for event trigger |
| `FUN_0044686d` | `EventProcessor_Execute` | 7,134 B | **Main event interpreter** (giant switch on opcode) |
| `FUN_00443824` | `Load2dEvents` | ~640 B | Parse `2dEvents.txt` building/shop definitions |
| `FUN_00449bd7` | `CheckCondition` | varies | Evaluate condition check for opcode 0x0E |
| `FUN_0044b01e` | `AddToPlayerStat` | varies | Add value to player stat (opcode 0x10) |
| `FUN_0044a5ee` | `SubtractFromStat` | varies | Subtract from player stat (opcode 0x12) |
| `FUN_0044b9f0` | `RemoveFromPlayer` | varies | Remove item from inventory (opcode 0x11) |
| `FUN_00446680` | `CheckMapVariable` | varies | Check map-specific variable (opcode 0x33) |
| `FUN_00446602` | `CheckTimeCondition` | varies | Check game time condition (opcode 0x38) |
| `FUN_004485ca` | `SetHostileByIndex` | varies | Set monster hostility by index (opcode 0x3C) |
| `FUN_0044853b` | `SetMonsterHostile` | 143 B | Set monster hostility by group match (opcode 0x31) |

---

## 2D Event System

The `2dEvents.txt` file defines building and shop types for the 2D interaction system
(shops, guilds, temples, training halls, etc.). It is loaded by `FUN_00443824` and
stored at `DAT_005c344C`.

When opcode 0x16 (`EVT_SHOW_BUILDING`) fires:

- If `param_3 == 0`: stores the building ID in `DAT_005c3444` for deferred processing
- If `param_3 != 0`: immediately opens the building UI, creating the appropriate
  interface (shop inventory, guild spell list, temple services, etc.)

The building ID from `2dEvents.txt` determines which UI template is instantiated and
which item/service lists are presented.

---

## Error Handling

The event system includes several validation points:

- **Buffer overflow**: `FUN_00443d04` validates that loaded file size does not exceed
  the buffer maximum. Error: `"File %s Size %lu - Buffer size %d"`
- **String overflow**: `FUN_00443e54` checks that no string exceeds 800 bytes.
  Error: `"MAX_EVENT_TEXT_LENGTH needs to be increased to %lu"`
- **Missing transitions**: When a map face has an event trigger but no transition text
  is found, logs: `"No transition text found"` with source file reference at line 0x582
  (1410) of `Events.cpp`
- **Invalid NPC**: `"NPC id exceeds MAX_DATA!"` when NPC ID is out of range

---

## Cross-References

- [architecture.md](architecture.md): Main loop where events are dispatched
- [blv-indoor-maps.md](blv-indoor-maps.md): Face trigger IDs that invoke events
- [odm-outdoor-maps.md](odm-outdoor-maps.md): Outdoor model face triggers
- [npc-dialogue.md](npc-dialogue.md): NPC data referenced by dialog opcodes
- [monster-ai.md](monster-ai.md): Monster records modified by event opcodes
- [time-calendar.md](time-calendar.md): Game clock used by timer events
- [map-transitions.md](map-transitions.md): Map loading triggered by opcodes 0x06/0x0C
