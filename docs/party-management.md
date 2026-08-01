---
title: "Party Management"
summary: "Party state covers formation, movement, resources, rest, alignment, and shared time."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Party Management

Party state covers formation, movement, resources, rest, alignment, and shared time.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

The party management system governs the player's group of four characters as they
move through the world. It encompasses world-space position and orientation, movement
and collision, formation geometry, the rest/sleep system, consumable resources (gold,
food), moral alignment, the game-time calendar, fast travel, and the day/night cycle.

All party-level state is stored in a contiguous region of the data segment beginning at
`DAT_00acce38`, with position/orientation at `DAT_00acd4EC`--`DAT_00acd50C`. The four
player character records (each 0x1B3C = 7,004 bytes) follow at `DAT_00acd804`.

---

## Data Structures

### Party Global State

The party structure occupies roughly 0x16238 = 90,680 bytes when serialized to
`party.bin` in save files. This includes the party header, four character records,
inventory, quest state, and ancillary data.

```cpp
struct Party {
    // --- Physics / Geometry ---
    int32_t  height;            // DAT_00acce3C  default 0xC0 (192)
    int32_t  currentHeight;     // DAT_00acce40  runtime height (may differ while jumping/falling)
    int32_t  eyeLevel;          // DAT_00acce44  default 0xA0 (160)  camera offset from feet
    int32_t  currentEyeLevel;   // DAT_00acce48  runtime eye level

    // --- Game Clock ---
    uint32_t gameTimeLow;       // DAT_00acce64  64-bit game time, low dword
    uint32_t gameTimeHigh;      // DAT_00acce68  64-bit game time, high dword

    // --- World Position ---
    int32_t  posX;              // DAT_00acd4EC  party X in world units
    int32_t  posY;              // DAT_00acd4F0  party Y in world units
    int32_t  posZ;              // DAT_00acd4F4  party Z (altitude)
    int32_t  facing;            // DAT_00acd4F8  yaw angle (0..2047, 0 = East, 512 = North)
    int32_t  pitch;             // DAT_00acd4FC  pitch angle (look up/down)

    // --- Saved Position (respawn / recall) ---
    int32_t  savedPosX;         // DAT_00acd500
    int32_t  savedPosY;         // DAT_00acd504
    int32_t  savedPosZ;         // DAT_00acd508
    int32_t  savedFacing;       // DAT_00acd50C
    int32_t  savedPitch;        // DAT_00acd510

    // --- Miscellaneous ---
    int32_t  viewZOffset;       // DAT_00acd520  additional Z for camera
    int32_t  posZ_physics;      // DAT_00acd538  copy of posZ used by physics
    int32_t  alignmentState;    // DAT_00acd59C  alignment-related flags
    int32_t  combatMode;        // DAT_00acd6B4  1 = turn-based combat active
    int32_t  alignment;         // DAT_00acd6C0  0 = good, 1 = neutral, 2 = evil

    // --- Characters ---
    // Player characters[4] at DAT_00acd804..DAT_00ad44F4
    // Each 0x1B3C = 7,004 bytes; see character-system.md
};

```

### Height & Eye Level (from mm7.ini)

Read by `FUN_00466086` from the `[party]` INI section:

| INI Key     | Address        | Default | Description              |
|-------------|----------------|---------|--------------------------|
| `height`    | `DAT_00acce3C` | 0xC0 (192) | Party bounding cylinder height |
| `eyelevel`  | `DAT_00acce44` | 0xA0 (160) | Camera offset from ground |

The `walkspeed` key (same INI section) controls base movement speed.

### Active Character

| Address        | Purpose                                  |
|----------------|------------------------------------------|
| `DAT_00507a6c` | Currently selected character index (0--3) |
| `DAT_005061c8` | Secondary character selection (for some operations) |

The party display rotates character portraits so the active character is drawn first:

```text
order[i] = (activeCharacter - 1 + i) % 4   // for i = 0..3

```

---

## Key Algorithms

### Movement & Collision

Movement is processed in `FUN_004304d6` (the main gameplay tick function, ~27,000
bytes). Each tick:

1. **Input sampling**: Key bindings read from the INI file:
   - `KEY_FORWARD` / `KEY_BACKWARD` -- forward/back movement
   - `KEY_STEPLEFT` / `KEY_STEPRIGHT` -- strafing
   - `KEY_FLYUP` / `KEY_FLYDOWN` -- vertical movement (when flying)
   - `KEY_JUMP` -- jump initiation

2. **Velocity computation**: Walk speed from `walkspeed` INI key, modified by:
   - Backward movement (reduced speed, typically 50%)
   - Strafing (reduced speed)
   - Haste/slow spell effects
   - Terrain type modifiers

3. **Turn rate**: `TurnDelta` string referenced in the movement code controls
   yaw rotation speed per tick.

4. **Collision detection**: The party is treated as a cylinder with:
   - Radius: derived from party width constant
   - Height: `DAT_00acce3C` (default 192 world units)
   - The engine checks against map geometry (BLV faces or ODM terrain)
   - Indoor: sector-based face collision
   - Outdoor: heightmap terrain + BSP model collision

5. **Gravity / falling**: When the party is above the floor and not flying, gravity
   applies. The physics Z (`DAT_00acd538`) tracks the fall. The "Can't jump to
   that location!" error (string at `004e45e4`) is shown when a jump target is
   unreachable.

6. **Walk sound**: Unless `nowalksound` INI flag is set, `WalkSound` is triggered
   on each step. The sound varies by terrain surface type.

### Formation

The party occupies a single world-space point for position purposes.
Individual character "slots" (1--4) determine the order in which characters
are targeted by area effects and melee attacks. The formation is implicit:

- Characters are ordered 0--3 in the `characters[]` array
- Front rank: characters 0--1 (melee-reachable)
- Back rank: characters 2--3 (ranged only unless enemies close)
- Active character selection rotates the display order but not the
  formation rank for targeting purposes

### Rest Mechanics

The rest screen is initialized by `FUN_0041f66a` (triggered by `KEY_REST`).
The screen mode `DAT_004e28d8` is set to `0x0D` (13 = rest screen).

**UI assets loaded:**

| Texture Name | Purpose           |
|--------------|-------------------|
| `restmain`   | Background panel  |
| `restb1`     | Button 1 (rest until healed) |
| `restb2`     | Button 2 (rest for N hours) |
| `restb3`     | Button 3 (wait without resting) |
| `restb4`     | Button 4 (unused / cancel) |
| `restexit`   | Exit/cancel button |

**Rest flow:**

1. Player opens rest screen (screen mode 0x0D)
2. Checks performed:
   - Hostile monsters nearby (combat encounter detection uses distance
     threshold 0x1400 = 5120 outdoor, 0x0A00 = 2560 indoor)
   - Safe rest location (some maps flag rest as unsafe)
3. Food is consumed (1 unit per character per 8 hours of rest)
4. Game time advances by the rest duration
5. HP/SP regeneration calculated per character based on rest hours
6. Random encounter check during rest (probability per hour rested)
7. Conditions/buffs with timed expiry may trigger or expire

**Rest screen terrain background:** `TERRA%03d` textures are loaded by
`FUN_0041f4f3` based on the current outdoor terrain tile.

### Food & Gold

**Gold:**

- Displayed as `"%d gold"` or `"(%s), and %d gold"` (strings at `004e4494`/`004e449c`)
- Event opcode 0x07 (`EVT_GIVE_GOLD`) adds gold to the party
- Event opcode 0x3A (`EVT_TAKE_GOLD`) removes gold
- Gold is stored in the party structure within `party.bin`

**Food:**

- Consumed during rest: 1 ration per character per 8-hour rest period
- If insufficient food, rest is shortened or denied
- Food can be purchased at taverns (2D event building type)

### Alignment

Party alignment is stored at `DAT_00acd6C0`:

| Value | Alignment | UI Theme Suffix |
|-------|-----------|-----------------|
| 0     | Good      | `-b` (e.g., `bardata-b`, `ib-*-B`) |
| 1     | Neutral   | none (e.g., `bardata`, `ib-*-A`) |
| 2     | Evil      | `-c` (e.g., `bardata-c`, `ib-*-C`) |

Alignment affects:

- UI skin (interface bar textures, NPC dialog frames)
- Loading screen bar (`bardata-b`/`bardata`/`bardata-c`)
- Quest outcomes and NPC reactions
- Temple healing costs

The entire interface theme loads different texture sets based on alignment:

- Good: `ib-r-B.pcx`, `ib-b-B.pcx`, `evtnpc-b`, etc.
- Neutral: `ib-r-A.pcx`, `ib-b-A.pcx`, `evtnpc`, etc.
- Evil: `ib-r-C.pcx`, `ib-b-C.pcx`, `evtnpc-c`, etc.

---

## Constants & Enums

### Game Time / Calendar

Game time is stored as a 64-bit integer (`DAT_00acce64:68`) representing "game
ticks." The conversion from real-world time units to ticks uses a multiplier of
0x80 (128 ticks per real-time second).

| Constant | Value    | Meaning                           |
|----------|----------|-----------------------------------|
| 0x80     | 128      | Ticks per second (time multiplier) |
| 0x3C     | 60       | Seconds per minute                 |
| 0xE10    | 3,600    | Seconds per hour                   |
| 0x15180  | 86,400   | Seconds per day                    |
| 0x93A80  | 604,800  | Seconds per week (7 days)          |
| 0x24EA00 | 2,419,200| Seconds per month (28 days = 4 weeks) |
| 0xD5     | 213      | Time increment per game step (~1.66s) |
| 0x500    | 1,280    | Time increment for "wait" actions (~10s) |

**Calendar structure:**

| Unit   | Count | Notes                        |
|--------|-------|------------------------------|
| Second | 60/min | Standard                    |
| Minute | 60/hr  | Standard                    |
| Hour   | 24/day | Standard                    |
| Day    | 7/week | Named: Sunday--Saturday      |
| Week   | 4/month| 28 days per month            |
| Month  | 12/year| 336 days per year (12 x 28)  |

Day names are present as strings: `Sunday`, `Monday`, `Tuesday`, `Wednesday`,
`Thursday`, `Friday`, `Saturday` (at `004db908`--`004db8cc`).

**Time display format:** `%d:%02d %s` (12-hour with AM/PM), date as `%d %s %d`
(day month year). String at `004e45c0`.

The `TIME/CAL` key (string at `004e2ca8`) opens the time/calendar display,
and `sbdate-time` (at `004e2830`) is the status bar date/time UI element.

### Travel

Fast travel is handled through stables and boats (2D event building types):

| Building Type      | Example Names                    |
|-------------------|----------------------------------|
| Stables           | Human Stables01, Elf Stables, Warlock Stables, etc. |

Travel advances game time based on distance, consuming the appropriate number
of days. During travel, random encounters may occur.

### Day/Night Cycle

The sky color transitions between day and night values read from the INI file:

| INI Key              | Purpose                   |
|----------------------|---------------------------|
| `RGBDayTop.r/g/b`   | Daytime sky gradient top  |
| `RGBDayBottom.r/g/b` | Daytime sky gradient bottom |
| `RGBNightTop.r/g/b`  | Nighttime sky gradient top |
| `RGBNightBottom.r/g/b`| Nighttime sky gradient bottom |

These are read by `FUN_00466086` (INI loader). The current hour is derived from
game time to interpolate between day and night sky colors. Combat turn sounds
include `turnhour` (played when an hour passes during turn-based combat).

### Screen Modes (Party-Relevant)

| Value | Mode         | Trigger        |
|-------|-------------|----------------|
| 0x00  | Normal gameplay | Default     |
| 0x0D  | Rest screen  | `KEY_REST`    |
| 0x10  | Map screen   | Map key       |

### Party Start Points

When entering a new map, the party spawn position is set by `FUN_004498f8`:

| Direction Index | Decoration Name | Purpose          |
|-----------------|----------------|-------------------|
| 0               | `"Party Start"` | Default entry     |
| 1               | `"North Start"` | From north map    |
| 2               | `"South Start"` | From south map    |
| 3               | `"East Start"`  | From east map     |
| 4               | `"West Start"`  | From west map     |

The function searches map decorations (`DAT_0069ac50` count, `DAT_00683550` array,
32-byte stride) for matching trigger IDs, then writes to `DAT_00acd4EC`--`DAT_00acd4FC`.

If the event system has override coordinates (`DAT_005b6440 != 0`), those take
precedence over decoration-based positioning.

---

## Integration Points

### Save/Load

- `party.bin` (0x16238 bytes) in the save LOD contains the full party state
- `clock.bin` (0x28 bytes) stores the game timer state
- Written by `FUN_0045f4a2` (WriteSaveArchive)
- Party position is saved independently in the map data section

### Event System

| Opcode | Name              | Party Effect                    |
|--------|-------------------|---------------------------------|
| 0x06   | `EVT_MAP_TRANSITION` | Sets party position + map change |
| 0x07   | `EVT_GIVE_GOLD`  | Adds gold to party              |
| 0x08   | `EVT_SET_PLAYER_STAT` | Modifies a player variable   |
| 0x10   | `EVT_ADD_PLAYER_STAT` | Adds to a player variable    |
| 0x12   | `EVT_SUBTRACT_STAT` | Subtracts from a player variable |
| 0x39   | `EVT_GIVE_EXPERIENCE` | Awards XP to party           |
| 0x3A   | `EVT_TAKE_GOLD`  | Removes gold from party         |

### Timer System

Timer events interact with the party through the 64-bit game clock. Periodic events
(type 0x26) and absolute events (type 0x1F) fire based on calendar arithmetic
applied to `DAT_00acce64:68`. See [time-calendar.md](time-calendar.md) for full timer details.

### Map Transitions

On map change, `FUN_0044989e` saves current map state, updates `DAT_006be1c4`
(current map name), and sets `DAT_006a0bc8 = 2` (load pending). The loading screen
displays one of 5 random images (`loading%d.pcx`, 1--5) with an alignment-themed
progress bar. See [map-transitions.md](map-transitions.md) for details.

### Combat

`DAT_00acd6B4` flags turn-based combat mode. During combat, game time advances
per turn with `turnstart`/`turnstop`/`turn0`--`turn4` sounds. Character formation
(front/back rank) affects melee targeting. See [combat-system.md](combat-system.md).

---

## Integration notes

### Key Functions

| Address      | Suggested Name             | Size   | Purpose |
|-------------|---------------------------|--------|---------|
| `004304d6`  | `GameTick_ProcessAll`      | ~27K   | Main gameplay tick (movement, combat, events) |
| `00466086`  | `LoadIniSettings`          | 1,619  | Reads mm7.ini including party height/eye/walk |
| `004498f8`  | `SetPartyStartPoint`       | 372    | Sets party spawn from map decoration |
| `0041f66a`  | `InitRestScreen`           | varies | Rest screen UI setup |
| `0045f4a2`  | `WriteSaveArchive`         | 3,087  | Serializes party + map state to save LOD |
| `0045eec3`  | `DoSaveGame`               | 1,503  | Full save game operation |
| `00443fff`  | `ProcessTimerEvents`       | varies | Calendar-based event firing |
| `0044686d`  | `EventProcessor_Execute`   | 7,134  | Main event interpreter (handles gold, XP, etc.) |
| `0044989e`  | `MapTransition_Execute`    | 90     | Initiates map change |

### Global Variables Summary

| Address        | Name                | Type    | Description |
|----------------|---------------------|---------|-------------|
| `DAT_00acce3C` | PartyHeight         | i32   | Bounding height (default 192) |
| `DAT_00acce44` | PartyEyeLevel       | i32   | Camera Z offset (default 160) |
| `DAT_00acce64` | GameTimeLow         | u32  | Game clock low dword |
| `DAT_00acce68` | GameTimeHigh        | u32  | Game clock high dword |
| `DAT_00acd4EC` | PartyPosX           | i32   | World X |
| `DAT_00acd4F0` | PartyPosY           | i32   | World Y |
| `DAT_00acd4F4` | PartyPosZ           | i32   | World Z |
| `DAT_00acd4F8` | PartyFacing         | i32   | Yaw (0--2047) |
| `DAT_00acd4FC` | PartyPitch          | i32   | Pitch |
| `DAT_00acd500` | SavedPosX           | i32   | Respawn/recall X |
| `DAT_00acd504` | SavedPosY           | i32   | Respawn/recall Y |
| `DAT_00acd508` | SavedPosZ           | i32   | Respawn/recall Z |
| `DAT_00acd50C` | SavedFacing         | i32   | Respawn/recall yaw |
| `DAT_00acd520` | ViewZOffset         | i32   | Additional camera Z |
| `DAT_00acd538` | PhysicsZ            | i32   | Physics Z copy |
| `DAT_00acd6B4` | CombatMode          | i32   | Turn-based combat flag |
| `DAT_00acd6C0` | PartyAlignment      | i32   | 0=good, 1=neutral, 2=evil |
| `DAT_00507a6c` | ActiveCharIndex     | i32   | Selected character (0--3) |
| `DAT_005061c8` | SecondaryCharIndex  | i32   | Secondary selection |
| `DAT_004e28d8` | CurrentScreenMode   | i32   | UI mode (0x0D=rest, etc.) |

### Serialization Notes

- `party.bin` = 0x16238 bytes = party header (~0x100 bytes) + 4 x 0x1B3C character
  records (27,888 bytes) + remaining party state (inventory, quests, bounties, etc.)
- `clock.bin` = 0x28 = 40 bytes of timer/clock state
- The 64-bit game time value must be preserved exactly across save/load cycles

### Clean-Room Implementation Guidance

1. **Position storage**: Use a struct with named fields rather than raw address offsets.
   The X/Y/Z/facing/pitch group forms a natural `PartyPosition` sub-struct.

2. **Game clock**: Implement as a 64-bit tick counter. Provide conversion functions
   for tick-to-calendar and calendar-to-tick. The calendar is non-standard (28 days
   per month, 336 days per year).

3. **Height/eye level**: These should be configurable (INI-driven) with the documented
   defaults. The eye level determines the camera's vertical offset from the party's
   ground position.

4. **Alignment theming**: Use the alignment value (0/1/2) to select texture set suffixes
   (`-b`/none/`-c`) for all alignment-themed UI elements.

5. **Rest system**: The rest screen is a modal overlay. Implement food consumption,
   HP/SP regeneration, time advancement, and random encounter checks as separate
   testable functions.
