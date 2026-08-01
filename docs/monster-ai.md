---
title: "Monster AI System"
summary: "Monster AI coordinates actor state, hostility, target selection, movement, and attacks."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Monster AI System

Monster AI coordinates actor state, hostility, target selection, movement, and attacks.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Table of Contents

1. [Overview](#1-overview)
2. [Monster / Actor Struct Layout](#2-monster-actor-struct-layout)
3. [AI State Machine](#3-ai-state-machine)
4. [Hostility System](#4-hostility-system)
5. [Visibility and Target Selection](#5-visibility-and-target-selection)
6. [Pathfinding and Movement](#6-pathfinding-and-movement)
7. [Attack Selection](#7-attack-selection)
8. [Spawning and Placement](#8-spawning-and-placement)
9. [Monster Types and Data Files](#9-monster-types-and-data-files)
10. [Special Abilities and Spell Casting](#10-special-abilities-and-spell-casting)
11. [Turn-Based AI](#11-turn-based-ai)
12. [Key Functions](#12-key-functions)
13. [Key Global Variables](#13-key-global-variables)

---

## 1. Overview

The Monster AI system in MM7 governs the behavior of all non-player actors in the game
world. Each actor is a state machine that transitions between behavioral states (idle,
wandering, pursuing, attacking, fleeing, dead, stunned, paralyzed) based on distance to
targets, hostility relationships, current HP, active conditions, and recovery timers.

The system has two distinct modes of operation:

- **Real-time mode** (`DAT_00acd6b4 != 1`): Monsters are processed individually each
  frame. The main AI update function iterates over all "active" (visible, nearby) actors
  and evaluates their behavior based on distance, hostility, attack readiness, and
  current state.

- **Turn-based mode** (`DAT_00acd6b4 == 1`): A separate function (`FUN_00405e14`)
  handles actor turns sequentially, using the same underlying attack and movement
  subroutines but with turn ordering and initiative.

The main AI update loop is `FUN_00401a91` (2,956 bytes), called from the gameplay loop
(`FUN_00463186`). It calls 30+ sub-functions covering visibility sorting, target
selection, hostility evaluation, attack execution, movement, and spell casting.

### High-Level Processing Flow

```text
FUN_00401a91 (AI_UpdateAll)
  |
  +-- Outdoor? FUN_004014e6 (visibility/sort, outdoor)
  |   Indoor?  FUN_004016fa (visibility/sort, indoor)
  |
  +-- Earthquake damage processing (if DAT_00ae2f74 > 0)
  |
  +-- Turn-based? FUN_00405e14 (turn-based AI)
  |   Real-time? Per-actor loop:
  |     +-- Skip dead/stunned/paralyzed/disabled actors
  |     +-- Condition expiry checks (22 conditions iterated)
  |     +-- Recovery timer advancement
  |     +-- FUN_00401221 (find nearest hostile target)
  |     +-- Evaluate hostility level via hostility matrix
  |     +-- FUN_00427002 (select attack type: 0=melee, 1=ranged, 2=spell1, 3=spell2)
  |     +-- Distance-based behavior decision:
  |         +-- Close range: FUN_00402968 (melee attack)
  |         +-- Medium range: FUN_0040281c (approach + melee) or FUN_00402686 (pursue)
  |         +-- Long range: FUN_00402ad7 (ranged attack) or FUN_004032b2 (wander)
  |         +-- Spell range: FUN_00404874 / FUN_00404ac7 (cast spell)
  |         +-- Flee: FUN_00403476 / FUN_0040368b (flee from target)
  |         +-- No target: FUN_00403eb6 (idle/fidget) or FUN_00403f58 (wander)

```

---

## 2. Monster / Actor Struct Layout

### Struct Size and Storage

Actors are stored in a contiguous array starting at `DAT_005feffc` (base pointer).
The total active actor count is stored in `DAT_006650a8`.

**There are two stride values used in the code:**

- **Byte stride:** `0x344` = 836 bytes per actor record
- **Word stride:** `0x1A2` = 418 (used when indexing 16-bit fields via short pointers)
- **DWord stride:** `0xD1` = 209 (used when indexing 32-bit fields via int pointers)

These are all equivalent: `836 bytes = 418 shorts = 209 ints`.

### Key Fields

Offsets are relative to the start of each actor's 836-byte record.

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0x00 | 4 | flags | Bitfield: bit 2 = active/rendered, bit 6 = visible to party, bit 15 = hostile-flagged, bit 19 = always-hostile, bit 24 = combat-engaged |
| +0x08 | 2 | spriteType | Monster type / sprite ID |
| +0x12 | 1 | canFly | Non-zero if monster can fly |
| +0x13 | 1 | aiType | AI personality: 0=Normal, 1=Wimp, 2=Aggressive, 4=AdditionalHostile, 5=Fleeing |
| +0x14 | 1 | moveSpeed | Movement type category |
| +0x15 | 1 | hostilityLevel | Current hostility: 0=friendly, 4=hostile (set dynamically) |
| +0x1D | 1 | hasSpell1 | Non-zero if monster has first spell ability |
| +0x1E | 1 | statusFlags | Bit 4 = invisible |
| +0x23 | 1 | spellType1 | First spell attack type ID |
| +0x24 | 4 | flagsExtended | Bit 19 = permanently hostile (0x80000) |
| +0x25 | 1 | spellType2 | Second spell attack type ID |
| +0x27 | 1 | spellType3 | Third spell attack type ID |
| +0x34 | 4 | groupId | Alliance group ID |
| +0x38 | 2 | monsterTypeId | Index into monster type table (monlist/dmonlist) |
| +0x3C | 2 | attack1Spell | First spell for spell attacks |
| +0x3E | 2 | attack2Spell | Second spell for spell attacks |
| +0x44 | 4 | maxHP | Maximum hit points |
| +0x54 | 4 | recoveryTimer | Counts down; actor cannot act while > 0 |
| +0x60 | 2 | allianceGroup | Alliance/faction number for hostility checking |
| +0x62 | 2 | height | Actor sprite height |
| +0x64 | 2 | moveSpeedValue | Numeric movement speed |
| +0x66 | 2 | positionX | World position X |
| +0x68 | 2 | positionY | World position Y |
| +0x6A | 2 | positionZ | World position Z |
| +0x6C | 2 | velocityX | Current velocity X (for knockback, movement) |
| +0x6E | 2 | velocityY | Current velocity Y |
| +0x70 | 2 | velocityZ | Current velocity Z |
| +0x72 | 2 | facingAngle | Actor yaw / facing direction |
| +0x74 | 2 | pitchAngle | Actor pitch angle |
| +0x78 | 2 | movementSpeed | Calculated movement speed per tick |
| +0x7A | 2 | sectorId | Indoor sector index (BLV maps) |
| +0x88 | 2 | aiState | Current AI state (see state machine below) |
| +0x8C | 2 | currentHP | Current hit points |
| +0x90 | 4 | aiTimer | Time spent in current AI state |
| +0xE4 | 4 | charmTimeLo | Charm/berserk condition time (low dword) |
| +0xE8 | 4 | charmTimeHi | Charm/berserk condition time (high dword) |
| +0xF0 | 4 | stoneTimeLo | Stoned condition time (low dword) |
| +0xF4 | 4 | stoneTimeHi | Stoned condition time (high dword) |
| +0x124 | 2 | eventTriggerId | Event to fire on death |
| +0x164 | 4 | deadTimeLo | Dead condition time (low dword) |
| +0x168 | 4 | deadTimeHi | Dead condition time (high dword) |
| +0x194 | 4 | paralyzeTimeLo | Paralysis condition time (low dword) |
| +0x198 | 4 | paralyzeTimeHi | Paralysis condition time (high dword) |
| +0x1FC | 4 | summonGroup1 | First summoned monster group count |
| +0x200 | 4 | summonGroup2 | Second summoned monster group count |
| +0x20C | 4 | summonGroup3 | Third summoned monster group count |
| +0x210 | 4 | summonGroup4 | Fourth summoned monster group count |
| +0x29C | 4 | targetId | Current target actor/player ID (packed: bits 0-2 = type, bits 3+ = index) |
| +0x2C4 | 4 | teamId | Team membership ID (same team = allies) |
| +0x2C8 | 4 | hostilityOverride | Overridden hostility group; 9999 = force-friendly |
| +0x308 | 4 | aggroTarget | Current aggro target reference |

### Target ID Encoding

Target IDs use a packed format:

- **Bits 0-2:** Target type: `3` = another actor, `4` = party/player
- **Bits 3+:** Target index within the type's array

Example: `(actorIndex << 3) | 3` = targeting actor at `actorIndex`.
Value of `4` = targeting the party.

---

## 3. AI State Machine

### State Values

The AI state is stored at offset +0x88 in the actor struct (a 16-bit value).

| Value | Name | Description |
|-------|------|-------------|
| 0 | **Standing** | Default idle state; actor stands in place, not engaged |
| 1 | **Wandering** | Actor moves randomly within its area |
| 2 | **Guarding** | Patrolling or guarding a position |
| 3 | **Fidgeting** | Playing idle animation variation |
| 4 | **Fleeing** | Running away from target |
| 5 | **Dead** | Actor is dead; excluded from AI processing |
| 6 | **Pursuing** | Moving toward target to close distance |
| 7 | **Attacking** | In melee attack animation |
| 8 | **AttackingRanged** | Firing ranged/spell attack |
| 9 | **AttackingMelee2** | Alternate melee attack |
| 11 (0x0B) | **Stunned** | Temporarily incapacitated; recovery timer counts down |
| 12 (0x0C) | **CastingSpell1** | Casting first spell ability |
| 13 (0x0D) | **CastingSpell2** | Casting second spell ability |
| 17 (0x11) | **Paralyzed** | Held by paralysis effect; transitions to Standing (0) when expired |
| 18 (0x12) | **CastingSpell3** | Casting third spell ability |
| 19 (0x13) | **Stoned** | Petrified; fully immobile |

### State Transitions

```text
                  +--------+
                  |Standing|<-----------+
                  |  (0)   |----+       |
                  +--------+    |       |
                    |    |      |       |
           target   |    | no   |       | recovery
           found    |    | target|       | expires
                    v    v      |       |
              +---------+  +---------+  |
              |Pursuing |  |Wandering|  |
              |  (6)    |  |  (1)    |  |
              +---------+  +---------+  |
                   |                    |
           in range|                    |
                   v                    |
              +-----------+             |
          +---|Attacking  |---+         |
          |   |(7,8,9,    |   |         |
          |   |12,13,18)  |   |         |
          |   +-----------+   |         |
          |                   |         |
    HP low|           hit     |         |
          v                   v         |
     +---------+        +---------+     |
     |Fleeing  |        |Stunned  |-----+
     |  (4)    |        | (0x0B)  |
     +---------+        +---------+
          |
     timer expires
     (state -> 5 Dead)
          |
          v
     +---------+        +---------+     +---------+
     |  Dead   |        |Paralyzed|---->|Standing |
     |  (5)    |        | (0x11)  |     |  (0)    |
     +---------+        +---------+     +---------+
                        +---------+
                        | Stoned  |
                        | (0x13)  |
                        +---------+

```

### State Processing Rules

The main AI loop (`FUN_00401a91`) skips actors in these states: Dead (5), Stunned (0x0B),
Paralyzed (0x11), Stoned (0x13). It also skips actors whose `active/rendered` flag (bit 2
of byte at flags+1) is not set.

**Fleeing special case:** When an actor in state Fleeing (4) has its recovery timer
expire, it transitions to Dead (5). This represents a monster that ran away and
despawned. For Paralyzed (0x11), expiry transitions back to Standing (0).

**Recovery timer:** Stored at offset +0x54. Each frame, `DAT_0050ba54` (frame delta
ticks) is added to the AI timer at +0x90 and subtracted from the recovery timer. When
recovery > 0, the actor cannot initiate new actions. When the recovery timer from the
max-timer field at offset -8 relative to AI state is reached, the state times out.

---

## 4. Hostility System

The hostility system determines whether two actors (or an actor and the party) are
hostile to each other. This is handled by `FUN_0040104c` (469 bytes), which takes two
actor pointers and returns a hostility value.

### Return Values

| Value | Meaning |
|-------|---------|
| 0 | Allied / friendly -- will not attack each other |
| 4 | Hostile -- will attempt to attack each other |

(Intermediate values 1, 2, 3 may be returned from the hostility matrix for graded
responses, but the AI typically treats any non-zero value as hostile.)

### Alliance Groups

Each actor has an **alliance group** at offset +0x60 (a 16-bit integer). The raw value
is converted to a group index via:

```text
groupIndex = (allianceGroup == 0) ? 0 : (allianceGroup - 1) / 3 + 1

```

This means monster types come in groups of 3 (A/B/C variants share the same alliance
group). Group 0 is reserved for "no group" / party-allied.

### Hostility Matrix

A global matrix at `DAT_005c8b40` stores hostility relationships between all alliance
groups. The matrix dimensions are 89 columns (`0x59`) per row.

Lookup: `hostility = DAT_005c8b40[groupA * 0x59 + groupB]`

This matrix is loaded from `hostile.txt` (`FUN_00454810`). Non-zero entries mean the
groups are hostile to each other.

If either group index exceeds 88 (`0x58`), the function returns 0 (friendly), preventing
out-of-bounds access.

### Team IDs

Actors can also be assigned a **team ID** at offset +0x2C4. If two actors both have
non-zero team IDs and those IDs match, they are always considered allied (returns 0)
regardless of the hostility matrix.

### Hostility Override

An actor's **hostility override** at offset +0x2C8 can force a specific hostility group:

- If `hostilityOverride > 0`: Use this value instead of the natural alliance group
- If `hostilityOverride == 9999`: Force-friendly (group becomes 0, meaning ally)

### Dead/Condition Checks

Before evaluating hostility, the function checks condition timestamps:

- If the actor's dead condition time at +0x168/+0x164 is active (positive or non-zero
  low word), the function returns 4 (hostile to everything -- corpse cleanup).
- If the actor's paralysis condition at +0x198/+0x194 is active, the hostility group is
  forced to 0 (the paralyzed actor is treated as non-threatening).

### Permanently Hostile Flag

If bit 19 (`0x80000`) of `flagsExtended` at +0x24 is set and the opposing actor's group
is 0 (party-allied), the actor is always hostile (returns 4).

### Charm/Berserk Override

If the actor has an active charm condition at +0xE8/+0xE4, and the opposing actor's
group is 0, the charmed actor is considered friendly (returns 0).

### Algorithm Summary (FUN_0040104c)

```cpp
function checkHostility(actorA, actorB):
    // 1. Dead actors are hostile to all
    if actorA.deadTime > 0: return HOSTILE
    if actorB.deadTime > 0: return HOSTILE

    // 2. Extract alliance groups
    groupA = actorA.allianceGroup (converted to index)
    groupB = actorB.allianceGroup (converted to index)

    // 3. Same team = allied
    if actorA.teamId != 0 and actorA.teamId == actorB.teamId: return ALLIED

    // 4. Apply hostility overrides
    if actorA.hostilityOverride > 0: groupA = override
    if actorA.hostilityOverride == 9999: groupA = 0
    if actorA.paralyzeTime > 0: groupA = 0
    (same for actorB)

    // 5. Charm check
    if actorA.charmTime > 0 and groupB == 0: return ALLIED
    if actorB.charmTime > 0 and groupA == 0: return ALLIED

    // 6. Permanently hostile flag
    if actorA has 0x80000 flag and groupB == 0: return HOSTILE
    if actorB has 0x80000 flag and groupA == 0: return HOSTILE

    // 7. Bounds check
    if groupA > 88 or groupB > 88: return ALLIED

    // 8. Matrix lookup
    return hostilityMatrix[groupA * 89 + groupB]

```

---

## 5. Visibility and Target Selection

### Visibility Sorting

Before AI processing, the engine determines which actors are "active" (close enough
to process). Two separate functions handle this for the two map types:

- **Outdoor:** `FUN_004014e6` (532 bytes) -- max visibility range `0x15FF` (5,631 units)
- **Indoor:** `FUN_004016fa` (919 bytes) -- max visibility range `0x27FF` (10,239 units)

Both functions iterate all actors (`DAT_006650a8` count) and compute 3D distance to the
party using a fast approximation:

```text
dx = abs(partyX - actorX)
dy = abs(partyY - actorY)
dz = abs(partyZ - actorZ)
sort(dx, dy, dz) => max, mid, min
distance = max + (min / 4) + (mid * 11 / 32) - actorRadius

```

This avoids expensive square-root computation. The result is a rough octahedral distance
metric.

Actors beyond the maximum visibility range are flagged as inactive (bit 6 of flags byte
cleared). Actors within range are flagged as potentially active (bit 6 set, bit 24 set
if hostile).

The functions produce a sorted list of up to **30** (`0x1E`) nearest active actors,
stored in `DAT_004f7c30` (actor indices) and `DAT_004f7460` (distances). The count is
stored in `DAT_004f7458`.

**Indoor-specific:** `FUN_004016fa` additionally checks sector membership. Actors in the
same BSP sector as the party (checked via `FUN_0049aba0`) are included even if they
did not pass the initial distance culling, and a line-of-sight check (`FUN_004070ef`)
is used.

### Target Selection (FUN_00401221)

`FUN_00401221` (709 bytes) finds the closest hostile target for a given actor. It
iterates all actors and evaluates:

1. Skip dead (5), fleeing (4), stunned (0x0B), paralyzed (0x11), stoned (0x13)
2. Skip self
3. Check if the potential target is the actor's current aggro target (+0x308) and
   validate via `FUN_004089c7`
4. Call `FUN_0040104c` to check hostility between the two actors
5. If hostile, compute 3D distance using the approximation formula
6. Apply aggression range based on hostility level:
   - Hostility 1: 0x400 (1,024 units)
   - Hostility 2: 0xA00 (2,560 units)
   - Hostility 3: 0x1400 (5,120 units -- full combat range)
   - Hostility 4: `DAT_004df390` (maximum range)
7. Check line of sight via `FUN_004070ef`
8. Track the closest hostile actor found

After checking all actors, the function also evaluates whether the **party** is a valid
target (if param_3 is non-zero). It calls `FUN_0040104c` with the party as the second
parameter and compares the party distance against the best actor distance found.

The result is written to `param_2` as a packed target ID:

- Actor target: `(actorIndex << 3) | 3`
- Party target: `4`
- No target found: `0`

### Aggression Range by Hostility Level (DAT_004df380)

| Hostility Level | Range (units) | Hex |
|-----------------|---------------|-----|
| 0 | 0 | 0x0000 |
| 1 | 1,024 | 0x0400 |
| 2 | 2,560 | 0x0A00 |
| 3 | 5,120 | 0x1400 |
| 4 | 10,240 | 0x2800 |

---

## 6. Pathfinding and Movement

### Pathfinding movement functions

| Function | Size | Purpose |
|----------|------|---------|
| `FUN_00402686` | 406 | Pursue target at long range (walking toward) |
| `FUN_0040281c` | 332 | Approach target at close range (melee close-in) |
| `FUN_00403f58` | 216 | Wander randomly (no target) |
| `FUN_004032b2` | 452 | Wander toward last known target area |
| `FUN_00403eb6` | 162 | Idle/fidget animation (stand in place) |

### Movement Approach

MM7 does not use grid-based or waypoint-based pathfinding. Instead, movement is
direction-based:

1. **Direction computation:** `FUN_004040e9` (1,115 bytes) computes the direction
   and distance from the actor to its target. It produces a 28-byte result structure
   containing the direction vector, distance, and angle information.

2. **Speed calculation:** The actor's movement speed (`+0x78`) is computed from its
   speed stat, potentially modified by flying height if `canFly` is set and the map
   is outdoor.

3. **Facing update:** The actor's facing angle (`+0x72`) is updated toward the target
   direction. A random offset of +/- 0x100 may be added for slight variation.

4. **Position update:** The velocity fields at +0x6C/+0x6E/+0x70 are set based on
   speed and direction. Actual position integration is handled by the physics system
   (`FUN_00407a1c`, 3,277 bytes) which performs collision detection against map geometry.

### Flying Movement

When `canFly` is non-zero (+0x12) and the map is outdoor (`DAT_006be1e0 == 2`), the
actor's target height is adjusted: `targetZ = actorHeight + 0x200`. The actor will
attempt to maintain altitude above the ground.

### Wander Behavior

When an actor has no target (`hostilityLevel == 0` or no hostile in range):

- `FUN_00403f58` selects a random direction based on `_rand()` and sets the actor
  to walk in that direction for a random duration.
- `FUN_00403eb6` puts the actor in a fidget/idle animation (state 3).
- There is a 2% chance per frame (`_rand() % 100 < 2`) of playing a vocalization
  sound via `FUN_00402ced`.

### Flee Behavior

Flee functions (`FUN_00403476`, `FUN_0040368b`) compute the direction **away** from
the threat using `FUN_00407a1c` for pathfinding with collision avoidance. The actor
sets state to Fleeing (4) and runs at increased speed.

- `FUN_00403476`: Flee from melee target (no ranged attack available)
- `FUN_0040368b`: Flee from ranged attacker (has ranged but is retreating)

---

## 7. Attack Selection

### Attack Type Determination (FUN_00427002)

`FUN_00427002` (183 bytes) selects which attack type a monster should use. It calls
`FUN_004270b9` (499 bytes) which evaluates the monster's available attacks and returns:

| Return Value | Attack Type | Description |
|--------------|-------------|-------------|
| 0 | Melee (basic) | Physical close-range attack |
| 1 | Ranged (basic) | Physical ranged projectile |
| 2 | Spell Attack 1 | First special/spell ability |
| 3 | Spell Attack 2 | Second special/spell ability |

The selection considers:

- Whether the monster has the attack type defined (spell bytes at +0x23, +0x25, +0x27)
- A random roll weighted by the `usePercent` for each spell attack
- Whether the monster is in range for the attack type
- Whether the monster's recovery timer allows the attack

### Distance Thresholds for Attack Decisions

The main AI loop uses several distance constants to decide behavior:

| Distance | Hex | Behavior Triggered |
|----------|-----|--------------------|
| Close range | < `DAT_004d8430` * scale | Attempt melee attack |
| Melee range | < 0x400 (1,024) | Close enough for melee swing |
| Mid range | < 0x1400 (5,120) | Ranged attack or pursue |
| Long range | >= 0x2800 (10,240) | Wander toward or disengage |

The distance is also scaled by a multiplier based on target type:

- **Targeting another actor** (bits 0-2 == 3): scale factor = `0x3F000000` (0.5 as
  IEEE 754 float)
- **Targeting party** (bits 0-2 == 4): scale factor = `0x3F800000` (1.0 as
  IEEE 754 float)

This means actors are more aggressive toward each other (engage at half the normal
distance threshold).

### AI Personality Influence

The `aiType` byte at offset +0x13 influences flee/attack decisions:

| Value | Personality | Behavior |
|-------|-------------|----------|
| 0 | Normal | Standard behavior; balanced attack and retreat |
| 1 | Wimp | More likely to flee when HP is low; higher flee threshold |
| 2 | Aggressive | Prefers to close distance; less likely to use ranged; lower flee threshold |
| 3 | Suicidal (unused?) | Never flees |
| 4 | Additional Hostile | Has special hostility rules |
| 5 | Fleeing | Special flee-on-spawn type |

The flee decision at offset +0x14 involves comparing current HP to thresholds:

- **Personality 2 (Aggressive):** Flee when `currentHP < maxHP * DAT_004d8440` (roughly 15%)
- **Personality 3:** Flee when `currentHP < maxHP * DAT_004d8444` (roughly 25%)
- These are float-point multiplier constants that define the HP fraction threshold.

If the flee condition is met **and** the actor is beyond melee range (distance >= 0x2800),
the actor enters flee mode instead of pursuing.

### Melee Attack Execution

`FUN_00402968` (367 bytes) handles melee attacks:

1. Calls `FUN_00438bce` (Monster_CheckAlive) to verify target is still alive
2. Computes direction via `FUN_004040e9`
3. If within melee range, sets AI state to Attacking (7, 8, or 9)
4. Calls `FUN_004597a6` to update the sprite frame for the attack animation
5. 2% chance of vocalization via `FUN_00402ced`

### Ranged Attack Execution

`FUN_00402ad7` (471 bytes) handles ranged attacks:

1. Calls `FUN_00438bce` to verify target
2. Computes direction and distance
3. Calls `FUN_0045284a` to evaluate line-of-sight
4. If clear, calls `FUN_00402cae` (63 bytes) which spawns the projectile via
   `FUN_0042f5c9` (Combat_SpawnProjectile)
5. Sets AI state to AttackingRanged (8)

### Spell Attack Execution

Two spell casting functions exist:

- `FUN_00404874` (543 bytes): Casts "simple" spells (first spell slot)
- `FUN_00404ac7` (3,860 bytes): Casts complex spells with full effect resolution

Spell casting involves:

1. Computing projectile trajectory via `FUN_0049aba0` (sector lookup) and
   `FUN_0042f5c9` (projectile spawning)
2. Checking spell prerequisites (stat checks via `FUN_0048ca25`, `FUN_0048c9a8`,
   `FUN_0048cc19`)
3. Applying spell effects via `FUN_00458519` (condition application)
4. Setting AI state to CastingSpell (12, 13, or 18)

---

## 8. Spawning and Placement

### Map-Based Spawning

Monsters are placed in maps during map loading. The map file (BLV or ODM) contains
spawn point records that define:

- Monster type ID (indexes into the monster table)
- Position (X, Y, Z)
- Facing angle
- Group/alliance ID
- Initial AI state

### Event-Based Spawning

The event system can spawn monsters via opcodes:

- **Opcode 0x0B (`EVT_SPAWN_MONSTER`)**: Spawns a monster/NPC at a specified location
- **Opcode 0x36 (`EVT_REPLACE_MONSTER`)**: Replaces one monster type with another
  (old type at +5, new type at +9)
- **Opcode 0x31 (`EVT_SET_MONSTER_HOSTILE`)**: Sets hostility for a specific monster
- **Opcode 0x37 (`EVT_SET_MONSTER_AI`)**: Sets AI behavior for a monster
- **Opcode 0x2F (`EVT_SET_MONSTER_HP`)**: Sets monster HP directly
- **Opcode 0x30 (`EVT_SET_MONSTER_FIELD`)**: Sets an arbitrary field on a monster
- **Opcode 0x32 (`EVT_SET_MONSTER_GROUP`)**: Sets monster group/AI state
- **Opcode 0x3C (`EVT_SET_MONSTER_HOSTILE_BY_INDEX`)**: Sets hostility by monster index

### Random Monster Generation

The function at `FUN_0044f5a8` handles random monster generation using `MapStats.txt`
and `Monsters.txt`. If a monster type string from the map data cannot be resolved, the
game displays: `"Can't create random monster: '%s'! See MapStats.txt and Monsters.txt!"`

A second function at `FUN_0045504a` also generates random monsters and displays:
`"Can't create random monster: '%s' See MapStats!"` on failure.

### Monster Array Management

- Actors are stored contiguously starting at `DAT_005feffc`
- Maximum actor count: limited by the active array (30 processed per frame, though
  more may exist on the map)
- Actor count stored at `DAT_006650a8`
- Actor data is saved/loaded with the map state (written in `FUN_0045f4a2`)

### Death Processing

When an actor's HP reaches 0 or below:

1. `FUN_00402d6e` (409 bytes) -- Monster death handler:
   - Sets AI state to Dead (5)
   - Calls `FUN_0042f7c7` to spawn death effects
   - Calls `FUN_004585be` to clear active conditions
   - Calls `FUN_00404736` to spawn loot/projectile drops
   - 2% vocalization chance via `FUN_00402ced`
   - Updates animation state via `FUN_004597a6`
2. `FUN_00438ce2` (322 bytes) -- Post-death processing:
   - Calls `FUN_00438b8a` for group notification
   - Calls `FUN_00449ba1` / `FUN_00449b7a` to update the game world
   - Calls `FUN_0047752f` to update NPC/quest state if relevant
3. If the actor has an event trigger ID at +0x124, event `FUN_0042694b` is called

---

## 9. Monster Types and Data Files

### Data File Loading

Monster type definitions are loaded from two sources:

| File | Format | Purpose | Loader Function |
|------|--------|---------|-----------------|
| `data\monlist.txt` | Tab-separated text | Human-readable monster definitions | `FUN_00465245` |
| `data\dmonlist.bin` | Binary | Compiled/cached monster race list | `FUN_00459935` |

The text parser at `FUN_00465245` reads `monlist.txt` and produces the binary
`dmonlist.bin` cache. The binary loader at `FUN_00459935` reads the cached version
for faster loading.

On save, `FUN_00459899` writes `dmonlist.bin` (error: `"Unable to save dmonlist.bin!"`).

### monsters.txt

The `monsters.txt` file (`FUN_0045504a`, loaded from `data\monsters.txt` at address
`0x004e8a3c`) defines the full monster stat block used by the `MonstersParser`. Fields
include:

- Name, picture sprite, level, HP, AC, XP reward
- Movement type: Free, Short, Med, Long, Stand
- AI type: Normal, Wimp, Aggress, Suicidal
- Two physical attacks with damage dice and hit percentage
- Two spell attacks with use percentage and spell/mastery/skill
- Ten resistance values (Fire, Air, Water, Earth, Mind, Spirit, Body, Light, Dark, Physical)
- Special abilities string (shot multiplier, explode, summon)
- Treasure, quest flag, fly capability, speed, recovery, haste

### Monster Type Grouping

Monsters are organized in groups of 3 (A/B/C tiers). The alliance group index formula
`(id - 1) / 3 + 1` means:

- IDs 1-3 share alliance group 1
- IDs 4-6 share alliance group 2
- And so on.

This grouping is also used in the hostility matrix where A/B/C variants of the same
creature share alliances and enemies.

### Monster Type Table (DAT_005fefd0)

The global at `DAT_005fefd0` points to the loaded monster type table. Each entry is
`0x98` (152) bytes. When an actor's conditions expire and it needs to reset its sprite,
the code reads:

```text
actor.height = monsterTypeTable[actor.monsterTypeId * 0x98 - 0x98]

```

Similarly, the base hostility level is read from:

```text
actor.hostilityLevel = DAT_005cccd1[actor.monsterTypeId * 0x58]

```

This second table at `DAT_005cccd1` (stride `0x58` = 88 bytes per entry) contains
per-type default behavior values.

### Debug Flag

The INI flag `nomonster` (at `FUN_00466086`) disables all monster processing when set.

---

## 10. Special Abilities and Spell Casting

### Spell Attack Slots

Each actor can have up to three spell abilities, stored at offsets +0x23, +0x25, and
+0x27 in the actor struct. The spell ID at offsets +0x3C and +0x3E specify the actual
spell to cast.

### Spell Casting Flow

1. The main AI selects spell attack via `FUN_00427002` (returns 2 or 3 for spell)
2. For spell type 1: `FUN_00404874` is called (simple spell)
3. For spell type 2 or 3: `FUN_00404ac7` is called (complex spell, 3,860 bytes)

The complex spell function (`FUN_00404ac7`) is the largest single monster AI function.
It handles:

- Stat-based spell effects (Intellect via `FUN_0048c9a8`, Personality via `FUN_0048ca25`,
  Luck via `FUN_0048cc19`)
- Condition application via `FUN_00458519` (timed effects like poison, disease, etc.)
- Projectile spawning for direct damage spells via `FUN_0042f5c9`
- Area-of-effect calculations
- Visual effect spawning via `FUN_004a7e19` (particle effects)
- Condition clearing via `FUN_004585be`

### Monster-Specific Special Attacks

The `special` field in `monsters.txt` can specify:

- **shot,xN**: Fires N projectiles (e.g., `shot,x3` = triple shot)
- **explode,NdM,element**: Explodes on death dealing NdM damage of specified element
  (e.g., `explode,5D8,light`)
- **Summon,element,MonsterName**: Summons another monster (e.g.,
  `Summon,air,Dragon C`)

### Bonus Effects on Hit

The `bonus` field specifies conditions applied on successful attack:

- Disease1, Disease2, Disease3: Disease conditions of varying severity
- BrkArmor: Break armor effect
- Afraid: Fear condition
- Insane: Insanity condition
- DrainSP: Drain spell points
- Uncon: Unconscious
- Agex2: Aging (double)
- Cursex2: Curse (double)
- Dead: Instant death attempt
- Errad: Eradication attempt

### Unknown Monster Spell Warning

If a monster references a spell not in the spell table, the game logs:
`"Unknown monster spell %s"` (at `FUN_0045490e`).

---

## 11. Turn-Based AI

### Turn-Based Mode

When `DAT_00acd6b4 == 1` (combat mode active), the AI switches to turn-based processing
via `FUN_00405e14` (573 bytes).

This function manages the turn order and delegates to sub-functions:

| Function | Size | Purpose |
|----------|------|---------|
| `FUN_004061ca` | 471 | Process actor's turn action (attack or move) |
| `FUN_00406051` | 377 | Evaluate turn-based target selection |
| `FUN_004065b0` | 152 | Determine best action for current turn |
| `FUN_00406648` | 455 | Execute attack during turn |
| `FUN_0040680f` | 596 | Execute movement during turn |
| `FUN_0040652a` | 134 | Turn initiative calculation |
| `FUN_00406afe` | 161 | Check if actor can act this turn |
| `FUN_00406b9f` | 369 | Process idle/wait during turn |
| `FUN_00406fa8` | 327 | Process ranged attack during turn |
| `FUN_00406a63` | 155 | Process approach during turn |
| `FUN_00406d10` | 664 | Complex turn-based action resolution |

### Turn Processing

Each actor gets one action per turn. `FUN_00405e14` iterates the active actor list and:

1. Checks if the actor can act (`FUN_00406afe`, `FUN_0040894b`)
2. Processes condition timers via `FUN_00458603`
3. Selects an action via `FUN_004065b0` and `FUN_00406051`
4. Executes the action (attack via `FUN_00406648`, move via `FUN_0040680f`, etc.)
5. Calls `FUN_004597a6` to update animation state

### Combat Encounter Detection

`FUN_0042f4b6` determines when to transition between real-time and turn-based modes:

- **Outdoor threshold:** `0x1400` (5,120 units) -- hostile actor within this range
  triggers combat
- **Indoor threshold:** `0x0A00` (2,560 units)
- Dead (5), fleeing (4), stunned (0x0B), paralyzed (0x13), special (0x11) actors are
  excluded from triggering combat

---

## 12. Key Functions

### Core AI Functions

| Address | Suggested Name | Size | Description |
|---------|----------------|------|-------------|
| `FUN_00401a91` | `AI_UpdateAll` | 2,956 | Main AI update loop (real-time mode) |
| `FUN_0040104c` | `AI_CheckHostility` | 469 | Hostility between two actors |
| `FUN_00401221` | `AI_FindNearestTarget` | 709 | Find closest hostile for an actor |
| `FUN_004014e6` | `AI_VisibilitySortOutdoor` | 532 | Outdoor visibility sorting |
| `FUN_004016fa` | `AI_VisibilitySortIndoor` | 919 | Indoor visibility sorting |
| `FUN_00405e14` | `AI_TurnBasedUpdate` | 573 | Turn-based AI processing |

### Movement function index

| Address | Suggested Name | Size | Description |
|---------|----------------|------|-------------|
| `FUN_00402686` | `AI_Pursue` | 406 | Chase target at long range |
| `FUN_0040281c` | `AI_ApproachMelee` | 332 | Close in for melee |
| `FUN_00403f58` | `AI_WanderRandom` | 216 | Random wandering (no target) |
| `FUN_004032b2` | `AI_WanderToward` | 452 | Wander toward area |
| `FUN_00403eb6` | `AI_StandIdle` | 162 | Idle/fidget in place |
| `FUN_00403476` | `AI_FleeFromMelee` | 533 | Flee from melee threat |
| `FUN_0040368b` | `AI_FleeFromRanged` | 457 | Flee from ranged threat |
| `FUN_00403c6c` | `AI_FleeGeneric` | 501 | Generic flee behavior |
| `FUN_004040e9` | `AI_ComputeDirection` | 1,115 | Direction/distance to target |
| `FUN_00407a1c` | `AI_PathfindMove` | 3,277 | Collision-aware movement |

### Attack Functions

| Address | Suggested Name | Size | Description |
|---------|----------------|------|-------------|
| `FUN_00402968` | `AI_MeleeAttack` | 367 | Execute melee attack |
| `FUN_00402ad7` | `AI_RangedAttack` | 471 | Execute ranged attack |
| `FUN_00404874` | `AI_CastSpellSimple` | 543 | Cast basic spell |
| `FUN_00404ac7` | `AI_CastSpellComplex` | 3,860 | Cast complex spell with full effects |
| `FUN_00427002` | `AI_SelectAttackType` | 183 | Choose attack type (melee/ranged/spell) |
| `FUN_004270b9` | `AI_EvaluateAttacks` | 499 | Evaluate available attacks |
| `FUN_00402cae` | `AI_SpawnProjectile` | 63 | Create attack projectile |

### State and Utility Functions

| Address | Suggested Name | Size | Description |
|---------|----------------|------|-------------|
| `FUN_00402d6e` | `Actor_Die` | 409 | Handle actor death |
| `FUN_004030ad` | `Actor_PlayHitReaction` | 276 | Play hit animation |
| `FUN_00402ced` | `Actor_Vocalize` | 129 | Play random vocalization sound |
| `FUN_004597a6` | `Actor_UpdateAnimation` | 167 | Update sprite frame state |
| `FUN_00438bce` | `Actor_CheckAlive` | 244 | Check if actor is still alive |
| `FUN_00438ce2` | `Actor_OnDeath` | 322 | Post-death processing (loot, XP) |
| `FUN_0043ac68` | `Actor_UpdateAfterHit` | 239 | Update AI state after being hit |
| `FUN_0040894b` | `Actor_IsActive` | 124 | Check if actor should be processed |
| `FUN_004089c7` | `Actor_ValidateTarget` | - | Validate current target still valid |
| `FUN_00458603` | `Actor_ProcessConditions` | 54 | Process timed conditions |

### Monster Data Loading

| Address | Suggested Name | Size | Description |
|---------|----------------|------|-------------|
| `FUN_00465245` | `MonsterList_LoadTxt` | - | Parse `monlist.txt` |
| `FUN_00459935` | `MonsterList_LoadBin` | - | Load `dmonlist.bin` |
| `FUN_00459899` | `MonsterList_SaveBin` | - | Save `dmonlist.bin` |
| `FUN_00454810` | `Hostility_LoadMatrix` | - | Parse `hostile.txt` |
| `FUN_0045504a` | `Monster_SpawnRandom` | - | Generate random monster from tables |
| `FUN_0044f5a8` | `Monster_CreateRandom` | - | Create random monster instance |
| `FUN_0045490e` | `Monster_ParseSpells` | - | Parse monster spell definitions |

---

## 13. Key Global Variables

### Actor Array

| Address | Type | Description |
|---------|------|-------------|
| `DAT_005feffc` | u32[] | Actor array base (stride 0x344 = 836 bytes) |
| `DAT_005ff000` | i16[] | Actor HP array (stride 0x1A2 shorts, offset +0x04 from base) |
| `DAT_005ff088` | i16[] | Actor AI state array (stride 0x1A2, offset +0x8C from base) |
| `DAT_006650a8` | i32 | Total active actor count |
| `DAT_005fefd0` | ptr | Monster type table (0x98 bytes per entry) |

### Visibility System

| Address | Type | Description |
|---------|------|-------------|
| `DAT_004f7458` | i32 | Count of visible/active actors (max 30) |
| `DAT_004f7c30` | i32[] | Sorted list of visible actor indices |
| `DAT_004f7460` | i32[] | Distances for sorted visible actors |
| `DAT_004f64b8` | i32[] | Secondary actor list (indoor LOS-verified) |
| `DAT_004f5ce8` | i32[] | Secondary distance list |
| `DAT_004f6c88` | i32[] | Per-actor target ID cache |

### Hostility

| Address | Type | Description |
|---------|------|-------------|
| `DAT_005c8b40` | u8[] | Hostility matrix (89 columns per row) |
| `DAT_005cccd1` | u8[] | Per-monster-type default hostility (stride 0x58) |

### Distance Constants

| Address | Type | Description |
|---------|------|-------------|
| `DAT_004df380` | int32[5] | Aggression range by hostility level (0, 0x400, 0xA00, 0x1400, 0x2800) |
| `DAT_004df390` | i32 | Maximum aggression range |
| `DAT_004d8430` | double | Base melee engagement range |
| `DAT_004d8440` | float | HP flee threshold multiplier (aggressive personality) |
| `DAT_004d8444` | float | HP flee threshold multiplier (normal personality) |

### Combat/Game State

| Address | Type | Description |
|---------|------|-------------|
| `DAT_00acd6b4` | i32 | Combat mode flag: 0 = real-time, 1 = turn-based |
| `DAT_0050ba54` | i32 | Frame delta ticks (time elapsed this frame) |
| `DAT_00ae2f74` | i32 | Earthquake timer (damages all actors when active) |
| `DAT_00ae2f78` | i32 | Earthquake damage base |
| `DAT_004f86f4` | i32 | Post-earthquake cooldown counter |
| `DAT_00acd4ec` | i32 | Party X position (for distance calculations) |
| `DAT_00acd4f0` | i32 | Party Y position |
| `DAT_00acd4f4` | i32 | Party Z position |
| `DAT_00acd4f8` | i32 | Party yaw facing direction |
| `DAT_00acd4fc` | i32 | Party pitch |

---

## Notes

- All addresses reference the original `MM7-Rel.exe` (v1.21) virtual address space.
- Function names prefixed with `FUN_` are Ghidra auto-generated identifiers.
- `DAT_` prefixed names are Ghidra auto-generated global variable references.
- The actor struct size of 836 bytes (0x344) contains many more fields than documented
  here; only fields observed in the AI-related functions are listed.
- The 3D distance approximation formula used throughout is a well-known octahedral
  distance estimate that avoids sqrt at the cost of ~4% maximum error.
- Monster type IDs use 1-based indexing (ID 0 is invalid/unused).
