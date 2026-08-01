---
title: "Usable Items (Potions, Scrolls, Books, Wands)"
summary: "Usable-item dispatch turns inventory objects into spell, condition, skill, or charge effects."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Usable Items (Potions, Scrolls, Books, Wands)

Usable-item dispatch turns inventory objects into spell, condition, skill, or charge effects.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Completes the item interaction picture begun in
[inventory-equipment.md](inventory-equipment.md) (which covered equip/unequip) with the
consume/use path for non-equippable item types.

---

## 1. The consume dispatch — `FUN_004680F1` (3485 B)

- **Address**: `0x004680F1`, `ret 8`, arg = character slot index (1-based).
- **Callers**: `FUN_00416D0B @ 0x416E34` (the inventory "use item" click
  handler, pushes the active member at `[0x507A6C]`), and `FUN_00468F8E @
  0x46968C` (the equip handler's fall-through for non-equippable items).
- **Character base**: `esi = slot * 0x1B3C + 0xACBCC8` (stride 0x1B3C = 6972).
- **Item**: cursor item id at `0xAD458C`; power/charge at `0xAD4590`.

### equipType switch (`0x468141`, rebased `sub eax, 0xD`)

| rebased | equipType | branch | action |
|---|---|---|---|
| 0 | 13 Reagent | `0x468D44` | reuses potion cases for itemIds 160–162 |
| 1 | 14 Potion | `0x468782` | 52-case effect table (itemIds 220–271) |
| 2 | 15 SpellScroll | `0x4685DE` | targeting mode + cast inscribed spell |
| 3 | 16 Book | `0x4684ED` | learn spell (gated on school mastery) |
| 4 | 17 MessageScroll | `0x4684C3` | open reader, NOT consumed |
| default | — | second switch | quest/special items (0x268+) |

**Wand (equipType 12) is NOT here** — wands are equipped MainHand weapons;
their "use" is the combat attack (`FUN_004156FB` wand-bolt at `0x415C5E`).

---

## 2. Potion effects (equipType 14) — hardcoded 52-case table

`0x468782`: `eax = itemId - 0xDC` (220); bounds-checked `<= 0x33` (51), then
`jmp [eax*4 + 0x468EBE]` — a 52-entry jump table (potion itemIds **220–271**).
**There is no potion→spell mapping table.** Each case hardcodes an effect by
directly mutating the character struct:

| itemId | case addr | effect |
|---|---|---|
| 220 | `0x468798` | **Heal**: `FUN_0048DB9F(char, power + 10)` — restore HP (capped at maxHP; halved if afflicted). |
| 221 | `0x468D96` | **Cure condition 6**: `FUN_00492D5D(char, 6)` (clears Poison1). |
| 222 | `0x4687B7` | **Restore SP**: `add [esi+0x1940], (power + 0xA)`, capped at GetMaxSP. |
| 223–226 | `0x4687CC`–`0x4687FC` | **Cure specific condition**: zero pairs of condition-timestamp qwords at char offsets +0x58/+0x48, +0x50/+0x40, +0x10/+0x14, etc. |
| 227+ | `0x468804`+ | **Stat buff**: magnitude `power * 0x38400 * 0.0332`; applied via buff helper `0x468C4A` (sets a buff-expiry timestamp via `FUN_00458519`). E.g. cases 264–267 add +50 to a stat (`add word [esi+0xD4/0xCC/0xC0], 0x32`). |

Common potion tail (`0x4687A8`): `FUN_004948A9(char, 0x24, 0)` play-effect,
then `jmp 0x468DA0` (consume tail → **item consumed**).

So potions apply effects **directly** (HP/SP writes, condition clears, stat
buffs) — they do NOT route through the spell dispatcher. The only shared
primitives are `FUN_0048DB9F` (HP restore, `0x48DB9F`, 101 B) and
`FUN_00492D5D` (SetCondition/cure, doc 30). A healing potion heals **one
character** (the active member), not the whole party.

---

## 3. Spell scroll (equipType 15, branch `0x4685DE`)

- Gate `FUN_00492C03(char)` (clean-state; refuse if afflicted).
- Scroll index = `itemId - 0x12B` (299). Special indices (itemIds 330/303/394/327) → special path.
- General path (`0x468684`): stashes the scroll id at `0x720804`, calls
  `FUN_004698AA` (sets the **spell-targeting cursor** — swaps to MICON1/MICON2),
  then queues a cast record into the pending-action queue at `0x50C868`
  (action code 0x92, target = scroll index, caster = char).
- **Scroll→spell table**: `0x723BB8` (`scroll.txt`, loaded by `FUN_004764C6`,
  indexed by scroll itemId). Read by `FUN_00467FBA` (`0x467FEE`).
- Reading a scroll does **not** instantly cast — it enters **targeting mode**;
  the player picks a target, then the queued action casts the inscribed spell.
  The scroll is consumed after the cast resolves.

---

## 4. Book (equipType 16, branch `0x4684ED`) — learn spell

- `edi = itemId - 0x190` (400) → **book index** (books are itemIds 400+).
- `eax = charBase + bookIndex + 0x192` → address of the **known-spell byte** in
  the character's spellbook. `cmp byte [eax], 0`: if already known → "already
  known" message, skip.
- Gate `FUN_00492C03` (clean). Then reads the character's **magic-school skill**
  (`[esi + eax*2 + 0x120]`), extracts mastery, compares against a required tier
  (school-dependent). **Refuse if school mastery insufficient.**
- On success: `mov byte [eax], 1` — writes 1 into the known-spell byte (LEARNS
  the spell). Then `FUN_004948A9(char, 0x15, 0)` play-effect → consume tail
  (**book consumed**).

So: **reading a book learns the spell, gated on the character's magic-school
mastery; book is destroyed on success.**

---

## 5. Message scroll (equipType 17, branch `0x4684C3`) — NOT consumed

- Gate `FUN_00492C03` (clean).
- `FUN_00467F4C(cursorItem)` opens a reader window: for itemIds **700–782** it
  loads a parchment background, computes `textIndex = itemId - 0x2BC`, opens a
  640×480 reader UI.
- `FUN_004948A9(char, 0x25, 0)` play-effect, then `jmp 0x468E87` — **plain
  return, does NOT reach the consume tail.** Message scrolls are displayed and
  **NOT consumed**.

---

## 6. Wand (equipType 12) — combat attack path

Wands are equipped MainHand weapons (doc 31); their "use" is the combat
attack in `FUN_004156FB`:

- `0x415C5E`: when the equipped weapon's equipType == 0xC (wand), bolt power =
  `rand() % 6 + ItemDesc[wandId].byte_0x5D2884 + 1` (per-wand spell/damage byte).
- Bolt applied via `FUN_00456D51` (effect apply).
- **Charge decrement / 0-charge behavior**: lives in the combat attack
  resolution; not fully traced in this pass. `numCharges` (Item+0x10) is
  decremented per bolt; the wand is inert (or destroyed) at 0 charges.

---

## 7. Consume tail & item destruction (`0x468DA0`)

Reached from potion/scroll/reagent paths:

- `0x468DB1`: potion (0xE) → message 0xD2; reagent (0xD) → message 0xD3.
- Queues an effect-application record into the second action queue `0x50CA50`
  (action code 0x71).
- **Item destruction**: `FUN_004680F1` never writes 0 to the cursor global
  `0xAD458C`. The actual inventory-slot decrement is performed by the
  pending-action processor `FUN_004304D6` when it drains the queues. For
  RuneHarbor (no deferred-action queue), the consume should be **inline**:
  `takeFromBackpack` after the effect applies.

---

## 8. Targeting / active-character rule

- **Target = party active member** (`[0x507A6C]`) for potions/books/message
  scrolls. Both callers push the active member as the character argument.
- **Scrolls are the exception**: they enter spell-targeting mode (cursor swap)
  so the player picks the spell target at cast time.
- Potions heal **one character** (not party-wide; party-wide is temple Heal All).

---

## Integration notes

What exists and is reusable:

- `SpellSystem::castScriptSpell(spellId, power, targetCharIdx)` — the
  opcode-driven cast that applies an effect without mana/skill checks. Ideal
  for potions (heal/cure) and scrolls.
- `Character` HP/SP fields, `clearCondition`/`clearAllConditions` (temple work).
- `Inventory::getEquipType(itemId)` returns Potion/SpellScroll/Book/Wand/...
- `Item.chargeCount` for wands.
- `Inventory::takeFromBackpack` to consume on use.

What must be built:

1. **Item→effect resolution**: potions have no spell table, so RuneHarbor needs
   a per-itemId effect mapping (the 52-case table). A faithful first pass can
   route the common cases (heal/mana/cure-condition) through direct HP/SP
   writes + `clearCondition`, and `castScriptSpell` for scroll/buff effects.
2. **`Inventory::useItem(characterIndex, backpackSlot, targetCharIndex)`** —
   the consume entry. Returns a `ShopReceipt`-style result describing the
   effect (HP healed, spell learned, etc.).
3. **Scroll→spell mapping**: a `scroll_parser` for `SCROLL.TXT`, or a runtime
   table from itemId→spellId. Without it scrolls can't resolve their spell.
4. **Book learn path**: MM7 models a known-spell byte array at `charBase+0x192`.
   RuneHarbor has no `knownSpells` member; the simplest faithful model is a
   `std::array<bool>` spellbook on `Character` + a `knowsSpell`/`learnSpell`
   pair, gated on school mastery.
5. **Widget "use" hook**: branch in `handleClick` on `getEquipType` for usable
   kinds before the equip path; right-click or a dedicated use action.

### Function index additions ([function-index.md](data/function-index.md))

`0x4680F1` (ConsumeDispatch, 3485B), `0x468141` (equipType switch),
`0x468782` (PotionBranch, 52-case), `0x4685DE` (ScrollBranch),
`0x4684ED` (BookBranch), `0x4684C3` (MessageScrollBranch),
`0x48DB9F` (HpRestore), `0x467F4C` (MessageScrollReader),
`0x4698AA` (SetTargetingCursor), `0x415C5E` (WandBoltCast in combat).

### Global additions ([globals-reference.md](data/globals-reference.md))

`0x723BB8` (scroll→spell table), `0xAD4590` (cursor item power/charge),
`0x720804` (stashed scroll id), `0x50C868`/`0x50CA50` (pending-action queues).

---

## Open questions

Track these unresolved items in the [open-question register](open-questions.md).

- **Full 52-case potion table**: ~15 of 52 cases traced in detail (heal/cure/
  mana/condition-cures/buff-stat). The complete itemId→effect enumeration
  should be derived against `items.txt`/`potion.txt` ids during implementation.
- **Wand charge decrement**: lives in `FUN_004156FB`'s weapon sub-cases
  (combat); not byte-confirmed. Treat as "decrement per use, inert at 0."
- **Book mastery switch**: the school-index arithmetic (`[esi + eax*2 + 0x120]`)
  and the skill→required-mastery mapping were observed but not fully resolved.
- **Item-destruction mechanism**: inferred as deferred to `FUN_004304D6` via
  the action queues; RuneHarbor will do it inline (simpler, same observable
  result).
