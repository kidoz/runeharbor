---
title: "Active-Member Selection & Spell Targeting"
summary: "Active-party-member selection feeds the HUD, inventory, and spell-targeting flows."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Active-Member Selection & Spell Targeting

Active-party-member selection feeds the HUD, inventory, and spell-targeting flows.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Builds on [spell-system.md](spell-system.md) (spell data),
[combat-system.md](combat-system.md), and [ui-windows.md](ui-windows.md) (HUD/spellbook windows).

---

## 1. Active-member selection

**Active member global: `0x507A6C`** — int32, **1-based** (1..4 = player 0..3;
0 = none). Corrects doc 31 §4.3 (which said 0-based and mis-identified the
writers).

### 1.1 Auto-picker — `FUN_00493707` (363 B)

Selects a valid active member when the current one becomes invalid. Per
candidate (0..3) it skips any with a non-zero active-condition timer (the six
paired-dword timers at +0x10/+0x14, +0x60/+0x64, ... +0x80/+0x84 — Dead,
Unconscious, Asleep, Paralyzed, Petrified, etc.) or a non-zero combat recovery
timer (`word [player + 0x2300]`). First pass picks the first valid candidate;
second pass breaks ties by highest `word [+0x1866]` (initiative/speed).
Returns the candidate index **+1**.

### 1.2 Cycle key — `FUN_00493872` (87 B)

`KEY_CHARCYCLE` (keymap index 12). Increments the active member (wraps 4→1),
or decrements if SHIFT held (wraps 1→4). Skips members with a non-zero
recovery timer (`word [player + 0x1934]`); tries at most 4 candidates. **Does
NOT skip dead members** (only recovery) — relies on the subsequent auto-pick
to correct.

### 1.3 Auto-switch triggers

`FUN_0048E962` (recovery changed) and `FUN_0048E8ED` (recovery completed): if
the active member's recovery changes and `0x50BF44` (targeting lock) is 0,
re-run the auto-picker. So when the active member starts an action, the engine
auto-selects the next valid member. If the active member dies mid-combat, the
next picker run skips them.

### 1.4 Player click selection

- **Inventory portrait row** has a static coordinate table at `0x4E2910`
  (X-min: 20, 131, 242, 357) and `0x4E2918` (X-max: 83, 198, 312, 423); all 4
  share Y 375..466. Click handler `FUN_00416D0B` enqueues a commit to
  `0x507A6C`.
- **Main-view HUD portraits** are GUI buttons (per-button rects via
  `GUIButton::Create`); no static table. Committed in render-frame cases
  69 (0x45) / 143 (0x8F) at `0x433259`: `mov [0x507A6C], eax`, gated by the
  targeting-pending flag `0x50C820`.

**No number-key (1-4) selection.** Selection is via the cycle key, portrait
click, or auto-pick.

---

## 2. Spell casting entry

**Spellbook window type = 18**, opened by render-frame case 105 (action 0x69)
at `0x4347C8`. Pre-conditions: not in a restricted combat sub-state, no
pending queue, an active member exists, the active member's recovery is 0, and
UI mode not in {5, 7, 100..103}.

**KEY_CAST keymap index 10**, default binding 'C'. Enqueues action 105 via the
window/button event layer.

**Spellbook click handler — `FUN_00421E4F` (662 B):** the spellbook is a grid
of 32×32 px cells, origin (17,14), 14 rows: `cellIndex = ((cursorX-17)>>5)*14

+ ((cursorY-14)>>5)`. `FUN_00421E1E` reads the per-character spellbook grid at
`[player + cell*4 + 0x157C]` (spell ID, or 0 = not known). On a valid spell it
queues a cast (code 0x71) via the delayed-action slot and swaps the cursor to
MICON1 (targeting reticle).

### Quickbar (2 slots)

Per-character bytes `[player + 0x1A4E]` (slot 1) and `[player + 0x1A4F]`
(slot 2). Resolved via an 11-byte-stride table at `0x5063CC`. Cast paths:
slot 1 = render-frame case 86 (action 0x8E); slot 2 = keyboard case 7 (action
0x19) with full mana gating at `0x43010E`. **No number-key (1-9) hotkeys
beyond these 2 slots.**

---

## 3. Spell targeting model

### 3.1 Target-type classifier + targeting opener — `FUN_00427734` (1339 B)

Central cast-routing function (mislabeled "canCast" in doc 32). Args:
`ecx = spell_id`, `edx = char_index`. Classifies the spell's target type into
a bitmask and opens the targeting overlay if the spell needs a clicked target.
The target-type is a **per-spell hardcoded switch** (96 cases at `0x427C6F`),
NOT a data field in `spells.txt`.

| Bit | Spell IDs | Routing |
|-----|-----------|---------|
| `bl.7` (0x80) | 4, 30 | **Single party member** — overlay type-27, `0x50C820=1`, wait for portrait click. |
| `bh.0` (0x100) | 47, 89 | **Single monster** — overlay with button 0x8D; click a monster in 3D. |
| `ebx.1` (0x2) | 40, 50, 53, 56, 64, 71, 72, 73, 95 | **All-party / choice** — overlay with single-vs-all buttons (0x35/0x36). |
| `ebx.3` (0x8) | 2, 11, 20, 26, 32, 35, 39, 52, 59, 60, 66, 70, 78, 79, 87, 90, 93, 94 | **No target (instant/AoE)** — fires immediately. |
| (none) | 3, 7–10, 12–17, 19, 21–23, 25, 27, 33, 38, 43, 48, 54, 63, 75, 80, 83–86, 88 | **Self / no target** — caster only. |

Cross-checked against spell names (spell 47 = Spirit 3 Turn Undead → single
undead; spell 50/71/72/73 = party-wide heals/buffs; etc.) — consistent.

### 3.2 Targeting resolution

When `0x50C820 == 1` (targeting mode active) and the player clicks:

- **Single-party**: the portrait handler stashes the clicked member index into
  the queued action's target field.
- **Monster**: `FUN_004220E5` (3D-view click handler) resolves the cursor to a
  monster instance and stashes it.

Then the pending action (in the processing queue `0x50CA50`) is dequeued by
`FUN_00435737`, the targeting window is torn down (`0x50C820` cleared), and
the spell-effect dispatcher `FUN_00427DB8` runs with `{spell_id, caster,
target, skill}`.

### 3.3 The action queues (corrects doc 32)

- `0x50C868` — **incoming** queue: `[+0]`=count, entries `[+0x4 + N*12]` =
  `{code, p1, p2}`, cap 40.
- `0x50CA50` — **processing** queue: memcpy'd from `0x50C868` each frame,
  then drained by `FUN_00435737`. (Doc 32 had the direction reversed.)
- `0x50C850/4/8` — a single delayed-action slot `{code, p1, delay_frames}`
  that drains into `0x50CA50`.

---

## 4. Mana / skill gating on cast

Three gates:

1. **Spell known**: the spellbook grid cell at `[player + cell*4 + 0x157C]`
   must hold the spell ID (0 = not known).
2. **School mastery**: `FUN_00427734`'s switch calls `FUN_0045827D(skill)` and
   compares mastery (some spells require Master or GM).
3. **Mana (SP)**: at `0x43010E`, reads 4 parallel mana-cost arrays at
   `0x4E3C48..0x4E3C4E` (Novice/Expert/Master/GM, each `uint16[100]` indexed
   by spell_id), compares cost to `[player + 0x1940]` (current SP). If cost >
   SP, the cast is skipped.

When a skill-override is provided (EVT opcode 0x29, or scroll use), the mana
cost is bypassed (set to 0).

---

## Integration notes

Existing assets:

- `Party::activeMemberIndex()` / `setActiveMemberIndex(int)` (clamped -1..3).
- `SpellSystem::castDamageSpell(caster, spellId, MonsterInstance*)`,
  `castHealSpell(caster, spellId, targetCharIdx)`, `castBuffSpell(caster,
  spellId)`, `canCast`, `getManaCost`, `getAvailableSpells`. **Heal/buff casts
  are implemented but never called today.**
- `SpellInfo.target` (SpellTarget enum) — coarse heuristic today (Spirit/Body/
  Mind → SingleAlly, else SingleEnemy). The binary's authoritative per-spell
  target-type is the hardcoded switch in §3.1.
- HUD portrait rects: game-coord X `[124 + 72*i, 124 + 72*i + 59]`, Y
  `[373, 452]`. The HUD has no input path today.
- `pickMonsterUnderCursor()` + `combatSystem->getMonster(idx)` resolve a
  monster target.

What must be built:

1. **HUD portrait click → `setActiveMemberIndex`** — hit-test the 4 portrait
   rects; on click set the active member (skip dead/unconscious for
   faithfulness, or allow and let the next action re-pick).
2. **Spell-selection UI** — the SpellbookWidget is a stub; build a real spell
   list (from `getAvailableSpells`) the player clicks to choose a spell.
3. **Targeting flow** — when a spell is chosen, branch on `SpellInfo.target`:
   - `SingleAlly` → targeting mode; next portrait click = heal target →
     `castHealSpell`.
   - `SingleEnemy` → targeting mode; next monster click (or current cursor
     pick) → `castDamageSpell`.
   - `AllAllies`/`Self` → `castBuffSpell` immediately (no target click).
4. **Replace the RMB auto-cast** with the chosen-spell model (or keep RMB as
   a quick-damage shortcut). The `findActivePartyMember()` helper ignores
   `activeMemberIndex()` — switch casters to the selected active member.

### Function index additions ([function-index.md](data/function-index.md))

`0x493707` (PickActiveMember), `0x493872` (CycleActiveMember), `0x48E962`
(OnRecoveryChanged), `0x48E8ED` (OnRecoveryCompleted), `0x421E4F`
(SpellbookClickSpell), `0x421E1E` (SpellbookResolveCell), `0x427734`
(SpellClassifyAndRoute), `0x4698AA` (SetTargetingCursor), `0x416D0B`
(InventoryPortraitClick).

### Global additions ([globals-reference.md](data/globals-reference.md))

`0x507A6C` (active member, 1-based), `0x4E2910/0x4E2918` (inventory portrait
X tables), `0x50C868`/`0x50CA50` (incoming/processing action queues),
`0x50C820` (targeting-mode flag), `0x50BF44` (targeting lock), `0x4E3C48..E`
(mana-cost arrays), `player+0x157C` (spellbook grid), `player+0x1A4E/F`
(quickbar slots).

---

## Open questions

Track these unresolved items in the [open-question register](open-questions.md).

- **KEY_CAST → action 105 enqueue site** not pinned to one instruction (the
  keymap index 10 is fall-through in the movement dispatcher; handled by the
  window/button layer). The consumption (case 105) is firm.
- **Quickbar slot 2 keymap binding** (case 7) — not definitively mapped to a
  named KEY_ binding. Treat the 2 quickbar slots as bindable hotkeys.
- **`FUN_00427734`** was mislabeled "canCast"; it is the classifier + targeting
  opener. The actual canCast gates are distributed (spell-known, mastery,
  mana at separate sites).
- **Main-view HUD portrait positions** are not a static table (GUI buttons).
  RuneHarbor should use its own portrait rect constants (already present in
  `hud.cpp`).
