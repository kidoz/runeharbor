---
title: "Time and Calendar System"
summary: "A single tick counter drives calendar display, timers, conditions, travel, and lighting changes."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Time and Calendar System

A single tick counter drives calendar display, timers, conditions, travel, and lighting changes.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

> Original source files:
>
> - `D:\mm7Src_eng\MM7\Code\Game.cpp` (string at 0x004e7fd8)

---

## 1. Overview

MM7 uses a 64-bit integer game clock running at 128 ticks per real-time
second. All game time -- calendar display, event scheduling, condition
durations, travel delays, and day/night rendering -- derives from this
single monotonic counter.

The in-game calendar uses a custom structure:

- 60 minutes per hour
- 24 hours per day
- 7 days per week
- 4 weeks per month
- 12 months per year
- **28 days per month, 336 days per year**

---

## 2. Game Time Representation

### Tick rate

The fundamental unit is a **game tick**, with 128 ticks per real-time second.

```text
1 game second  = 128 ticks
1 game minute  = 128 * 60     = 7,680 ticks
1 game hour    = 128 * 3,600  = 460,800 ticks
1 game day     = 128 * 86,400 = 11,059,200 ticks

```

### Storage

Game time is stored as a 64-bit integer (two 32-bit DWORDs on the x86
platform). This allows a time range far exceeding the game's needs.

The current game time is maintained in a global variable that increments
each game frame by a delta proportional to the real-time elapsed.

### String references

- `"TIME/CAL"` (0x004e2ca8) -- Time/Calendar screen heading (FUN_004142de)
- `"sbdate-time"` (0x004e2830) -- Status bar date/time display (FUN_00411c07)
- `"turnhour"` (0x004e44d4) -- Turn-hour advancement (FUN_0042f3b2)
- `"KEY_TIMECAL"` (0x004e9068) -- Keybinding for time/calendar screen

---

## 3. Calendar Structure

### Time units

| Unit | Value | Relationship |
|------|-------|-------------|
| Minute | 1 minute | Base display unit |
| Hour | 60 minutes | |
| Day | 24 hours | |
| Week | 7 days | |
| Month | 4 weeks = 28 days | |
| Year | 12 months = 336 days | |

### Day names

Seven days of the week (strings at 0x004db8cc-0x004db908):

| Index | Name |
|-------|------|
| 0 | Sunday |
| 1 | Monday |
| 2 | Tuesday |
| 3 | Wednesday |
| 4 | Thursday |
| 5 | Friday |
| 6 | Saturday |

### Month names

Twelve months of the year (strings at 0x004db834-0x004db888):

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

Note: An abbreviated form also exists at 0x004db784:
`"JanFebMarAprMayJunJulAugSepOctNovDec"`

### MM7 calendar conversion

To convert a raw tick count to calendar components:

```text
totalMinutes  = ticks / (128 * 60)
minute        = totalMinutes % 60
totalHours    = totalMinutes / 60
hour          = totalHours % 24
totalDays     = totalHours / 24
dayOfWeek     = totalDays % 7
dayOfMonth    = totalDays % 28        (0-based, display as 1-based)
totalMonths   = totalDays / 28
month         = totalMonths % 12
year          = totalMonths / 12

```

### Starting date

The game begins on a specific date. The initial game time value encodes the
starting day, month, and year of the adventure. By convention, the game
starts in the morning of the first day.

---

## 4. Day/Night Cycle

The day/night cycle is driven by the hour component of the current game time.
The outdoor renderer uses time-of-day to interpolate between day and night
sky colors and adjust ambient lighting.

### Sky color interpolation

Sky colors are defined in the INI configuration (`[outdoor]` section):

| Setting | Default RGB | Purpose |
|---------|------------|---------|
| `RGBDayTop` | (81, 121, 236) | Day sky top color |
| `RGBDayBottom` | (153, 193, 237) | Day sky bottom color |
| `RGBNightTop` | (0, 0, 0) | Night sky top color |
| `RGBNightBottom` | (11, 41, 129) | Night sky bottom color |

These color values are stored in globals at `DAT_006bdf88` through
`DAT_006bdf93` (6 bytes for top RGB + 6 bytes for bottom RGB, for both
day and night).

The INI keys use component suffixes:

- `RGBDayTop_r`, `RGBDayTop_g`, `RGBDayTop_b` (strings at 0x004ea254-0x004ea26c)
- `RGBDayBottom_r`, `RGBDayBottom_g`, `RGBDayBottom_b` (0x004ea224-0x004ea244)
- `RGBNightTop_r`, `RGBNightTop_g`, `RGBNightTop_b` (0x004ea1f4-0x004ea214)
- `RGBNightBottom_r`, `RGBNightBottom_g`, `RGBNightBottom_b` (0x004ea1b8-0x004ea1e0)

### Lighting modulation

Indoor lighting uses sector-based ambient light plus time-of-day modulation.
The lighting setup function `FUN_00467d8c` adjusts ambient levels based on
RGB values at `DAT_00ae306c` / `DAT_00ae3068` / `DAT_00ae3064` combined with
time-of-day factors.

### Dawn/dusk transitions

The game smoothly interpolates between day and night sky colors during
dawn and dusk hours. The exact transition hours are encoded in the rendering
pipeline, typically:

- Dawn: ~5:00 - 6:00
- Dusk: ~20:00 - 21:00

During transition, the sky color is linearly blended between night and day
palettes.

---

## 5. Timer Events

### Turn-hour system

The string `"turnhour"` (0x004e44d4) at FUN_0042f3b2 indicates a timer-based
event that fires every game hour. This drives periodic game effects:

- Condition progression (poison ticking, disease advancement)
- Buff/debuff duration countdown
- NPC schedule changes
- Weather updates

### Frame timer

| Address | Purpose |
|---------|---------|
| `DAT_006e2028` | Frame timer (real-time tick count) |
| `DAT_006e202c` | Frame counter (incremented each frame) |

The frame timer uses `GetTickCount()` / `timeGetTime()` for real-time
measurement, which is then converted to game ticks based on the game speed
multiplier.

### Event scheduling

Game events can be scheduled for future execution by storing a target game
time. When the current game time exceeds the target, the event fires. This
is used for:

- Spell duration expiry
- Quest deadlines
- NPC availability windows
- Shop restock timers

---

## 6. Rest Mechanics

### Rest screen

The rest interface (window type 26) is created by FUN_0041f66a with these
UI elements:

| Texture | Action |
|---------|--------|
| `restmain` | Background |
| `restb1` | Rest until healed |
| `restb2` | Rest for specific duration |
| `restb3` | Wait (no rest, just pass time) |
| `restb4` | Additional rest option |
| `restexit` | Exit rest mode |

### Key binding

- `KEY_REST` (0x004e9074) -- Opens the rest screen

### Rest time advancement

When resting, the game advances the clock by the specified number of hours.
Each hour of rest:

1. Advances the game time by one hour (460,800 ticks)
2. Applies per-hour healing (HP/SP recovery based on character stats)
3. Checks for random encounters (outdoor rest, dungeon rest)
4. Processes condition changes (disease, aging, etc.)
5. Triggers any scheduled events that fall within the rest period

### Sleep condition

The string `"asleep"` (0x004e8968) at FUN_0045504a indicates a character
condition that can occur during rest (e.g., from a Sleep spell or
random event). Characters with the "asleep" condition cannot act in
combat until woken.

---

## 7. Travel Time

### Map transitions

When traveling between outdoor maps, the game advances time based on the
travel distance. The transport data table (`trans.txt`) defines travel
times between locations.

### Transport system

The `trans.txt` data file (loaded at 0x476650 via `FUN_00476650`) defines
transport routes with associated time costs. Travel by:

- **Walking:** Time based on distance between map cells
- **Boat:** Fixed time per route (defined in trans.txt)
- **Stable/Horse:** Fixed time per route
- **Lloyd's Beacon:** Instantaneous (no time cost)
- **Town Portal:** Instantaneous (no time cost)

---

## 8. Condition Durations

Many character conditions are tracked with timestamps:

### Time-based conditions

Conditions that use the game clock for duration or progression:

- **Poison** -- Periodic damage per hour
- **Disease** -- Progressive weakening per hour
- **Buff spells** -- Expire after duration (minutes to hours)
- **Temporary stat modifiers** -- Expire at specific game time
- **Shop restock** -- Items refresh after a set number of game days

### Lifetime tracking

The strings `"FTLifetime"` and `"Lifetime"` (0x004e8eac, 0x004e8eb8)
in the object list parser (FUN_0045915c) indicate that game objects
(projectiles, spell effects, decorations) have lifetime durations
measured in game time.

---

## 9. Status Bar Time Display

The status bar at the bottom of the game screen displays the current
date and time. The `"sbdate-time"` texture (0x004e2830) is the background
for this display element.

**FUN_00411c07** (3837 bytes) renders the status bar including:

- Day of week name
- Day number (1-28)
- Month name
- Year
- Current hour and minute

---

## 10. Key Functions

| Address | Size | Proposed Name | Purpose |
|---------|------|---------------|---------|
| 0x00411c07 | 3837 | `UI::DrawStatusBarDateTime` | Render date/time in status bar |
| 0x004142de | 2641 | `UI::DrawTimeCalendarTab` | Draw TIME/CAL screen tab |
| 0x0041f66a | - | `UI::CreateRestScreen` | Create rest screen with buttons |
| 0x0042f3b2 | - | `Game::ProcessTurnHour` | Process hourly game events |
| 0x0045504a | - | `Condition::CheckAsleep` | Check/apply asleep condition |
| 0x0045a035 | - | `Input::MapKeyTimeCal` | Map keybinding for time/calendar |
| 0x0045a999 | - | `Input::MapKeyRest` | Map keybinding for rest |

---

## 11. Key Globals

| Address | Purpose |
|---------|---------|
| `DAT_006bdf88` | Day sky top color (R, G, B bytes) |
| `DAT_006bdf8e` | Day sky bottom color (R, G, B bytes) |
| `DAT_006bdf88+6` | Night sky top color (R, G, B bytes) |
| `DAT_006bdf88+12` | Night sky bottom color (R, G, B bytes) |
| `DAT_006e2028` | Frame timer (real-time) |
| `DAT_006e202c` | Frame counter |
| `DAT_00ae3064` | Ambient light B component |
| `DAT_00ae3068` | Ambient light G component |
| `DAT_00ae306c` | Ambient light R component |

---

## Integration notes

### Time representation

Use a 64-bit integer with 128 ticks/second resolution, matching the original.
This preserves compatibility with save game time values and event timing.

```cpp
using GameTicks = int64_t;

constexpr GameTicks TICKS_PER_SECOND = 128;
constexpr GameTicks TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
constexpr GameTicks TICKS_PER_HOUR   = TICKS_PER_MINUTE * 60;
constexpr GameTicks TICKS_PER_DAY    = TICKS_PER_HOUR * 24;

constexpr int DAYS_PER_WEEK  = 7;
constexpr int DAYS_PER_MONTH = 28;
constexpr int MONTHS_PER_YEAR = 12;
constexpr int DAYS_PER_YEAR  = DAYS_PER_MONTH * MONTHS_PER_YEAR;  // 336

```

### RuneHarbor calendar conversion

```cpp
struct GameDateTime {
    int minute;      // 0-59
    int hour;        // 0-23
    int dayOfWeek;   // 0-6 (Sunday=0)
    int dayOfMonth;  // 0-27 (display as 1-28)
    int month;       // 0-11
    int year;        // 0+
};

GameDateTime ticksToDateTime(GameTicks ticks) {
    auto totalMinutes = ticks / TICKS_PER_MINUTE;
    auto minute = totalMinutes % 60;
    auto totalHours = totalMinutes / 60;
    auto hour = totalHours % 24;
    auto totalDays = totalHours / 24;
    auto dayOfWeek = totalDays % 7;
    auto dayOfMonth = totalDays % 28;
    auto totalMonths = totalDays / 28;
    auto month = totalMonths % 12;
    auto year = totalMonths / 12;
    return { minute, hour, dayOfWeek, dayOfMonth, month, year };
}

```

### Key considerations

1. **Save compatibility:** The 64-bit tick value must be stored in save
   files using the same format as the original (two 32-bit little-endian
   DWORDs).

2. **Time advancement:** Rest and travel advance game time in bulk. The
   implementation must process all per-hour events that fall within the
   advancement period, not skip them.

3. **Day/night rendering:** The sky color interpolation should use the
   hour (and optionally minute) to smoothly blend between day and night
   palettes. The original uses linear interpolation.

4. **Speed multiplier:** The game may run at different speeds (normal,
   fast-forward during rest). The tick advancement per frame must account
   for this multiplier.

5. **28-day months:** All 12 months have exactly 28 days. There are no
   irregular month lengths, leap years, or other calendar complications.

*All trademarks belong to their respective owners.*
