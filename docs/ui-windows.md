---
title: "UI Windows"
summary: "The UI uses fixed window records and child buttons composited over the game framebuffer."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# UI Windows

The UI uses fixed window records and child buttons composited over the game framebuffer.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

> Original source files:
>
> - `D:\mm7Src_eng\MM7\Code\Font.cpp` (string at 0x004e7d88)
> - `D:\mm7Src_eng\MM7\Code\Game.cpp` (string at 0x004e7fd8)

---

## 1. Overview

MM7 uses a custom immediate-mode windowing system for all in-game UI. The
system manages a fixed-size array of window structures, each 84 bytes, with
buttons created as child elements within windows. All UI renders as 2D
overlays composited onto the 640x480 framebuffer after 3D scene rendering.

---

## 2. Window Structure (0x54 = 84 bytes)

Windows are stored in a static array at `DAT_00506dd0` with stride 0x54
(84 bytes per entry). The array supports approximately 20 simultaneous
windows (range up to `DAT_00507478`).

```cpp
Offset  Size    Field               Notes
------  ------  ------------------  ------------------------------------
0x00    4       int X               Window left edge (pixels)
0x04    4       int Y               Window top edge (pixels)
0x08    4       int windowType      Type ID (see type table below)
0x0C    4       int width           Window width (pixels)
0x10    4       int right           X + width - 1
0x14    4       int bottom          Y + height - 1
0x18    4       int stateID         Current state within the window
0x1C    4       uint flags          Window behavior flags
0x3C    4       int zOrder          Rendering order / back buffer ref
0x48    4       int userData        Caller-supplied context data

```

Standard window dimensions match the base resolution: 640x480
(`0x280 x 0x1E0`).

### Window creation

**FUN_0041c3db** (GUIWindow::Create) at 0x0041c3db (2313 bytes):

Parameters:

- `param_1, param_2`: X, Y position
- `param_3, param_4`: Width, Height
- `param_5`: Window type ID
- `param_6`: Flags
- `param_7`: User data pointer

### Window destruction

**FUN_0041c213** (GUIWindow::Destroy) -- Removes a window from the active array
and frees associated button resources.

### Window rendering

**FUN_00469e3f** (GUIWindow::DrawBackBuffer) at 0x00469e3f (105 bytes):

Copies a pre-rendered pixel buffer to the DirectDraw back buffer. Operates
line-by-line, copying 16-bit pixels from the window's back buffer (offset
+0x3C) to the screen buffer at `DAT_00e31b54`.

Viewport boundaries from the window structure:

- `+0x40`: left
- `+0x44`: top
- `+0x48`: right
- `+0x4C`: bottom

Screen stride: `DAT_00e31b58` (640 pixels = 1280 bytes in 16-bit mode).

---

## 3. Window Types

Window types are identified by the `windowType` field (offset 0x08). Values
observed in the binary:

| ID | Hex | Name | Purpose |
|----|-----|------|---------|
| 1 | 0x01 | Main Game View | Primary 3D viewport |
| 4 | 0x04 | Dialogue | NPC dialogue / conversation window |
| 6 | 0x06 | Status Bar | Bottom status text display |
| 9 | 0x09 | Special Handler | Parameter-driven special window |
| 10 | 0x0A | Shop/NPC | Shop interface, NPC interaction screen |
| 12 | 0x0C | Inventory | Character inventory / equipment screen |
| 16 | 0x10 | Character Screen | Character stats and skills display |
| 18 | 0x12 | Spellbook | Spell selection and casting interface |
| 20 | 0x14 | Map | Automap / world map display |
| 26 | 0x1A | Rest Screen | Rest/camp interface |
| 27 | 0x1B | Chest/Container | Chest contents view |
| 31 | 0x1F | Parameter-Driven | Generic window using userData |
| 70 | 0x46 | Title Screen | Main menu (title screen overlay) |
| 90 | 0x5A | Credits | Credits scroll display |
| 96 | 0x60 | Special (96) | Unknown special purpose |
| 104 | 0x68 | Special (104) | Unknown special purpose |

---

## 4. Button System

### Button creation

**FUN_0041d0d8** (GUIButton::Create) -- Creates a UI button within a parent
window. The function is referenced by the string `"BUTTON"` at 0x004e322c.

Parameters include:

- Position (X, Y) relative to parent window
- Size (width, height)
- Button type / action ID
- Associated texture/icon name

### Button texture management

- **FUN_004101bd** (Button::SetTexture) -- Updates a button's texture reference
- **FUN_0040fb2c** (Icon::FindByName) -- Looks up icon/texture by name in the
  loaded icon table

### Title menu buttons

The title screen (FUN_004627f4) creates four buttons using textures from
ICONS.LOD:

| Texture Name | Action | X | Y |
|-------------|--------|---|---|
| `title_new` | NEW GAME | 495 | 172 |
| `title_load` | LOAD GAME | 495 | 227 |
| `title_cred` | CREDITS | 495 | 282 |
| `title_exit` | EXIT GAME | 495 | 337 |

Button spacing: 55 pixels vertical between each button.
All buttons positioned at X=495 (0x1EF).

---

## 5. Menu System

### Title screen

**FUN_004627f4** at 0x004627f4 (1245 bytes) -- Main menu loop:

1. Creates UI overlay at 640x480
2. Loads button textures from ICONS.LOD
3. Loads `title.pcx` as background (24-bit PCX, 640x480)
4. Message pump loop with `PeekMessageA`/`TranslateMessage`/`DispatchMessageA`
5. Processes button clicks, transitions via state machine `DAT_006a0bc4`:

| State | Meaning |
|-------|---------|
| 0 | Main menu (idle) |
| 1 | New Game -- calls `FUN_004608a7` |
| 2 | Load Game |
| 3 | Load Screen -- shows `lsave640.pcx` |
| 9 | Return to menu |
| 10 | Open file dialog for BLV files (debug) |

### In-game UI state

The game tracks the current UI mode through several globals:

| Address | Purpose |
|---------|---------|
| `DAT_004e28d8` | Current game mode (0=normal, 0xC=save/load, 0x10=video) |
| `DAT_006a0bc4` | Main state machine value |
| `DAT_006a0bc8` | Sub-state |
| `DAT_00507a4c` | Current active UI window pointer |
| `DAT_00507a5c` | Modal dialog active flag |
| `DAT_00507a64` | Cleanup needed flag |

---

## 6. HUD Layout

The main game HUD operates within the 640x480 base resolution. The 3D viewport
occupies a sub-region with UI chrome surrounding it.

### Viewport area

From INI configuration `[screen]` section:

- X offset: 8
- Y offset: 8
- Width: 468 (0x1D4)
- Height: 351 (0x15F)

This leaves space for:

- Right panel: character portraits, minimap, action buttons
- Bottom panel: text log / status bar

### UI frame borders

Indoor maps use themed PCX borders around the viewport. Three theme sets exist:

**Set A (default):**

- `ib-r-A.pcx` -- Right border
- `ib-b-A.pcx` -- Bottom border
- `ib-t-A.pcx` -- Top border
- `ib-l-A.pcx` -- Left border
- `IB-Foot-a.pcx` -- Footer

**Set B and Set C:** Same naming pattern with `-B` / `-C` suffixes.

Additional UI frame elements (from string references):

| Texture | Purpose |
|---------|---------|
| `ib-statR` | Status indicator - Red |
| `ib-statY` | Status indicator - Yellow |
| `ib-statG` | Status indicator - Green |
| `ib-statB` | Status indicator - Blue |
| `IB-selec-A/B` | Selection highlight |
| `IB-NPCLD-A/B/C` | NPC portrait (left) |
| `IB-NPCRD-A/B/C` | NPC portrait (right) |
| `IB-InitR/Y/G-b` | Initiative indicators (R/Y/G) |
| `IB-COMP-B` | Compass |
| `ib-autmask-b` | Automap mask |
| `ib-autin-B` / `ib-autout-B` | Automap in/out |
| `ib-mb-B` | Message bar |
| `ib-bcu-b` | Button bar |
| `evtnpc` / `evtnpc-b` / `evtnpc-c` | Event NPC portrait frame |

### Character-related buttons

| Texture | Purpose |
|---------|---------|
| `ib-cd1-d` through `ib-cd4-d` | Character portrait down states |
| `ib-m1d-b` through `ib-m4d-b` | Character menu buttons |
| `ib-m5-u` / `ib-m5-d` | Additional menu button up/down |
| `ib-m6-u` / `ib-m6-d` | Additional menu button up/down |

### Minimap

| Texture | Purpose |
|---------|---------|
| `sbmap` | Status bar map display |
| `mapbordr` | Map border frame |
| `MAPDIR1` through `MAPDIR8` | Compass direction indicators (8 directions) |

### Status bar

- `sbdate-time` (0x004e2830) -- Date/time display in status bar (FUN_00411c07)
- `fr_stats` (0x004e2f14) -- Character frame stats (FUN_004184ba)

---

## 7. Inventory UI (Window Type 12)

The inventory window displays the character's equipment and carried items.
It uses the "paper doll" approach where equipped items appear on a character
portrait, with a grid for unequipped inventory items.

### Inventory-related strings

- `"New Equipment"` (0x004dfad4) -- New equipment notification

### NPC portrait rendering

NPC portraits in dialogs and shops use the naming pattern:

- `NPC%03d` (0x004e2d58) -- e.g., `NPC001`, `NPC042`
- `npc%03u` (0x004e7cc8) -- Alternate format

---

## 8. Spellbook (Window Type 18)

The spellbook window allows spell selection and casting. Spell textures are
loaded from ICONS.LOD using the naming pattern `spell%02d` (e.g., `spell01`,
`spell27`, `spell96`).

### Spell texture references

The spell effect system (FUN_004a9030) loads numerous spell textures:

| Pattern | Examples |
|---------|---------|
| Base spells | `spell01` through `spell97` |
| Variants | `spell39c`, `spell57c`, `spell97c` |

### Tab buttons

The spellbook uses tab-style navigation, with textures referenced through
the standard button system. The string `"CAST READY"` (0x004e2cfc) indicates
a spell casting ready state within the window.

---

## 9. Character Screen (Window Type 16)

Displays character statistics, skills, and attributes.

### Character-screen strings

- `"CHAR CYCLE"` (0x004e2cd0) -- Character cycling through party members
  (FUN_004142de)

### Key bindings

- `KEY_TIMECAL` (0x004e9068) -- Time/Calendar screen toggle
- `KEY_REST` (0x004e9074) -- Rest screen toggle

---

## 10. Map Screen (Window Type 20)

The automap and world map display.

### Map-screen strings

- `"MAP BOOK"` (0x004e2c90) -- Map book heading (FUN_004142de)
- `"TIME/CAL"` (0x004e2ca8) -- Time/Calendar display (FUN_004142de)

### Map direction compass

Eight directional indicators loaded as textures:
`MAPDIR1` through `MAPDIR8` (FUN_0041b639)

---

## 11. Rest Screen (Window Type 26)

The rest/camp interface uses these textures (FUN_0041f66a):

| Texture | Purpose |
|---------|---------|
| `restmain` | Main rest screen background |
| `restb1` | Rest button 1 (rest until healed) |
| `restb2` | Rest button 2 (rest for specific hours) |
| `restb3` | Rest button 3 (wait without resting) |
| `restb4` | Rest button 4 |
| `restexit` | Exit rest screen |

---

## 12. Shop/NPC Screen (Window Type 10)

Shop and NPC interaction windows display the NPC animation (from Might7.vid
Smacker files at 460x344) in the viewport area, with dialogue text and
action buttons in the surrounding UI chrome.

### NPC portrait references

- `evtnpc` / `evtnpc-b` / `evtnpc-c` -- Event NPC portrait frames
  (three UI theme variants)
- `IB-NPCLD-A/B/C` -- NPC portrait left frame
- `IB-NPCRD-A/B/C` -- NPC portrait right frame

---

## 13. Font System

Source file: `Font.cpp` (0x004e7d88)

### Font loading

**FUN_0044c474** -- Loads a font file with an associated palette.

Font files (`.fnt` format) are loaded from LOD archives. The font palette
is specified by the `FONTPAL` entry in ICONS.LOD (string at 0x004e2658).

### Known fonts

| Font File | Loading Context | Purpose |
|-----------|----------------|---------|
| `arrus.fnt` | FUN_0041b521 | Main game text (serif) |
| `lucida.fnt` | FUN_0041b521 | UI labels (sans-serif) |
| `comic.fnt` | FUN_0041b521 | Informal text |
| `create.fnt` | FUN_0041b521 | Character creation screen |
| `smallnum.fnt` | FUN_0041b521 | Small numeric display |
| `book.fnt` | FUN_00411ab6 | Book/scroll reading |
| `book2.fnt` | FUN_00411ab6 | Book alternate |
| `autonote.fnt` | FUN_00411ab6 | Auto-notes display |
| `cchar.fnt` | FUN_004968e2, FUN_0049795a | Character screen text |
| `quick.fnt` | FUN_0049795a | Quick reference |
| `endgame.fnt` | FUN_004bf886 | Endgame/credits text |

### Font palette

All fonts share a common palette loaded as `FONTPAL` from ICONS.LOD. This
palette is referenced by multiple initialization functions:

- FUN_004968e2 (character screen)
- FUN_004bf886 (video/endgame)
- FUN_0049795a (quick reference)
- FUN_00411ab6 (books/notes)
- FUN_0041b521 (main UI)

### Font rendering functions

| Address | Proposed Name | Purpose |
|---------|---------------|---------|
| 0x0044c794 | `Font::RenderGlyph` | Render individual glyph |
| 0x0044c95f | `Font::DrawText` | Draw text string |
| 0x0044ce34 | `Font::MeasureText` | Calculate text dimensions |

---

## 14. Event Queue

The game processes UI events through a queue system:

| Address | Purpose |
|---------|---------|
| `DAT_0050ca50` | Event queue count |
| `DAT_0050ca54` | Event queue array (3 ints per entry: type, param1, param2) |
| `DAT_00576eac` | Need-redraw flag |
| `DAT_00576eb0` | Need-update flag |

The main event dispatcher is **FUN_00435737** at 0x00435737 (2699 bytes),
which processes keyboard commands, UI actions, and game state transitions.

---

## 15. Key Functions

| Address | Size | Proposed Name | Purpose |
|---------|------|---------------|---------|
| 0x00411ab6 | - | `UI::LoadBookFonts` | Load book/note fonts |
| 0x00411c07 | 3837 | `UI::DrawStatusBar` | Draw status bar with date/time |
| 0x004142de | 2641 | `UI::DrawScreenTabs` | Draw MAP BOOK, TIME/CAL, CHAR CYCLE tabs |
| 0x004184ba | 3055 | `UI::DrawCharFrame` | Draw character frame stats |
| 0x0041b521 | - | `UI::LoadMainFonts` | Load primary UI fonts |
| 0x0041b639 | 2487 | `UI::DrawMapScreen` | Draw map screen with compass |
| 0x0041c213 | - | `GUIWindow::Destroy` | Destroy UI window |
| 0x0041c3db | 2313 | `GUIWindow::Create` | Create UI window |
| 0x0041d0d8 | - | `GUIButton::Create` | Create UI button |
| 0x0041f66a | - | `UI::CreateRestScreen` | Create rest screen buttons |
| 0x00422698 | 4628 | `UI::DrawGameFrame` | Draw main game UI frame |
| 0x004264f5 | - | `Debug::MemoryStatus` | Windows Memory Status display |
| 0x004304d6 | 19695 | `Game::RenderFrame` | Main game rendering (incl. UI) |
| 0x00435737 | 2699 | `Game::EventDispatch` | Main event queue processor |
| 0x004627f4 | 1245 | `Menu::MainLoop` | Title screen main loop |
| 0x00469e3f | 105 | `GUIWindow::DrawBackBuffer` | Copy window to screen |
| 0x00469ea8 | 476 | `GUIWindow::DrawCursorOverlay` | Draw cursor/status overlay |

---

## Integration notes

### Architecture mapping

| Original | RuneHarbor |
|----------|------------|
| Fixed 84-byte window array | `std::vector<GUIWindow>` with dynamic allocation |
| Global pointer `DAT_00506dd0` | Window manager service (DI) |
| 16-bit pixel buffer blit | SDL3 texture rendering |
| PCX border loading | PNG/texture atlas |

### Key considerations

1. **Window limit:** The original supports ~20 simultaneous windows. This is
   sufficient for the game's UI depth (rarely more than 3-4 windows active).

2. **Resolution independence:** The original is hardcoded to 640x480. The
   reimplementation should use relative coordinates and scale factors.

3. **Theme system:** The three UI theme sets (A, B, C) correspond to different
   map contexts. The theme selection logic should be preserved.

4. **Font rendering:** The `.fnt` format is a custom bitmap font. The
   reimplementation can either parse the original format or use a modern
   font rendering library with compatible bitmap fonts.

5. **Button hot zones:** Original button textures like `title_new` (85x30)
   are uniform-color transparent click areas, not visible graphics. The
   visible button graphics use separate larger textures (`New1`, `Load1`,
   etc. at 214x40).

*All trademarks belong to their respective owners.*
