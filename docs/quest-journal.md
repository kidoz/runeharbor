---
title: "Quest Log / Journal / Autonotes"
summary: "Quest, autonote, and map books share a common window flow backed by separate data tables."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Quest Log / Journal / Autonotes

Quest, autonote, and map books share a common window flow backed by separate data tables.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

Extends [event-engine.md](event-engine.md) (QBits/EVT) and
[ui-windows.md](ui-windows.md) (window types) with the journal-specific mechanics.

---

## 1. Three book windows, each on its own key

MM7 has **three separate book windows**, each opened by its own key, all built
from a shared "book" UI:

| Action token | Default key | Window |
|---|---|---|
| `KEY_QUEST` | Q | Quest log |
| `KEY_AUTONOTES` | A | Autonotes |
| `KEY_MAPBOOK` | M | Map book |

(HUD buttons `MAP BOOK` / `AUTONOTES` / `QUEST` open the same windows.) The
journal is rendered as a state of the book overlay, not a standalone
`GUIWindow::Create` type. Book window constructor `FUN_00411938`; unified page
dispatcher `FUN_00413CD1` switches on a sub-state ID to a per-page renderer.

| sub-state | renderer | reads | purpose |
|---|---|---|---|
| 0xC3 (195) | `0x0041115C` | 0xACD59D | **quest list builder** |
| 0xC8 (200) | `0x00413131` | 0x722D90 | **quest page renderer** |
| 0xC9 (201) | inline | 0x723598/0x72359C | **autonote page** |

---

## 2. Data tables (loaded by the master text-loader `FUN_00477033`)

### quests.txt — loader `FUN_004768AD`

- Destination: `char*` table at **0x722D94**, 512 slots (loop bound `< 0x723594`).
- Schema: each record keeps **only field 1** (the description text) as a `char*`
  pointer. Field 0 (id) is parsed but discarded — **the quest index IS the
  array position**. So `questText[N] ↔ 0x722D94 + N*4`, and QBit N ↔ slot N.

### autonote.txt — loader `FUN_00476754`

- Destination: table at **0x7235A4**, stride 8, **195 records** (bound `< 0x723BBC`).
- Schema: `{ char* text; int category }`. Category from a keyword:
  `potion`=0, `stat`=1, `obelisk`=2, `seer`=3, (other)=4, `teacher`=5.
  These map to the autonote-book tabs.

### awards.txt — loader `FUN_004763E4`

- Destination: table at **0x723D08**, stride 8, **104 records**.
- Schema: `{ char* text; int number }` — number is the bit/flag index.

---

## 3. Quest state — single "acquired" bitfield at `0xACD59D`

**There is no separate active/completed/failed state.** MM7's quest log is a
**flat list of acquired quests** — once a QBit is set it stays listed.

- **Quest-acquired bitfield: `0xACD59D`** — 1 bit per quest (1-based index),
  ~64 bytes for 512 quests.
- **Bit-test**: `FUN_00449B7A` (byte = bitfield[(idx-1)/8], mask = 0x80 >> ((idx-1)%8)).
- **Bit-set/clear**: `FUN_00449BA1`.
- **Quest-bit setter**: EVT interpreter `FUN_00444A5EE` case 16 at `0x44A8C3`:
  tests the bit; if not set and `questText[idx]` is non-null, fires the
  "New Quest!" notification (`FUN_004948A9` const 0x5D), then sets the bit.
  A quest is "given" exactly once (notification only on the 0→1 transition).
- **Journal list builder**: `FUN_0041115C` scans `0xACD59D` and shows every
  quest whose bit is set and whose text pointer is non-null.

**Implication:** RuneHarbor's richer `QuestState` (Unknown/Active/Completed/
Failed) is an *extension* beyond the original. A faithful first pass shows
all acquired quests uniformly; the active/completed subdivision can layer on
top using the existing event-engine quest-bit writes.

> Note: doc 15's "QBit variable table @ 0x72D50C" (501 entries, bit 7 =
> active/completed) is actually the **NPC data table** (npcdata.txt). Do not
> use it for the quest journal — the real quest state is the bitfield at
> `0xACD59D`.

---

## 4. Autonotes

- **Acquired bitfield: `0xACD636`** — 1 bit per autonote (1-based), ~25 bytes
  for 196 notes.
- **Display filter** (autonote page builder, `0x4137D9`): show an autonote iff
  its category == the selected tab AND its bit in `0xACD636` is set AND its
  text pointer is non-null.
- **Add-hook**: bits set/cleared by wrappers `FUN_00449BD7`, `FUN_00444A5EE`,
  `FUN_00444B01E`, `FUN_00444B9F0`, invoked from the EVT interpreter
  `FUN_0044686D` (opcode 0x20 = `EVT_GIVE_AWARD`) and from the book UI.

---

## 5. NPC news / rumors — NOT part of the journal

NPC rumors (`npcnews.txt`, 51 entries, loader `FUN_00476CB9`) are shown
**transiently during NPC dialogue**, not stored in the journal. The per-NPC
news/state block at `0xACD804` (stride 0x1B3C) is manipulated by EVT opcode
0x20 (`EVT_GIVE_AWARD`).

---

## 6. Key addresses

| Addr | Role |
|---|---|
| `0x00477033` | TextTables::LoadAll (master loader) |
| `0x004768AD` | LoadQuestsTxt → 0x722D94 |
| `0x00476754` | LoadAutonoteTxt → 0x7235A4 |
| `0x004763E4` | LoadAwardsTxt → 0x723D08 |
| `0x00449B7A` / `0x00449BA1` | bit-test / bit-set primitives |
| `0x00411938` | BookWindow::Create |
| `0x00413CD1` | Book::RenderPage dispatcher |
| `0x00413131` | QuestPage::Render |
| `0x0041115C` | QuestList::Build |
| `0x00444A5EE` (case 16) | SetQuestBit (tests + sets + notifies) |
| **0x722D94** | `questText[N]` char* table (512) |
| **0x7235A4** | `autonote[N]` {char*,cat}[195] |
| **0x723D08** | `award[N]` {char*,num}[104] |
| **0xACD59D** | quest-acquired bitfield |
| **0xACD636** | autonote-acquired bitfield |

---

## Integration notes

RuneHarbor already has a complete-but-dormant `game::QuestLog` API (state
tracking, journal entries, callbacks) and `QuestsParser`/`AutonoteParser`, but
**none of it is wired** — no `QuestLog` instance, `quests.txt`/`autonote.txt`
never parsed at startup, no event-engine bridge. To ship a journal:

1. **Instantiate + load**: own a `QuestLog` in `Application`; parse
   `quests.txt` via `QuestsParser` and call `loadQuestData`; parse
   `autonote.txt` similarly.
2. **Bridge quest bits**: the EVT `SetGlobalVar`/`SetGlobalVar2` handlers
   currently write generic game vars — also call
   `QuestLog::startQuest`/`completeQuest`/`failQuest` keyed by the qBit (the
   MM7 model is "set = acquired"; RuneHarbor can map set→start, and use a
   companion var/flag for completion).
3. **Expose to widgets**: add `QuestLog*` to `SharedGameData` (or a
   `GameWorld::questLog()` accessor).
4. **Build the journal widget** (new `src/ui/journal_widget.{hpp,cpp}`,
   modeled on `map_widget`/`character_stats_widget`): render active/completed
   quest lists + journal entries; toggle with `Q`.

The faithful minimum (matching MM7) is "show all acquired quests"; RuneHarbor's
`QuestState` subdivision (active/completed/failed) is a value-add extension
that the existing `QuestLog` API already supports.

### Function/global additions for `docs/data/`

Functions: `0x477033` (LoadAllTextTables), `0x4768AD` (LoadQuestsTxt),
`0x476754` (LoadAutonoteTxt), `0x4763E4` (LoadAwardsTxt), `0x449B7A` (BitTest),
`0x449BA1` (BitSetClear), `0x411938` (BookWindowCreate), `0x413CD1`
(BookRenderPage), `0x413131` (QuestPageRender), `0x41115C` (QuestListBuild),
`0x44A5EE` (SetQuestBit).

Globals: `0x722D94` (questText table), `0x7235A4` (autonote table),
`0x723D08` (award table), `0xACD59D` (quest-acquired bitfield), `0xACD636`
(autonote-acquired bitfield).
