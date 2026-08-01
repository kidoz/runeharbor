---
title: "Turn-Based Combat"
summary: "Turn-based combat layers a countdown queue and discrete action cycle over shared combat rules."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Turn-Based Combat

Turn-based combat layers a countdown queue and discrete action cycle over shared combat rules.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

> Companion to [combat-system.md](combat-system.md) (damage/hit/resistance) and [monster-ai.md](monster-ai.md) (AI)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Combat Mode Toggle](#2-combat-mode-toggle)
3. [The Turn Queue Object](#3-the-turn-queue-object)
4. [Initiative / Turn Order](#4-initiative-turn-order)
5. [Turn Structure](#5-turn-structure)
6. [Action Resolution in TB](#6-action-resolution-in-tb)
7. [Monster Turns in TB](#7-monster-turns-in-tb)
8. [Combat Entry / Exit](#8-combat-entry-exit)
9. [Key Globals](#9-key-globals)
10. [Function Map](#10-function-map)
11. [Integration notes](#integration-notes)
12. [Open questions](#open-questions)

---

## 1. Overview

MM7 combat is a **hybrid** system. The party exists in one of two modes tracked by the
global `DAT_00acd6b4`:

- **Real-time (RT)** — `DAT_00acd6b4 == 0`. All actors update every frame via
  `FUN_00401a91` (`AI_UpdateAll`). Recovery timers gate action frequency continuously.
- **Turn-based (TB)** — `DAT_00acd6b4 == 1`. The same `AI_UpdateAll` dispatcher at
  `0x00401c25` detects TB mode, calls `FUN_00405e14` (`AI_TurnBasedUpdate`), and skips
  the real-time per-actor loop entirely.

Switching is **manual only**: the player presses the combat key. Proximity aggro does
**not** auto-enter TB — it only triggers real-time combat. TB is entered/exited by the
same toggle. Pressing the "pass" key while in TB advances the round.

The damage pipeline (`DamageMonsterFromParty` at `0x00439463`,
`DamagePlayerFromMonster` at `0x00439fee`) is **shared** between RT and TB. The only
TB-specific differences the damage functions observe are:

- Stun duration: fixed `20` ticks in TB vs `__ftol(real_time_value)` in RT
  (`0x00439c58` in `DamageMonsterFromParty`, `0x0043a353`/`0x0043a875` in
  `DamagePlayerFromMonster`).
- The TB branch gates *who acts when* via the turn queue; the damage math itself is
  mode-independent.

---

## 2. Combat Mode Toggle

### Keybind

The toggle is bound to the `KEY_COMBAT` ini key (string `0x004e90d8`). It is registered
in two places:

- `fcn.0045a035` @ `0x0045a2b2` — primary binder. Reads the ini value; if absent, falls
  back to a hardcoded default of `0x58` stored at `[esi+0x20]`:

```asm
  0x0045a2a5  mov dword [esi + 0x20], 0x58   ; 'X' = 88 (default KEY_COMBAT)
  ```

- `fcn.0045a999` @ `0x0045aa56` — re-binder (applied on config reload).

**Default key: 'X'.** Related combat keybinds (all in the same input-config struct,
registered back-to-back at `0x0045aa40`+):

| Ini Key        | Default | Purpose                                    |
|----------------|---------|--------------------------------------------|
| `KEY_COMBAT`   | `X`     | Toggle RT/TB combat mode                   |
| `KEY_PASS`     | (ini)   | Pass/skip current character's turn         |
| `KEY_ATTACK`   | (ini)   | Active character attacks                   |
| `KEY_CAST`     | (ini)   | Open spell cast UI                         |
| `KEY_CASTREADY`| (ini)   | Quick-cast ready spell                     |
| `KEY_YELL`     | (ini)   | Character yell/emote                       |
| `KEY_EVENTTRIGGER` | (ini) | Trigger event (space)                    |
| `KEY_JUMP`     | (ini)   | Jump (party)                               |

(Defaults for the non-combat keys are read from `mm7.ini`; only `KEY_COMBAT` has a
baked-in fallback in the binary.)

### Toggle Handler

The input dispatcher `fcn.0042fc2a` switches on the logical action index
(`var_10h_2`, range 0–29 = 30-entry jump table at `0x0043045e`). Combat is **case 6**
(enter TB) and **case 7** (pass / next round), both gated on the UI being in the main
gameplay screen (`[0x004e28d8] == 0x10`).

**Case 6 — Enter TB** (`0x00430014`):

```asm
0x00430014  cmp [0x4e28d8], edi          ; must be in main screen (16)
0x00430020  cmp [0xacd6b4], ebx          ; currently RT? (ebx = 0)
0x00430028  cmp [0x4f86dc], 3            ; queue phase check
0x00430041  push ebx                     ; arg = 0 (no "turnstart" sound yet)
0x00430047  mov [0xacd6b4], edi          ; SET MODE = 1 (TB)   <-- the write
0x0043004d  call fcn.00405cff            ; EnterTurnBased(arg=0)

```

If already in TB (`[0xacd6b4] != 0`), case 6 instead calls `fcn.004059db`
(`StartNextRound`) and writes `[0xacd6b4] = 0` — i.e. **pressing X while in TB exits TB**
back to RT (`0x00430057`–`0x00430061`). So X is a true toggle.

**Case 7 — Pass / Advance Round** (`0x0043006c`): active only when already in TB. Triggers
`StartNextRound` via the queue-advance path (see §5).

> Note: `edi` is `1` and `ebx` is `0` on entry to case 6 (set up by the dispatcher
> prologue), so `mov [0xacd6b4], edi` sets the flag to 1.

### What Happens Visually / Audibly on Entry

`EnterTurnBased` (`fcn.00405cff`, 277 bytes) does, in order:

1. Clears the queue's "pending" field (`[edi+4] = 0`) and iterates the existing queue,
   clearing bit 7 (`& 0x7f`) of each referenced actor's flag byte at
   `0x5feffc + idx*0x344` (drops the "in-combat" mark from a previous round).
2. For the NPC list at `0x6650ca` (stride `0x70`), clears bit 2 of each NPC's status word
   (NPCs are excluded from TB).
3. Recomputes each queue entry's TB recovery: `int(entry.value) * 2.1333` (the double at
   `0x004d8438` = `0x4001111111111111` = **32/15 ≈ 2.1333**), via `fild; fmul; call
   fcn.004ca74c` (ftol). Player results land at `player + 0x1934`; actor results at
   `0x5ff054 + idx*0x344` (actor recovery timer, `+0x58`).
4. Plays a sound via `fcn.004ab69f`; if `arg_8h != 0`, additionally plays sound **0xce
   (206) = `turnstart`** via `fcn.004aa29b`.
5. Clears bit 1 of `[0x4f86f0]` (a combat-internal flag).
6. Calls `fcn.00426349` (UI refresh).
7. Zeroes `[0x50c814]` (round tick counter) and `[0x50c818]` (party round speed).

`StartNextRound` (`fcn.004059db`) plays sound **0xcf (207) = `turnstop`** at `0x0045a13`
when a round ends/advances.

**Cursor / UI:** No explicit cursor-shape change is performed inside the toggle path.
The HUD reflects TB via the round-counter globals `[0x50c814]`/`[0x50c818]` (consumed by
`fcn.00441987`, the TB clock/round display). The earlier note in [combat-system.md](combat-system.md)
claiming "UI mode set to 3" appears inaccurate — `[0x4e28d8]` (UI window mode) is **not**
written to 3 by the toggle; it remains `0x10` (main gameplay). TB state is indicated to
the rest of the engine purely via `DAT_00acd6b4` and the queue phase `DAT_004f86dc`.

### Forced Exit (Map Transition)

The gameplay loop `FUN_00463186` has two paths (`0x00463257` and `0x004635d9`) that, when
the encounter/map state tears down, call `EnterTurnBased` and then immediately write
`[0xacd6b4] = 0`. These are **map-transition forced exits** from TB, not re-entries — the
flag is cleared right after the call.

---

## 3. The Turn Queue Object

TB ordering is driven by a single queue object at **`0x004f86d8`** (thiscall, passed in
`ecx`). Layout (offsets relative to `0x4f86d8`):

| Offset | Size | Field            | Meaning                                                     |
|--------|------|------------------|-------------------------------------------------------------|
| `+0x00`| 4    | (header)         | Cleared to 0 on round start                                 |
| `+0x04`| 4    | `state`          | Per-entry action state during the current turn (0/1/2/3)    |
| `+0x08`| 4    | `counter`        | Action sub-counter; initialised to `0x40` (64) on round start|
| `+0x0c`| 4    | `count`          | Number of entries currently in the queue                    |
| `+0x10`| 4    | `actionId`       | Current action id; reset to `0x64` (100) on round start     |
| `+0x14`| 4    | (aux)            |                                                             |
| `+0x18`| 4    | `flags`          | Bit 0, bit 1, bit 2 (=`in-combat`), bit 3 (="processed")    |
| `+0x1c`| 4    | (aux)            |                                                             |
| `+0x20`| 16*N | `entries[N]`     | The queue entries (see below)                               |

Each **entry is 16 bytes**:

| Offset | Field      | Meaning                                                          |
|--------|------------|------------------------------------------------------------------|
| `+0x00`| `id`       | Low 3 bits = type (`3` = actor, `4` = player); bits 3+ = index   |
| `+0x04`| `init`     | Initiative/sort key (lower = acts earlier)                       |
| `+0x08`| `field8`   | Scratch (cleared on some ops)                                    |
| `+0x0c`| `fieldc`   | Round-robin slot tag (`2` = active slot)                         |

The "next entry to act" field is at `queue + 0x20` (the first entry). The currently
active player index is mirrored to `DAT_00507a6c` (`= idx+1` when a player acts, `0`
otherwise).

The queue-phase global **`DAT_004f86dc`** tracks queue progress:

- `0` — idle / not in a turn
- `1` — a turn is in progress
- `2` — waiting to advance to the next actor (the state `QueueAdvance` checks)
- `3` — a specific sub-phase used by several input cases

---

## 4. Initiative / Turn Order

### It Is a Countdown/Side-Sort Hybrid, NOT a Pure Speed Sort

MM7 TB uses an **initiative-countdown** scheme. Each entry carries an `init` value; the
queue is kept sorted ascending by `init`, and the head entry is the one whose turn is
current. After an entry acts, its `init` is recomputed from the actor's/player's recovery
stat and the queue is re-sorted.

### Sort: `fcn.00404544` (QueueSort, ~460 bytes)

Insertion sort over the entries. Comparison key at `0x0040462b`–`0x00404666`:

1. Compare `entry[i+1].init` (`[ecx+4]`) vs `entry[i].init` (`[eax+4]`). Lower `init`
   sorts earlier (acts first).
2. On tie (`init` equal):
   - If `entry[i+1]` is a player (type 4) and `entry[i]` is an actor (type 3), the
     **actor wins** (actors act before players at equal initiative).
   - Otherwise compare the index portion (`id & ~7`); lower index first.

After sorting, the function sets `[0x507a6c]` from the new head entry (player idx+1 or 0),
sets/clears flag bit 2 (`0x4`), and for each player entry scales `init` by the double at
`0x004d8468` (`0x3fde000000000000` ≈ **0.46875** … actually `1.8671875`) and writes the
result to `player[0xacf138 + idx*0x1b3c]` — the per-player TB recovery display value.

> Ambiguity: `0x3fde000000000000` decodes to `1.8671875` (119/64), an odd constant. It is
> only used to derive a *display* figure for the active player's recovery bar, not the
> sort key itself. The sort key for players is computed elsewhere (see below).

### Initiative Computation: `fcn.0040652a` (RecomputeInit, 134 bytes)

Called from `AI_TurnBasedUpdate` after an actor acts (`0x00405ffa`). For each entry:

- If the entry's `fieldc` (`[edx]`) is already nonzero, skip.
- Player entries (type 4): exit early (their init is set by `StartNextRound` /
  `QueueAdvance`).
- Actor entries (type 3): look up `actor+0xb0` (current TB action state). If it is one of
  `{0, 7, 9}` (idle/attacking-melee/attacking-ranged), read the actor's monster-type
  recovery from the table at `0x5ccd10[type * 0x58]` and store it as the new `init`.
  If the actor has an active haste-style timer (`actor+0x144`/`+0x148` > 0), **double**
  the init (`lea eax, [esi+esi]`).

The monster recovery table `0x5ccd10` (stride `0x58` = 88 bytes per monster type) holds
the per-type TB recovery/AP value. (This is the same `0x58`-stride per-type table noted in
[monster-ai.md](monster-ai.md) at `DAT_005cccd1`; `0x5ccd10` is an adjacent field within each entry.)

### Advance to Next Actor: `fcn.00406457` (QueueAdvance, 211 bytes)

Called from `QueueAdvanceIfInTurn` (`fcn.0040471c`) after each player action when
`[0xacd6b4]==1` && `[0x4f86dc]==2`. Given the index `arg_10h` of the entry that just acted:

1. Reads the **next** entry `queue[(idx+2)*16]`.
2. Player (type 4): clears a flag at `0xae2f7c[idx*4]`; if it was set, recomputes the
   player's recovery via `fcn.0048e19b` (`Player_CalculateSpellDamage` reused as a
   generic "player action speed" computer), clamped to a **minimum of 30 (`0x1e`)**.
3. Actor (type 3): looks up monster-type recovery from `0x5ccd10` via
   `0x5ff038 + idx*0x344` (the actor's type field).
4. Stores the result into the next entry's `init` field (`queue[edi+0x24]`).
5. Calls `QueueSort` (`fcn.00404544`).
6. Sets `[0x507a6c]` from the new head, sets `[0x576eac] = 1` (turn-changed flag).
7. **Countdown loop** (`0x004064fa`–`0x00406522`): walks the sorted entries and decrements
   successive `init` counters until it finds the next entry ready to act. This is the
   "tick down until someone's initiative is up" step.

### Initiative Summary

| Actor type | Initiative source                                                |
|------------|------------------------------------------------------------------|
| Player     | `Player_CalculateSpellDamage` result (weapon/speed-based), min 30|
| Monster    | `0x5ccd10[monsterType * 0x58]` (per-type TB recovery)            |
| Hasted monster | `* 2` (longer gap — note: this delays the monster, odd as it reads) |

The system is **round-based** (everyone is in the queue once per round) with a
**continuous-initiative** flavour: after each action the queue is re-sorted by remaining
initiative, so faster actors can act more frequently within the round. There is **no
action-point system**.

---

## 5. Turn Structure

### Round Lifecycle

A round is started by `StartNextRound` (`fcn.004059db`, 804 bytes), invoked from the
toggle/pass handler. It:

1. Clears bit 1 of `[0x4f86f0]`; calls `fcn.0042632f` (time/bookkeeping); plays
   `turnstop` (sound 0xcf).
2. Computes the **party round speed**: reads `[0x50c810]` (active member index) → player
   record at `0xa74f68` → the word at `player + 0x1a` (a speed/recovery stat), `* 8` →
   `[0x50c818]`.
3. Resets the queue header: `actionId=0x64`, `counter=0x40`, `state=1`, `count=0`.
4. **Adds alive players**: iterates `0xacd804` (player base, stride `0x1b3c`) up to
   `0xad44f4`; for each player passing `fcn.00492c03` (alive check), appends an entry
   `id = (idx << 3) | 4`. Also zeroes `0xae2f7c[idx*4]`.
5. **Adds visible hostile monsters**: iterates the sorted visible-actor list at
   `0x4f7c30` (count `[0x4f7458]`). For each actor that passes `fcn.0040894b`
   (active check) **and** has bit 15 set in `actor+0x24` (hostile flag), it sets bit 7
   (`|= 0x80`) of `actor+0x24` (the "in TB combat" mark), computes distance via
   `fcn.004040e9`, and appends `id = (idx << 3) | 3`.
6. Sorts a local copy of actor indices (insertion sort at `0x405c7c`), assigns round-robin
   slot tags, then calls `QueueSort` (`fcn.00404544`) for the real ordering.

So the queue is **rebuilt from scratch each round** from alive players + visible hostile
actors. Dead/fled actors drop out; newly aggroed actors join.

### Per-Turn Actions

`AI_TurnBasedUpdate` (`fcn.00405e14`) dispatches per entry based on `entry.state`
(`[esi+4]`) and `entry.counter` (`[esi+8]`) at `0x00405fab`:

| `state` | Counter condition            | Handler          | Meaning                          |
|---------|------------------------------|------------------|----------------------------------|
| 1       | `[esi+8] == 0x40` (initial)  | `fcn.00406a63`   | Approach/close in (first action) |
| 1       | otherwise, `[esi+8] > 0`     | `fcn.00406afe`   | CanAct check (see below)         |
| 2       | (after can-act)              | `fcn.00406051` + `fcn.0040652a` or `fcn.004061ca` | Select target & act |
| 3       | `[esi+8] == 0x40`            | `fcn.00406a63`   | Approach                         |
| 3       | otherwise                    | `fcn.00406b9f`   | Wait/idle pass                   |

After each handler, the frame delta `[0x50ba7c]` is subtracted from `[esi+8]`
(`0x00406046`–`0x0040604b`) — this is the **TB action-countdown** that consumes the
counter (set to 64 at round start) over successive frames.

### Player Actions Per Turn

From the input cases and the TB action functions, a player character on their turn can:

- **Attack** (`KEY_ATTACK`) — melee or ranged depending on equipped weapon; routes through
  the player-attack dispatcher `fcn.0042ebca` which calls the shared damage pipeline.
- **Cast spell** (`KEY_CAST`) — opens the spell UI; on cast, resolves via
  `Player_CalculateSpellDamage` and the shared spell-damage path.
- **Use item** — via the inventory UI (unchanged from RT).
- **Pass / Wait** (`KEY_PASS`, case 7) — skip turn, advance queue.
- **Quick-cast ready spell** (`KEY_CASTREADY`), **Yell** (`KEY_YELL`).
- **Move** — party movement keys still function in TB (the party can reposition; see §6).
- **Flee** — there is no dedicated "flee" action; disengaging is done by pressing `X` to
  return to RT and running away, or by moving out of aggro range.

Each action ends the character's turn and triggers `QueueAdvanceIfInTurn`
(`fcn.0040471c`) → `QueueAdvance` (`fcn.00406457`) → next entry.

### Recovery in TB vs RT

- **RT:** recovery is continuous; `[0x50ba54]` (frame delta ticks) is added to each
  actor's AI timer and subtracted from its recovery timer every frame. Actions are
  available as soon as the recovery timer reaches 0.
- **TB:** recovery becomes the **initiative value**. On round start each entry gets an
  `init` derived from its recovery stat (`* 2.1333` for the initial fill in
  `EnterTurnBased`, then per-type/`Player_CalculateSpellDamage` values on subsequent
  advances). The queue is sorted by remaining `init`; the countdown loop in
  `QueueAdvance` reduces `init` across entries until the next actor is "ready." So
  recovery gates **turn frequency within the round**, not real-time spacing.

---

## 6. Action Resolution in TB

Damage/hit/resistance resolution is **identical to RT** (see [combat-system.md](combat-system.md)
§§3–6). The TB-specific behaviour is timing and targeting only.

### Attack

Player attack dispatch: `fcn.0042ebca` (called from the input handler for the active
character). It:

1. Validates the active player (`0x507a6c`) via `fcn.00492c03`.
2. Computes damage via `fcn.0048e19b` (`Player_CalculateSpellDamage`, reused as the
   generic player-damage calc), clamped to **minimum 30** for the recovery value.
3. RT branch (`[0xacd6b4]==0`, `0x0042ec64`): computes continuous recovery
   `fild [recovery] * [0x6be224] * 2.1333` and stores via `fcn.0048e962`.
4. TB branch: skips that recovery computation.
5. Calls `fcn.0040471c` (`QueueAdvanceIfInTurn`) → advances the queue.

The actual hit roll / damage / armor-class reduction is then handled by
`DamageMonsterFromParty` (`0x00439463`) exactly as in RT.

### Spell

Casting on your turn resolves the spell effect immediately through the shared spell
pipeline (`Player_CalculateSpellDamage`, then `DamageMonsterFromSpellObject` etc.). Spells
do not "linger" until a later phase — resolution is immediate, only the *casting
opportunity* is gated by the turn.

### Movement

**TB allows repositioning.** `FUN_0040680f` (Move, 596 bytes) is the TB movement handler.
It iterates queue entries; for actor entries not in states {Dead(5), Fleeing(4),
Stunned(0xb), Stoned(0x13), Paralyzed(0x11)} it calls `FUN_00401221`
(`AI_FindNearestTarget`) and moves the actor via the standard movement subroutines. The
party's own movement keys remain active. So unlike some_blob/grid TB games, **MM7 TB is
not static** — both sides can close distance, kite, and reposition on their turns. (This
matches MM6/MM7's continuous-coordinate combat; there is no grid.)

### Flee

There is no dedicated flee-from-combat command. Fleeing is emergent:

- Press `X` to return to RT, then run; monsters will pursue via the RT AI and eventually
  disengage when distance exceeds the aggro threshold.
- A monster in Fleeing state (4) that survives until its recovery timer expires is
  transitioned to Dead (5) and despawns (per [monster-ai.md](monster-ai.md)); the same can happen to
  party-adjacent actors.

---

## 7. Monster Turns in TB

### Same Queue, Same Order

Monsters are full participants in the turn queue (added as type-3 entries in
`StartNextRound`). They take their turns interleaved with players in initiative order —
there is no "all monsters act, then all players" phase.

### TB AI: `FUN_00405e14` (AI_TurnBasedUpdate, 573 bytes)

Called from `AI_UpdateAll` (`0x00401c33`) when `[0xacd6b4]==1`, *instead of* the real-time
per-actor loop. It iterates the actor array (`0x5ff0ac` base, stride `0x344`) and for each
actor:

1. Skips actors whose condition timers at `+0x30`/`+0x34` indicate dead/incapacitated.
2. Runs 22 (0x16) condition-expiry slots via `fcn.00458603`
   (`Actor_ProcessConditions`).
3. Re-reads the monster-type base AI field from `0x5fefd0[type*0x98 - 0x98]` into
   `actor+0x8a` (resets the per-round AI field).
4. Skips actors with active condition timers at `+0x24` bit 7, `+0x124`/`+0x128`,
   `+0x134`/`+0x138`.
5. Accumulates `[actor+0xb8] += [0x50ba54]` (frame delta) and compares to
   `[actor+0xa0]` (the actor's recovery threshold). If below threshold, the actor does
   nothing this frame.
6. Once the threshold is reached, computes direction (`fcn.004040e9`) and dispatches to
   the appropriate TB action handler (see §5 table): melee (`fcn.00402f87`),
   spell (`fcn.00404ac7`), approach (`fcn.00406a63`), ranged (`fcn.00406fa8`), or
   wait (`fcn.00406b9f`).

Then it processes the **queue head** entry (the dispatch at `0x00405fab`) to drive the
current actor's turn.

### Behaviour Differences: RT vs TB

The underlying attack/movement subroutines are **the same functions** used in RT
(`AI_MeleeAttack`, `AI_RangedAttack`, `AI_CastSpellComplex`, `AI_PathfindMove`, etc.).
The differences are:

- **Pacing:** in TB, actions fire only on the actor's turn (initiative-gated); in RT they
  fire as soon as recovery permits.
- **Stun duration:** fixed 20 ticks in TB vs float-derived in RT (see §1).
- **Target selection:** `FUN_00406051` (TB target select, 377 bytes) and
  `FUN_004065b0` (best-action eval, 152 bytes) are TB-specific wrappers that call into the
  same `AI_FindNearestTarget` / `AI_SelectAttackType` core.
- Monsters get the **same action set** (melee, ranged, spell, move, special abilities) in
  both modes.

### Monster Groups

There is **no group-acts-together** mechanic. Each actor is an independent queue entry and
acts on its own initiative. Alliance groups (the `+0x60` faction and `+0x2C4` team fields,
per [monster-ai.md](monster-ai.md) §4) affect *hostility* (who is a valid target), not turn ordering.
All A/B/C variants of a monster type share the same per-type recovery value in
`0x5ccd10`, so they tend to act at similar initiative, but they are scheduled individually.

---

## 8. Combat Entry / Exit

### Entry (proximity aggro → RT, manual → TB)

- **Proximity aggro** is evaluated by `FUN_0042f4b6` (`CheckForCombatEncounter`, 275
  bytes). Distance thresholds:

  | Map type | Threshold | Hex     |
  |----------|-----------|---------|
  | Outdoor  | 5,120     | `0x1400`|
  | Indoor   | 2,560     | `0x0A00`|

  (`0x6be1e0 == 1` → indoor.) The function iterates all actors, skips states
  `{5, 4, 0xb, 0x13, 0x11}` (dead/fleeing/stunned/stoned/paralyzed), checks the active
  flag (bit 3 at `actor-0x8a`), computes the octahedral distance
  (`max + min/4 + mid*11/32`), and checks hostility via `FUN_0040104c`. Returns 1 if any
  hostile monster is in range.

- **Crucially, aggro does not set `DAT_00acd6b4`.** It only enables real-time combat
  behaviour (monsters pursue/attack in RT). The only writers of `DAT_00acd6b4` are:
  - The toggle handler (`0x430047` → 1, `0x430061` → 0).
  - The map-transition teardown in the gameplay loop (`0x463270`, `0x4635eb` → 0).
- **TB entry is therefore always manual** (press X from RT).

- **Ambush / event-triggered combat:** the event engine (opcodes in [event-engine.md](event-engine.md))
  can set monster hostility/HP/AI (`EVT_SET_MONSTER_HOSTILE` 0x31, etc.), which then
  causes aggro, but still in RT unless the player toggles. No event opcode was found that
  directly forces `DAT_00acd6b4 = 1`.

### Exit

- **All monsters dead / not in range:** `CheckForCombatEncounter` returns 0; RT combat
  effectively ends (monsters stop pursuing). TB is unaffected unless the player presses X.
- **All party dead/unconscious:** handled by the party/condition system
  ([character-system.md](character-system.md)), triggers the death/game-over flow, not a TB-specific path.
- **Player toggles off (X):** exits TB → RT immediately.
- **Map transition:** forces TB exit (see §2 "Forced Exit").
- **Monster flees and despawns:** Fleeing(4) → Dead(5) transition per [monster-ai.md](monster-ai.md).

### On Combat End (XP / Gold / Return to Exploration)

There is no single "combat ended" callback because combat is not a discrete mode — it is
the RT/TB flag plus live aggro. Consequences are awarded per-kill:

- `FUN_00438ce2` (`Monster_OnDeath`, 322 bytes) awards XP
  `(level_bonus + monster_level + difficulty_bonus) * 100` to the party accumulator
  `DAT_00ae3060` (clamped to 4,000,000), increments kill counters, and triggers loot
  generation. This runs in both RT and TB.
- When no hostile actors remain and the player is in RT, exploration simply continues.
  When in TB, the player presses X to return to RT/exploration.

---

## 9. Key Globals

| Address      | Type   | Name (suggested)        | Description                                              |
|--------------|--------|-------------------------|----------------------------------------------------------|
| `0x00acd6b4` | i32  | `g_combatMode`          | **0 = RT, 1 = TB** (the master flag)                     |
| `0x004f86d8` | struct | `g_turnQueue`           | The TB turn-queue object (see §3)                        |
| `0x004f86dc` | i32  | `g_queuePhase`          | Queue phase: 0 idle, 1 acting, 2 awaiting-next, 3 sub    |
| `0x004f86f0` | i32  | `g_combatFlags`         | Bitfield; bit 1 toggled on round start/entry             |
| `0x00507a6c` | i32  | `g_activePlayerIdx1`    | Active player index + 1 (0 = monster's turn / none)      |
| `0x0050c810` | i32  | `g_activeMember`        | Active party member index                                |
| `0x0050c814` | i32  | `g_roundTickCounter`    | TB round tick counter (zeroed on entry/round)            |
| `0x0050c818` | i32  | `g_partyRoundSpeed`     | Party round speed (active-member stat * 8)               |
| `0x00576eac` | i32  | `g_turnChanged`         | Set to 1 when the active queue entry changes             |
| `0x0050ba54` | i32  | `g_frameDeltaTicks`     | Frame delta (added to AI/actor timers each frame)        |
| `0x0050ba7c` | i32  | `g_frameDeltaAlt`       | Frame delta (subtracted from TB action counters)         |
| `0x004e28d8` | i32  | `g_uiWindowMode`        | UI window mode (0x10 = main gameplay)                    |
| `0x004d8438` | double | `k_tbRecoveryMul`       | `32/15 ≈ 2.1333` — TB recovery multiplier                |
| `0x004d8468` | double | `k_tbDisplayMul`        | `1.8671875` — player TB recovery *display* scaler        |
| `0x004f7c30` | i32[]| `g_visibleActors`       | Sorted visible-actor index list (source for queue)       |
| `0x004f7458` | i32  | `g_visibleActorCount`   | Count of visible actors (max 30)                         |
| `0x005ccd10` | u8[] | `g_monTypeRecoveryTbl`  | Per-monster-type TB recovery (stride 0x58)               |
| `0x00ae2f7c` | i32[]| `g_playerActionFlag`    | Per-player action flag (cleared on queue advance)        |
| `0x00acd804` | struct | `g_playerArray`         | Player array base (stride 0x1b3c)                        |
| `0x005feffc` | struct | `g_actorArray`          | Actor array base (stride 0x344)                          |
| `0x006650a8` | i32  | `g_actorCount`          | Total actor count                                        |

### Per-actor TB-relevant offsets (within 0x344-byte record)

| Offset | Field (suggested)       | Use in TB                                              |
|--------|-------------------------|--------------------------------------------------------|
| `+0x24` | `flagsExtended`         | Bit 7 = "in TB combat"; bit 15 = hostile               |
| `+0x60` | `monsterTypeId`         | Indexes `0x5ccd10` recovery table                      |
| `+0x8a` | `aiBaseField`           | Reset from monster-type table each round               |
| `+0xa0` | `recoveryThreshold`     | Target for the `+0xb8` accumulator                     |
| `+0xb0` | `tbActionState`         | Current TB action (0/7/9 etc., gates `RecomputeInit`)  |
| `+0xb8` | `actionAccumulator`     | Accumulates frame delta; compared to `+0xa0`           |

### Per-player TB-relevant offsets (within 0x1b3c-byte record)

| Offset   | Field (suggested)     | Use in TB                                          |
|----------|-----------------------|----------------------------------------------------|
| `+0x1a`  | `roundSpeedStat`      | Word; `*8` → party round speed `[0x50c818]`        |
| `+0x1934`| `tbRecoveryTimer`     | Filled by `EnterTurnBased` (init * 2.1333)         |
| `+0xacf138` (abs) | `tbRecoveryDisplay` | Display value (init * 1.8671875)             |

---

## 10. Function Map

### Toggle & Queue Lifecycle

| Address      | Size | Suggested name            | Description                                       |
|--------------|------|---------------------------|---------------------------------------------------|
| `0x0042fc2a` | --   | `Input_Dispatcher`        | Main input switch; case 6 = toggle TB, case 7 = pass |
| `0x00405cff` | 277  | `TurnQueue_EnterTB`       | Enter TB: clear marks, recompute recovery, play `turnstart` |
| `0x004059db` | 804  | `TurnQueue_StartNextRound`| Rebuild queue (players + visible hostiles), play `turnstop` |
| `0x0040471c` | 26   | `TurnQueue_AdvanceIfInTurn` | If TB and queue phase==2, call Advance          |
| `0x00406457` | 211  | `TurnQueue_Advance`       | Recompute next entry's init, re-sort, countdown   |
| `0x00404544` | ~460 | `TurnQueue_Sort`          | Insertion sort by init (actors before players on tie) |

### TB AI Loop

| Address      | Size | Suggested name            | Description                                       |
|--------------|------|---------------------------|---------------------------------------------------|
| `0x00401a91` | 2956 | `AI_UpdateAll`            | Dispatch: TB → `0x405e14`, else RT per-actor loop |
| `0x00405e14` | 573  | `AI_TurnBasedUpdate`      | TB AI: per-actor timers + queue-head dispatch     |
| `0x00406051` | 377  | `TB_SelectTarget`         | TB target selection                               |
| `0x0040652a` | 134  | `TB_RecomputeInit`        | Recompute initiative for entries                  |
| `0x004065b0` | 152  | `TB_SelectAction`         | Pick best action for the turn                     |
| `0x004061ca` | 471  | `TB_ProcessActorAction`   | Execute an actor's chosen action                  |
| `0x00406648` | 455  | `TB_Attack`               | TB attack (melee/spell dispatch by action byte)   |
| `0x0040680f` | 596  | `TB_Move`                 | TB movement (reposition)                          |
| `0x00406fa8` | 327  | `TB_RangedAttack`         | TB ranged attack                                  |
| `0x00406a63` | 155  | `TB_Approach`             | Close in on target                                |
| `0x00406b9f` | 369  | `TB_Wait`                 | Wait/idle pass                                    |
| `0x00406afe` | 161  | `TB_CanAct`               | Check if any actor can still act                  |
| `0x00406d10` | 664  | `TB_ResolveAction`        | Complex action resolution                         |

### Player Attack / Combat Entry

| Address      | Size | Suggested name            | Description                                       |
|--------------|------|---------------------------|---------------------------------------------------|
| `0x0042ebca` | 199  | `Player_AttackDispatch`   | Player attacks; RT/TB recovery branch; advances queue |
| `0x0042f4b6` | 275  | `CheckForCombatEncounter` | Proximity aggro check (outdoor 0x1400 / indoor 0xA00) |
| `0x0048e19b` | 853  | `Player_CalculateSpellDamage` | Reused as generic player-action-speed calculator (min 30) |
| `0x0040104c` | 469  | `AI_CheckHostility`       | Hostility between two actors                      |

### Shared Damage Pipeline (mode-independent; see [combat-system.md](combat-system.md))

| Address      | Suggested name                |
|--------------|-------------------------------|
| `0x00439463` | `DamageMonsterFromParty`      |
| `0x00439fee` | `DamagePlayerFromMonster`     |
| `0x0043b1d3` | `DamageMonsterFromMonster`    |

---

## Integration notes

### Mapping to the existing real-time CombatSystem

The existing `src/game/combat.{hpp,cpp}` already implements the RT AI state machine. To
add TB:

1. **A `TurnQueue` struct** mirroring `0x4f86d8`:

   ```cpp
   struct TurnEntry {
       ActorRef actor;        // type + index
       int     initiative;    // sort key (lower = earlier)
       int     counter;       // 0x40 at round start, decremented per frame
       uint8_t state;
       uint8_t slotTag;
   };
   struct TurnQueue {
       std::vector<TurnEntry> entries;
       QueuePhase phase = QueuePhase::Idle;   // mirrors 0x4f86dc
       int activePlayerIdx1 = 0;              // mirrors 0x507a6c
       int actionId = 100;
   };

```text

2. **A `CombatMode` enum** `{ RealTime, TurnBased }` on `CombatSystem` standing in for
   `g_combatMode`. The toggle handler flips it and calls `enterTurnBased()` /
   `startNextRound()`.

3. **Initiative model.** MM7's scheme is "round-based queue, re-sorted by remaining
   initiative after each action, with a countdown pass." For RuneHarbor a faithful
   approximation:
   - On `startNextRound()`: add alive players + visible hostile monsters; assign each an
     initial `initiative` from `playerActionSpeed()` (min 30) or `monsterType.tbRecovery`.
   - Sort ascending; tie-break: monster before player, then by index.
   - After each action: recompute the acting entry's initiative and re-sort. Run a
     countdown loop subtracting from entries until the next actor is ready.

4. **Per-monster-type TB recovery** should be a data field on the monster definition
   (mirroring `0x5ccd10`), not hardcoded.

5. **Stun duration** branches on mode: TB = 20 ticks, RT = float-scaled.

6. **No auto-enter from aggro.** Aggro only enables RT combat. TB is strictly manual
   (toggle key). This matches player expectations from the original.

7. **Movement in TB is allowed** — do not freeze the party. The party can reposition on
   its turn; monsters close distance on theirs.

### Constants to port

- TB recovery multiplier: `32.0/15.0` (≈2.1333).
- Minimum player initiative: `30`.
- Round-start counter: `64` (0x40).
- Action-id reset: `100` (0x64).
- Round-robin slot tag: `2`.
- Monster "hasted" initiative multiplier: `2` (note: this *increases* the initiative gap,
  delaying the monster — verify intent against playtesting before copying semantics).

---

## Open questions

Track these unresolved items in the [open-question register](open-questions.md).

1. **`0x3fde000000000000` = 1.8671875.** Used only for the player's *display* recovery
   value (`player+0xacf138`). Not the sort key. Its semantic meaning (why ~1.87) is
   unclear; it may be a leftover scaling constant. Do not treat it as load-bearing for
   scheduling.

2. **Haste doubles monster initiative.** In `TB_RecomputeInit` an active timer at
   `actor+0x144`/`+0x148` causes `init *= 2`, which in an ascending-sort system means the
   actor acts *later*. This reads counterintuitively for a "haste" effect — it may be that
   the timer actually marks a *slow/debuff*, or that the field semantics are inverted from
   the name. Flag for playtest verification.

3. **`g_queuePhase` (0x4f86dc) exact state machine.** Values 0/1/2/3 are observed;
   transitions are written indirectly (no direct `mov [0x4f86dc], imm` was found in the
   xrefs — it appears to be set through register spills). The 0/1/2 meanings are
   well-supported; value 3 is used by several input cases as a guard but its setter path
   was not fully traced.

4. **"UI mode 3" claim in [combat-system.md](combat-system.md) §2.** Not corroborated: the toggle path
   does not write `[0x4e28d8]`. The combat HUD signal is `g_combatMode` + `g_queuePhase`,
   not the UI window mode. Treat the earlier doc's claim as stale.

5. **No dedicated flee action.** Confirmed by the input switch (no "flee" case) and the
   absence of a flee-from-TB function. Disengagement is emergent (toggle to RT + move).
   If the original manual documents a flee key, it may be a no-op or context-gated in a
   path not exercised here.

6. **`g_partyRoundSpeed` (`0x50c818`).** Computed from the active member's `+0x1a` stat,
   but its consumers are limited to the round-display function `fcn.00441987` and the
   `EnterTB`/`StartNextRound` zeroing. It does not appear to feed back into initiative;
   its exact role in scheduling (if any) vs. pure display is not fully resolved.
