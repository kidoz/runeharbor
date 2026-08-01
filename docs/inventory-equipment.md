---
title: "Inventory & Equipment Interaction"
summary: "Inventory interactions coordinate backpack cells, equipped slots, cursor items, and usable items."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Inventory & Equipment Interaction

Inventory interactions coordinate backpack cells, equipped slots, cursor items, and usable items.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Extends [item-system.md](item-system.md) (item struct,
enchantments, slots) and [ui-windows.md](ui-windows.md) (inventory UI) with the
interaction mechanics needed to wire RuneHarbor's inventory widget.

---

## 1. Equipment slot layout & equipType→slot mapping

The equipType→slot mapping is a **lookup table at `0x4E8348`** (13 bytes),
indexed by the item's equipType byte (ItemDesc +0x20, absolute `0x5D2880`):

| equipType | name | maps to slot index | slot name |
|---|---|---|---|
| 0 | weapon1H | 1 | MainHand |
| 1 | weapon2H | 1 | MainHand (also blocks slot 0 / OffHand) |
| 2 | missile (bow) | 2 | Bow |
| 3 | armor | 3 | Armor |
| 4 | shield | 0 | **OffHand** |
| 5 | helm | 4 | Helmet |
| 6 | belt | 5 | Belt |
| 7 | cloak | 6 | Cloak |
| 8 | gauntlets | 7 | Gauntlets |
| 9 | boots | 8 | Boots |
| 10 | ring | 10 | first Ring (fills 10..15) |
| 11 | amulet | 9 | Amulet |
| 12 | wand | 1 | MainHand (treated as a weapon) |
| 13+ | reagent/potion/scroll/book/... | — | not equippable |

Note the binary's slot indexing: **slot 0 = OffHand, slot 1 = MainHand**. This
is inverted relative to RuneHarbor's `EquipSlot` enum (which has MainHand=0),
but that is fine as an internal abstraction as long as RuneHarbor's own
mapping table is applied consistently — RuneHarbor does not need to mirror the
binary's indices.

The 16-slot equipment array lives at `char + 0x1948` (16 × int32, 1-based
indices into the inventory item array; 0 = empty). Readers: `FUN_0048D690`
(slot occupied & not broken), `FUN_0048D612` (equipType of item in slot),
`FUN_0048D65C` (off-hand usable), `FUN_0048D6B6` (16-slot scan, bound 0x10).

---

## 2. Equip validation

The interactive equip path is `FUN_00468F8E` (2229 B), invoked from the render
frame. There is no separate validate function; the gates are inline.

### 2.1 Skill requirement (HARD FAIL) — `FUN_004926F8` (69 B)

Called once per equip branch (0x4690B5 / 0x4691FA / 0x46933B / 0x469566) with
the item's weaponSkill byte (ItemDesc +0x21, absolute `0x5D2881`):

```cpp
if skillId >= 37:           return 1   // misc/none item -> no skill required
if char.skills[skillId] != 0: return 1 // skill learned -> allowed
print("you lack the skill"); return 0  // REFUSED

```

Caller on return 0: plays the "can't do that" voice (FUN_004948A9 arg 0x27)
and **aborts the equip**. **Equipping a weapon/armor whose skill is unlearned
is refused outright** — no equip, no partial bonus.

So RuneHarbor's `canEquip` must check the character has the item's skill
learned: sword needs Sword, plate needs Plate, shield needs Shield, bow needs
Bow, etc. (Reagents/potions/scrolls have skillId >= 37 and are never
skill-gated — but they aren't equippable anyway.)

### 2.2 Alignment/race restriction — `FUN_00492C46` (205 B)

Called once before the skill gate, but only switches on a small set of
hardcoded special/quest item ids (~0x203..0x259 = ids 515..601). For those it
checks party alignment against thresholds. **General weapons/armor have no
alignment/race restriction.** RuneHarbor can model this as a per-item flag on
that small fixed set; default unrestricted.

### 2.3 Stat minimum / weight / encumbrance — **NONE**

There is no minimum-strength check and **no encumbrance system** (no per-item
weight field; `izz` grep for weight/encumb/burden returns nothing). Equipping
is gated solely by skill (+ the rare alignment gate on special items).
RuneHarbor's `totalWeight()`/`getItemWeight()` have no binary basis.

### 2.4 Dual-wield / two-handed

- **Off-hand weapon (dual wield)** requires Dagger (Expert+) or Sword
  (Master+): the weapon branch at 0x469316 reads the Dagger/Sword mastery and
  gates on it. Shields in the off-hand need only the Shield skill learned.
- **Two-handed weapons** map to MainHand and additionally clear/forbid slot 0
  (OffHand): `and [charbase+0x1948], 0` at 0x4694C3. If both hands are
  occupied the equip is refused with a "need a free hand" message.

---

## 3. The swap model (no auto-place)

There is **no automatic swap-to-backpack**. The previously-equipped item is
**swapped to the cursor** (global `0xAD458C`, the "held item"). The player
then clicks an empty backpack cell to drop it. If the inventory is full the
item stays on the cursor. Pattern: copy cursor item → temp; copy equipped
item → cursor; clear equipped; write temp → equipped; mark dirty
(`FUN_004936D9`).

For RuneHarbor this means a click-pickup / click-place UX is the faithful
model. A simpler "equip auto-swaps to backpack" approximation is also
acceptable and friendlier — RuneHarbor's existing `equip()` already does the
auto-swap-to-backpack form.

---

## 4. Inventory-screen interaction model

### 4.1 Click-pickup / click-place (NOT drag, NOT single-click-equip)

`FUN_00468F8E` is structured as:

- **Left-click on a backpack cell with empty cursor** (0x469696): maps the
  mouse cell via lookup table `0x505828`, validates, **lifts the item into the
  cursor global** (`0xAD458C`), clears the cell.
- **Left-click with cursor holding an item** (0x468FE7): routes to the equip
  switch (if clicking a slot) or places into the backpack cell.

So the interaction is two discrete clicks: pick up, then place/equip. No
continuous drag, no right-click-to-equip.

### 4.2 Right-click

Used for the item info/identify overlay (the tooltip builder `FUN_00426A03`),
**not** for equipping or using. Right-click does not consume the item.

### 4.3 Active character selection

The inventory view tracks the **party active member** at global `0x507A6C`
(0-3), written by the portrait-click handlers. It is **not** a separate
inventory-only selection. RuneHarbor's widget should bind to the same active
member used everywhere else.

---

## 5. Item use (consumables) — partial

Confirmed data tables:

- **Scroll → spell**: pointer table at `0x723BB8` (`scroll.txt`, loaded by
  `FUN_004764C6`), indexed by scroll itemId. Read by the spellbook-list
  renderer `FUN_00467FBA`.
- **Potion power**: stored in the item's numCharges field at generation
  (`FUN_0045664C`, a 3d4 roll); effect keyed by potion itemId.
- **Wand**: equipType 0xC, holds charges, otherwise a MainHand weapon.

The single consume dispatch (drink/read/cast) was **not definitively
isolated**; the equip handler only routes equipTypes 0-12 (equippable) and
falls through for potions/scrolls/books. Inferred behavior to confirm later:

- Potion: drink → apply spell effect scaled by numCharges → destroy.
- Spell scroll: read → cast inscribed spell → destroy.
- Message scroll: read → display text; not consumed.
- Book: read → learn spell if school known → destroy.
- Wand: equipped; use → cast inscribed spell, decrement numCharges, destroy
  at 0.

This is a follow-up; the first inventory-interaction pass should focus on
**equip/unequip** (the confirmed, high-value path) and treat usable items as a
separate phase.

---

## Integration notes

What exists and is correct:

- `Inventory::equip(c, backpackSlot)` — backpack → equipped with auto-swap.
- `Inventory::unequip(c, EquipSlot)` — equipped → first free backpack (fails
  if full).
- `getEquipType`/`getEquipSlot` map the `equipStat` string → EquipType →
  EquipSlot. Rings auto-fill the first empty of Ring1..Ring6.

What must be added/fixed:

1. **`canEquip` must check skills** (FUN_004926F8 rule): sword→Sword,
   plate→Plate, shield→Shield, bow→Bow, etc. No stat/weight check.
2. **Dispatch input to the widget**: `InventoryWidget::handleEvent` is never
   called from `ingame_state.cpp`. Wire a `MouseDown` dispatch when the widget
   is visible.
3. **Replace the swallow in `handleEvent`** with backpack-grid hit-testing
   (the render math gives gridStartX/Y, slotSize, stride, cols=7 →
   `backpackSlot = row*cols + col`) → call `equip(activeChar, backpackSlot)`.
4. **Paper-doll unequip hit-testing** has no per-slot coordinates today (all
   equipped items render at the body center). Define per-slot rects;
   `ItemEntry.equipX/equipY` are available but currently ignored.
5. **2H weapon handling**: clear/forbid OffHand. Dual-wield needs Dagger
   (Expert+) or Sword (Master+). Optional refinement for a first pass.
6. The "find free backpack slot" loop in `equip` (inventory.cpp) has a
   redundant nested condition — works today but fragile; simplify.

Item→SkillId mapping must be authored (no such table exists in the data);
`ItemEntry.skillGroup`/`equipStat` are the inputs but are ambiguous for weapon
sub-types (a 1H weapon could be Sword/Axe/Mace). A reasonable default for a
first pass: gate by the broad armor/weapon category (Plate skill for Plate
armor, Shield skill for shields, etc.) and skip the per-weapon-subtype gate
until the mapping is RE'd.

### Function index additions ([function-index.md](data/function-index.md))

`0x468F8E` (InventoryEquipHandler, 2229B), `0x4926F8` (EquipSkillGate),
`0x492C46` (EquipAlignmentGate), `0x49273D` (EquipSlotWriter),
`0x467E83` (UnequipHelper), `0x4925DE` (FindFreeBackpackSlot),
`0x48D690` (SlotOccupiedNotBroken), `0x48D612` (SlotEquipType),
`0x48D65C` (OffHandUsable), `0x426A03` (ItemTooltipBuilder),
`0x491375` (GrantStartingEquipment).

### Global additions ([globals-reference.md](data/globals-reference.md))

`0x4E8348` (equipType→slot table), `0xAD458C` (cursor/held item id),
`0xAD45A4` (held-item flag), `0x507A6C` (active character index),
`0x505828` (mouse→backpack-cell lookup), `0x5D2880` (equipType byte base),
`0x5D2881` (weaponSkill byte base).
