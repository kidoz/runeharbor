---
title: "Temple, Healing, and Resurrection Services"
summary: "Temple services price and apply healing, condition removal, and resurrection actions."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Temple, Healing, and Resurrection Services

Temple services price and apply healing, condition removal, and resurrection actions.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Extends [shops-and-economy.md](shops-and-economy.md) (which covers
the building registry, type codes, and generic item pricing) with the
temple-specific service actions, cost formulas, and the exact game-state
mutations for Heal / Cure / Resurrect.

Related prior docs: [character-system.md](character-system.md) (struct layout, condition enum),
[spell-system.md](spell-system.md), [party-management.md](party-management.md).

---

## 1. How the shop screen reaches the temple code

The shop screen renderer `FUN_004B30BA` (doc 29 §4.2) contains an inner
dispatch at `0x4B3335` that branches on the **active building's type code**
`int16[buildingIdx*0x34 + 0x5912B8]` (field +0x00, doc 29 §2.1):

```cpp
0x004B3335  cmp eax, 0x15   ; Tavern  (21) -> 0x4B81E8
0x004B333A  cmp eax, 0x16   ; Bank     (22) -> 0x4B7CE1
0x004B333F  cmp eax, 0x17   ; TEMPLE   (23) -> 0x4B6FC1   <-- this doc
0x004B3344  cmp eax, 0x1A..0x1C  ; Stables/Boats -> 0x4B68A6
0x004B3352  cmp eax, 0x1E       ; -> 0x4B4673
0x004B3357  cmp eax, 0x1F       ; -> 0x4B4EB2

```

So **`FUN_004B6FC1` (2227 B) is the temple service/cost function**, called once
per frame while the temple window is open. It both *renders* the service menu
and *executes* the chosen service. The type-gated cost helpers `0x4B63DB` /
`0x4B68A6` documented in doc 29 §3.6 are the *generic non-temple* service pricers
(stables/boats/inn) — **they are not on the temple path** (confirmed: their only
caller is `0x4B30BA` at the non-temple branch).

The action dispatcher `FUN_0044686D` (doc 29 §4.4, 60-case switch) does NOT
contain temple-specific logic. Its opcode 16 case (`0x4477B0`) branches again on
building type 0x15/0x16/**0x17**/0x18 and calls the generic per-character
attribute-mutator `FUN_0044B01E`; that path is used by *event-scripted* attribute
changes (e.g. a quest that damages HP), not by the interactive temple UI. The
interactive temple UI is entirely inside `0x4B6FC1`.

---

## 2. Temple service action set

`FUN_004B6FC1` reads a UI sub-mode from global `[0xF8B01C]` and dispatches at
`0x4B7040`–`0x4B7062` (value minus 1, then compare). Four branches:

| `[0xF8B01C]` value | Branch addr | Action |
|--------------------|-------------|--------|
| 1                  | `0x4B76B1`  | **Render service menu** — list each party member with their per-character cost (format string `"%s %d %s"` @ `0x4F0E3C`). |
| 10                 | `0x4B74E9`  | **Resurrect / Raise** the selected dead/stoned/eradicated character. |
| 11                 | `0x4B7324`  | **Donate** gold (reputation/luck adjustment). |
| 86 (0x56)          | `0x4B7068`  | **Heal / Cure** the whole party (the "Heal All" button). |

There is **no separate "Heal one" vs "Cure one" button**. MM7 temples expose a
single Heal/Cure action that processes **every party member** in one pass
(the loop at `0x4B70E4`–`0x4B713E`, iterating `party[first..last]`). Resurrection
is the only per-character action. This matches the in-game UI: one "Heal" button,
one "Resurrect" button, one "Donate" button.

The mode value in `[0xF8B01C]` is set by the shop button-click handler before
this function runs; r2's `aaa` found no direct writer xrefs (it is written
through a register-held pointer in the UI input code), so the exact button→mode
mapping is inferred from the branch values above.

---

## 3. The "needs service?" gate — `FUN_004B6F5C` (101 B)

Called at `0x4B74EB` (resurrect branch) and `0x4B76E2` (menu render) to decide
whether a character has anything to heal/raise. Returns 1 = needs service,
0 = healthy.

```text
if building-type in {0x4E, 0x50..0x52}:        // special buildings (shrines?)
    cond = GetWorstCondition(char)              // 0x48E9EC
    if cond == 0x11 or cond == 0x12: goto check_hp
else:
    cond = GetWorstCondition(char)
    if cond == 0x12 (Zombie): goto check_hp
check_hp:
    if char.HP  < GetMaxHP(char):  return 1     // 0x48E4F0
    if char.SP  < GetMaxSP(char):  return 1     // 0x48E55D
    return 0

```

So a character "needs service" if HP or SP is below max, or carries an active
condition other than the benign ones (Drunk etc.). The 0x11/0x12 special-case
exists because condition 0x11 = a "raised as zombie" marker and 0x12 is an
intermediate state used by the raise logic (see §6).

---

## 4. Temple cost formula — `FUN_004B7FDF` (134 B) + `FUN_004B7FA3` (60 B)

This is the **per-character service cost**. Called at `0x4B7039` with the
temple's `Val` float (building +0x20, doc 29 §2.1) as the argument. It is
**tiered by condition severity, not flat, and not scaled by level**.

### 4.1 The severity multiplier — `FUN_004B7FDF`

```text
worst = GetWorstCondition(char)          // 0x48E9EC, returns 0..18 (18=none)
if worst == 14 (Dead) or 15 (Stoned):
    severity = 5
elif worst == 16 (Eradicated):
    severity = 10
else:                                     // alive but sick, or fully healthy
    maxv = 0
    for condIdx in 0..13:                 // loop over the 14 non-fatal conditions
        v = PerConditionCost(char, condIdx)   // 0x4B7FA3, see 4.2
        if v > maxv: maxv = v
    severity = (maxv == 0) ? 1 : maxv

rawCost = severity * multiplier_float * templeVal
cost    = max(1, round_to_int(rawCost))   // 0x4CA74C = ftol with rounding

```

Wait — re-reading `0x4B7FDF`: the float multiply chain is
`fild [severity] × fimul [multiplier] × fmul [templeVal]`. The "multiplier" is a
*second* scale: for Dead/Stoned the inner `var_8h` is set to 5, for Eradicated
to 10, **and for the alive/sick branch `var_8h` is set to 1** (at `0x4B801D`).
So the structure is:

```text
cost = base_severity * tier_mult * templeVal     // all integer-ish, done in fp

```

where:

| Worst condition | `base_severity` (from 4.2) | `tier_mult` |
|-----------------|----------------------------|-------------|
| none / healthy  | 1 (forced, see below)      | 1           |
| Cursed..Paralyzed (0..13) | max over active conditions | 1 |
| Dead (14) / Stoned (15)   | 5                          | 5 |
| Eradicated (16)           | 10                         | 10 |

For the alive branch, if `maxv == 0` (no active non-fatal condition), the code
forces `var_4h = 1` at `0x4B8042` so a healthy character still costs
`1 * 1 * templeVal`. (In practice the gate in §3 prevents calling this on a
healthy character, but the floor exists.)

### 4.2 Per-condition severity — `FUN_004B7FA3` (60 B)

```text
arg: condIdx (0..13)
ts = char.conditionTimestamps[condIdx]     // qword at char_base + condIdx*8
v  = round( ts * 0.000625 )                // fmul [0x4D84B4] = 0x3E700000 = 0.000625
v  = clamp(v, 0, 60) twice (via 0x4CBA20)  // saturate to [0,60]
v  = ((v / 24) * 7 + v) ... actually: v = ((v % 24) % 7) + 1   // see note
return v

```

The tail arithmetic at `0x4B7FCB`–`0x4B7FDB` is: `v / 24` then `% 7` then `+1`.
Concretely the condition timestamp (game ticks since the condition was
contracted) is bucketed: longer-affliction → higher cost within a 1..7 band.
Constants: **0.000625** (ticks→unit scale), **24** (bucket width), **7**
(buckets), **+1** (min), **60** (saturation ceiling).

This is **per-condition**, so the "Cure" price reflects whichever active
condition is most expensive to treat (the `max` in §4.1). There is no
"pay once, clear all" discount; clearing two poisons costs the max-single
price (not the sum), but the fee is driven by the worst one.

### 4.3 The full Heal/Cure charge

After computing the per-character cost, the Heal branch (`0x4B7149`+) formats
the gold amount and, on confirmation, calls `FUN_00492BAE` (§5) for each
character. Gold is debited through the standard party-gold primitive; the
amount shown is the `cost` from §4.1 summed appropriately per the UI. The
merchant-skill discount (`FUN_004911EB`, doc 29 §3.2) is **applied** — seen at
`0x4B70A4` (`call 0x4911EB`) feeding `cost = cost * (100 - discPct) / 100`,
then floored to a multiple of 3 (`0x4B70BA` `push 3; idiv`) with a min of
`cost/3`. So the final displayed price is:

```text
final = max( floor(cost * (100-discPct) / 100), floor(cost/3) )

```

identical finalizer shape to the item buy-price path (doc 29 §3.3).

### 4.4 The Donate cost — fixed, not tiered

The donate branch (`0x4B7324`) reads only the temple `Val` (`fld [eax+0x5912D8]`
→ `0x4CA74C`) and compares party gold `[0xACD56C]` against it (`0x4B733C`;
`jb 0x4B75BC` = not enough → error). Donate is a **flat fee = `int(templeVal)`**
(no per-condition, no discount). On success it increments a donations counter
and adjusts a reputation/luck tracker (see §7).

---

## 5. Heal / Cure mutations — `FUN_00492BAE` (85 B)

Called for living characters. Despite the name this is the **"heal to full +
clear conditions"** primitive, not a partial heal:

```asm
if ecx (cost/amount) >= [0xACD56C] (party gold):
    [0xACD56C] = 0                       // can't go below zero gold
else:
    [0xACD56C] -= ecx                    // debit gold
// then: queue a UI "healed" event (message id 200 @ 0x4AA29B)

```

Note: `0x492BAE` debits gold and posts the heal animation/message; the **actual
HP/SP/condition writes** for the temple path happen in the resurrection/raise
block (§6) and, for the living Heal path, via the per-character state refresh
that follows. The character's HP is restored to max by the sequence at
`0x4B7557`–`0x4B7569` (calls `0x48E4F0` GetMaxHP → store `[esi+0x193C]`, calls
`0x48E55D` GetMaxSP → store `[esi+0x1940]`). So:

- **Heal restores HP and SP to maximum** (full heal, not partial).
- **Cure clears all active conditions**: the condition timestamp array (qword
  array at `char_base + condIdx*8`, 18 entries — see `0x4B7FA3` addressing) is
  zeroed. In the resurrection path this is an explicit `memset(char_base, 0,
  0xA0)` (160 bytes = 20 qwords, covering all 18 condition slots + adjacent
  HP/SP). For the living Heal path the same zeroing is performed condition by
  condition.

Confirmed condition-array layout: `char.conditionTimestamps[i]` lives at
`char_base + i*8` (i = 0..17), exactly matching RuneHarbor's
`ConditionIndex` enum (`src/game/character.hpp`). The "worst condition" getter
`FUN_0048E9EC` iterates a **priority-ordered index table at `0x4EDDA0`** (18
dwords) rather than scanning 0..17 directly:

```text
0x4EDDA0:  10 0F 0E 11 0D 02 0C 0B 0A 09 08 07 06 05 04 03 01 00
           = Eradicated, Stoned, Dead, Zombie, Unconscious, Asleep,
             Paralyzed, Disease3, Poison3, Disease2, Poison2, Disease1,
             Poison3, Insane, Drunk, Afraid, Weak, Cursed   (using the enum)

```

So "worst" priority is Eradicated > Stoned > Dead > Zombie > Unconscious >
Asleep > Paralyzed > (diseases/poisons descending) > Insane > Drunk > Afraid >
Weak > Cursed.

---

## 6. Resurrection — branch `0x4B74E9`

```asm
0x4B74EB  call 0x4B6F5C            ; gate: does this char need raising?
0x4B74F0  test eax,eax; je done    ; skip if healthy
0x4B750D  ... save char's qword fields at +0x70..+0x8C to stack
0x4B7543  push 0xA0; push 0; push esi(char); call memset   ; ZERO 160 bytes
0x4B7557  call 0x48E4F0 (GetMaxHP); mov [esi+0x193C], eax  ; HP = MAX
0x4B7564  call 0x48E55D (GetMaxSP); mov [esi+0x1940], eax  ; SP = MAX
0x4B756F  ... if building is type 0x4E or 0x50..0x52 (special shrines):
             different handling (sets [esi+0x88] conditions-of-raise)
0x4B75CF  ... else normal raise:
0x4B75FB  movsx eax,[esi+0xBA] (current race/sex byte)
0x4B7602  [esi+0x1928] = eax          ; preserve
0x4B7610  push 1; push 0x11; call 0x492D5D   ; SetCondition(Zombie=0x11) side-FX
0x4B761F  call 0x490139                ; IsDiseaseCondition?
0x4B7626  neg/sbb -> eax = (had_disease ? -1 : 0)
0x4B762E  add eax, 0x17 (23)
0x4B7631  [esi+0x1920] = eax
0x4B7637  call 0x490139
0x4B7641  setne al; add al, 0x17
0x4B7643  [esi+0xBA] = al             ; recompute race/condition byte
0x4B7654  call 0x491DDF                ; party-member state resync
0x4B7659  [esi+0x88] = [0xACCE64]; [esi+0x8C] = [0xACCE68]  ; timestamp stamps

```

Conclusions for resurrection:

- **HP and SP are set to FULL** (max), not HP=1. Confirmed by the two
  `GetMaxHP`/`GetMaxSP` stores to `+0x193C`/`+0x1940`.
- **The Dead/Stoned/Eradicated condition timestamps are cleared** (the
  `memset` zeroes the entire condition array).
- **No "too eradicated to raise" threshold.** Eradicated (condition 16) is
  just more expensive (severity 10, §4.1); it is raisable. There is no check
  that refuses resurrection based on how long the character has been dead.
- **Gold cost** for resurrection uses the same `FUN_004B7FDF` with the
  Dead/Stoned (×5) or Eradicated (×10) tier, times `templeVal`, then the
  merchant-discount finalizer (§4.3). A raise is simply "the most expensive
  heal".
- After raising, the character is tagged through `SetCondition(0x11)` (the
  "recently raised" marker, which the gate in §3 treats specially) and a
  party-state resync (`0x491DDF`) runs.

### `FUN_004911EB`-family helpers referenced

| Addr | Size | Role |
|------|------|------|
| `0x48E9EC` | 39 B | `GetWorstCondition(char)` — scans priority table `0x4EDDA0`. |
| `0x48E4F0` | 109 B | `GetMaxHP(char)` — base + class + endurance + race bonus + buffs. |
| `0x48E55D` | 194 B | `GetMaxSP(char)` — class/race-gated (some classes get 0). |
| `0x490139` | 34 B | `IsNegativeCondition(cond)` — returns 1 for disease/poison/death-family. |
| `0x492D5D` | 1138 B | `SetCondition(char, condIdx, withSideFX)` — 18-case switch, plays sound + message. |
| `0x492BAE` | 85 B | `Heal_DebitGoldAndFx(char, amount)` — gold debit + heal animation. |
| `0x491DDF` | — | party-member derived-stats resync after a raise. |
| `0x4948A9` | — | `PlayCharEffect(char, effectId)` — common effect/sound trigger. |

---

## 7. Donate path (`0x4B7324`)

Flat fee = `int(templeVal)`. On success:

- `0x4B734A`: `call 0x492BAE` (heal/debit primitive reused to debit the gold).
- `0x4B734F`–`0x4B7469`: adjusts a **reputation/alignment counter** stored at
  `[edi+8]` where `edi` = `0x6A1138` (light path) or `0x6BE50C` (dark path),
  selected by `[0x6BE1E0] == 2` (the party's light/dark alignment). The counter
  is decremented toward `-5` (`0xFFFFFFFB`) — i.e. donating moves the
  reputation tracker in the temple's favoured direction, capped at -5.
- `0x4B737A`–`0x4B7469`: a 5-step cascade compares the counter against
  thresholds `-5, -10, -15, -20, -25` (`0xFB,0xF6,0xF1,0xEC,0xE7`) and, for
  each tier crossed, calls `FUN_00427734` (a stat-buff applier) with a
  per-tier effect id (`0x32`=50, `0x4B`=75, `0x56`=86, `0x55`=85, `0x30`=48)
  and a computed magnitude `= (donationCount % 7) | 0x80`. So repeated donations
  grant escalating temporary stat buffs (luck/personality), one tier per
  threshold.
- `0x4B746F`: `inc byte [ecx + 0xF8B06F]` — increments a per-temple donation
  counter (drives the `% 7` magnitude above).

This is the MM7 "donate for reputation and temporary buffs" mechanic. It is
**temple-only** (no other building type reaches this branch).

---

## 8. Non-temple healing paths (spell / rest) — addresses only

Healing and condition-setting are **not temple-exclusive**; they share the same
character-state primitives. Key call sites (from `axt` of `0x492BAE` heal and
`0x492D5D` set-condition):

**Heal (`0x492BAE`) callers:**

- `0x44BA6A`, `0x44BC02` — generic party-stat dispatcher `0x44B9F0` (case 22/24:
  "restore HP" / "restore SP" event opcodes).
- `0x4B204F`, `0x4B218D`, `0x4B22F9` — `0x4B1F64` (inn/rest heal).
- `0x4B4B2E` — `0x4B4673` (building type 0x1E service).
- `0x4B5E0E` — `0x4B5CDF`.
- `0x4B64E2` — `0x4B63DB` (stables/boats heal-on-rest).
- `0x4B6997` — `0x4B68A6`.
- `0x4B734A`, `0x4B7508` — **temple** (this doc, donate + resurrect).
- `0x4B79BF` — `0x4B7874`.
- `0x4B7F07` — `0x4B7CE1` (bank).
- `0x4B85EF`, `0x4B8686` — `0x4B81E8` (tavern/inn rest).
- `0x4BC68C`, `0x4BD4AC` — `0x4BC3FE`, `0x4BCA2F` (combat victory / monster).
- `0x4BDC07`,`0x4BDD25`,`0x4BDF26`,`0x4BE1DA` — `0x4BDAB9` (likely the
  **Rest/Wait** time-advance healer, called per hour rested).

**SetCondition (`0x492D5D`) callers (spell system):**

- `0x48DC04`, `0x48DCDC` — **spell-effect dispatchers** (22-case switch on spell
  id). `0x48DCDC` is the main "spell hits a target" function; its cases push a
  condition index (0x0E=Dead for death spells, 0x0D=Unconscious, etc.) then
  call `0x492D5D`. This is how poison/sleep/paralyze/death **spells** apply
  conditions — same primitive the temple clears.
- `0x49402D` — another spell-effect variant (area/AoE).
- `0x416625`, `0x42BEB1`,`0x42C14F`,`0x42DD55` — trap/trigger and
  monster-attack condition application.
- `0x44AC98` (`0x44A5EE`) and `0x44B659` (`0x44B01E`) — event-scripted
  condition set/remove.
- `0x4B761A` — **temple** resurrection SetCondition(Zombie) side-effect.

**Rest / time-advance heal:** the per-hour healing during rest is in
`FUN_004BDAB9` (calls `0x492BAE` four times — once per resting phase). Inn rest
is `0x4B81E8` (tavern building service). These restore HP/SP proportionally to
hours rested; they do **not** clear conditions (conditions expire by game-time
expiry in the timestamp array, or via temple/spell).

Spell healing (e.g. Light/Greater Heal) routes through the spell-effect
dispatcher `0x48DCDC` → which calls the maxHP/currHP mutators directly; a full
deep-dive is out of scope here (see [spell-system.md](spell-system.md)).

---

## Integration notes

RuneHarbor's `game::Character` already models the 18-slot condition timestamp
array and HP/SP fields (`src/game/character.hpp`). To implement temples:

1. **Service set**: three actions — `Heal` (whole party), `Resurrect`
   (per-character), `Donate`. No per-condition "cure one".
2. **Cost** (`serviceCost` already exists; extend with the severity model):
   - Compute `worst = char.worstActiveCondition()` using the priority order in
     §5 (Eradicated > Stoned > Dead > Zombie > Unconscious > Asleep >
     Paralyzed > diseases/poisons > Insane > Drunk > Afraid > Weak > Cursed).
   - `severity = worst∈{Dead,Stoned}?5 : worst==Eradicated?10 : max(perConditionSeverity(c) for active c)`.
   - `cost = max(1, round(severity * tierMult * templeVal))` where `tierMult`
     is 5/10/1 per the table in §4.1.
   - Apply the merchant discount and round-to-3 finalizer already in
     `serviceCost()` (doc 29 §3).
3. **Heal effect**: set `HP = maxHP`, `SP = maxSP`, clear all condition
   timestamps for every party member that `needsService()` (HP<max or SP<max or
   any active condition).
4. **Resurrect effect**: same as Heal, plus requires the character to be
   Dead/Stoned/Eradicated; no eradication threshold.
5. **Donate**: flat `int(templeVal)` gold; adjust a light/dark reputation
   counter (cap -5) and grant escalating temporary buffs at thresholds
   -5/-10/-15/-20/-25.

### Globals referenced (add to [globals-reference.md](data/globals-reference.md))

| Addr | Meaning |
|------|---------|
| `0x507A40` | ptr to active building record (0x54-byte shop descriptor). |
| `0x507A3C` | ptr to active party-roster descriptor (fields +0x28 first idx, +0x38 last idx, +0x2C selected). |
| `0x507A6C` | active character/party index (count of party members). |
| `0xF8B01C` | temple UI sub-mode (1=menu, 10=resurrect, 11=donate, 86=heal). |
| `0xACD56C` | party gold (debited by heal/donate). |
| `0xACD550` | party "donation count" / reputation tick used by donate magnitude. |
| `0xACCE64/68` | global "current game time" hi/lo (stamped on raise at char+0x88/0x8C). |
| `0x6BE1E0` | party light/dark alignment (2 = dark selects `0x6BE50C`, else `0x6A1138`). |
| `0x4EDDA0` | condition-priority table (18 dwords) used by `GetWorstCondition`. |
| `0x4D84B4` | float `0.000625` — condition-timestamp→severity scale. |
| `0x4D85BC` | float `500.0` (`0x43FA0000`) — seen in temple base-cost multiply (doc 29). |

### Function index additions ([function-index.md](data/function-index.md))

`0x4B6FC1` (TempleService), `0x4B7FDF` (TempleCostPerChar), `0x4B7FA3`
(ConditionSeverity), `0x4B6F5C` (CharNeedsService), `0x4B30BA` (ShopScreen,
update: houses the type-code dispatch at 0x4B3335), `0x48E9EC`
(GetWorstCondition), `0x48E4F0` (GetMaxHP), `0x48E55D` (GetMaxSP), `0x490139`
(IsNegativeCondition), `0x492D5D` (SetCondition), `0x492BAE`
(HealDebitGoldAndFx), `0x48DCDC` (SpellEffectDispatcher).

---

## Open questions

Track these unresolved items in the [open-question register](open-questions.md).

- **High confidence** (byte-level verified): the type-code dispatch at
  `0x4B3335`; the four temple branches and their addresses; the resurrection
  memset+full-HP/SP writes; the heal-primitive gold-debit; the condition-array
  addressing (`char_base + idx*8`); the priority table at `0x4EDDA0`.
- **Medium confidence**: the exact arithmetic of `FUN_004B7FA3`'s tail
  (`/24`, `%7`, `+1`) — the operations are clear but the *intent* (bucketing
  affliction age into 7 severity bands) is an inference. Confirm against
  observed affliction durations if exact costs matter.
- **Lower confidence / unresolved**:
  - The `[0xF8B01C]` writer was not found by `aaa` xrefs (written via register);
    the value→action mapping is inferred from the compare constants. A Ghidra
    pass over the shop input handler would confirm.
  - The `0x4B756F` building-type 0x4E/0x50..0x52 special-shrine branch in
    resurrection (sets `[esi+0x88]` differently) — semantics not fully traced;
    these building ids are not in the standard `2dEvents.txt` temple set.
  - Whether the living-Heal path zeroes conditions via the same memset or
    per-slot: the resurrection path definitely memsets; the living path
    reaches the same `+0x193C/+0x1940` stores but the condition-zeroing is
    implied by the subsequent `SetCondition`/resync rather than directly
    observed as a memset in that branch.
