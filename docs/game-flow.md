---
title: "Game Flow"
summary: "The game flow connects process startup, menus, character creation, map loading, and gameplay."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Game Flow

The game flow connects process startup, menus, character creation, map loading, and gameplay.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## 1. Overview

This document traces the complete execution flow from process launch through
intro videos, main menu, character creation, and into the main game loop.
The engine is a single-threaded Win32 message-pump application with a
top-level state machine controlling transitions between game phases.

---

## 2. PE Layout

| Section | Start | End | Size | Permissions |
|---------|-------|-----|------|-------------|
| Headers | 0x00400000 | 0x00400FFF | 4 KB | R-- |
| .text | 0x00401000 | 0x004D7FFF | 860 KB | R-X |
| .rdata | 0x004D8000 | 0x004DEFFF | 28 KB | R-- |
| .data | 0x004DF000 | 0x00F954D7 | 10.7 MB | RW- |
| .rsrc | 0x00F96000 | 0x00FA5FFF | 64 KB | R-- |

Binary: MM7-Rel.exe, PE32 x86, MSVC 6.0. Total ~2309 functions, ~860KB code.

---

## 3. Entry Point and WinMain

Entry point: `0x004cd7ae` (CRT startup code, calls into MSVC runtime
initialization before transferring control to WinMain).

WinMain equivalent: `0x00462cd1` (1,205 bytes).

### 3.1 Command-Line Processing

WinMain scans the command line for the following flags using string
comparison:

| Flag | Effect | Global |
|------|--------|--------|
| `-usedefs` | Load text data tables instead of binary `.bin` files | `DAT_0071fe88 = 1` |
| `-window` | Windowed mode (sets engine flag bit 0x01) | `DAT_006be1e8 \|= 0x01` |
| `-nosound` | Disable all sound output (sets engine flag bit 0x10) | `DAT_006be1e4 \|= 0x10` |
| `-noanim` | Disable animations (sets engine flag bit 0x40), also sets noanim global | `DAT_006be1e4 \|= 0x40`, `DAT_0071fe8c = 1` |

### 3.2 WinMain Flow

```asm
 1. FindWindowA(className)            -- prevent second instance
 2. Parse command-line flags
 3. Call InitializeEngine (0x00465245)
 4. FUN_00422698()                    -- load additional data
 5. FUN_004bf6fc()                    -- video/audio init
 6. FUN_00464d6f()                    -- additional renderer setup
 7. Set engine flags |= 0x4000
 8. FUN_0044ea43()                    -- load sprite/texture frame tables
 9. FUN_00465d0f()                    -- load content tables
10. FUN_004a0e27(0, 0, 0x27f, 0x1df) -- set viewport (640x480)
11. FUN_004647e8()                    -- cursor init
12. Enter outer loop:
    +-> TitleScreenLoop (0x004627f4)
    |   Switch on GameFlowState (DAT_006a0bc4):
    |     State 1: Stop CD audio -> FUN_004917c6 -> create new game
    |              FUN_004608a7 -> FUN_00463186 (MainGameLoop)
    |     State 2: Stop CD audio -> credits sequence -> back to title
    |     State 3: In TitleScreenLoop itself (load game from title)
    |     State 4: GameplaySubState = 1 (continue to gameplay)
    |     State 5/9: Clean up -> exit or back to title
    |     State 10: GetOpenFileNameA -> load BLV file directly
    +-- Loop back to title unless exit
13. Shutdown (SetPriorityClass, cleanup)

```

---

## 4. Initialization (0x00465245, 2539 bytes)

Called from WinMain. Single large function performing all engine bootstrap:

### 4.1 Early Setup

1. **RNG seed**: `GetTickCount()` seeds the PRNG
2. **Disk space check**: `GetDiskFreeSpaceA()` verifies available storage
3. **Window class registration**: `RegisterClassExA` with:
   - WndProc: `FUN_00463828`
   - Icon: `"MM7_ICON"` (loaded via `LoadIconA`)
   - Class name stored at `DAT_006be158`
4. **Color depth check**: `GetDeviceCaps(DC, BITSPIXEL)` -- requires 16-bit
   color depth; engine flag bit 0x02 set at `DAT_006be1e8` if not 16-bit

### 4.2 INI Loading

Configuration is read from `mm6.ini` (inherited filename from MM6):

| Section | Keys | Purpose |
|---------|------|---------|
| `[settings]` | `use_cd`, `soundflag`, `musicflag`, etc. | Core application settings |
| `[render]` | `graphicsmode`, `Tinting`, `GammaPos`, `Bloodsplats` | Rendering options |
| `[debug]` | Various debug toggles | Development/debug flags |

See [ini-configuration.md](ini-configuration.md) for full INI details.

### 4.3 Window Creation

`CreateWindowExA` with:

- Style: `0xCA0000` (`WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`)
- Dimensions: 640x480

### 4.4 Memory Allocation

Attempts to allocate 26 MB; falls back to 16 MB (based on `DAT_00df1a68`).
On failure, exits with `MessageBoxA`: `"More RAM Memory Required"`.

### 4.5 LOD Archive Opening

Archives are opened in this order:

| Order | Path | Function | Chapter Argument |
|-------|------|----------|------------------|
| 1 | `data\icons.lod` | `FUN_0040fafa` | `"icons"` |
| 2 | `data\events.lod` | `FUN_0040fafa` | -- |
| 3 | `data\bitmaps.lod` | `FUN_0040fa3a` | `"bitmaps"` |
| 4 | `data\sprites.lod` or `data\spriteLO.lod` | `FUN_004ac6f8` | (resolution-dependent) |

If any LOD is missing, the engine displays:
`MessageBoxA`: `"Files Missing\n\nPlease Reinstall."`

### 4.6 Renderer Initialization

Based on INI `graphicsmode` value:

**Software path:**

```cpp
FUN_00464a2c -> FUN_00464b3f -> FUN_00464c2c -> FUN_00464d6f -> FUN_004650b2

```

**Direct3D path:**

```text
FUN_0049ff8b / FUN_004a0583

```

Also loads `data\d3dbitmap.hwl` and `data\d3dsprite.hwl` for hardware
texture caching.

---

## 5. Intro Videos

The game plays intro videos on first launch. Video playback uses the
`CShow` class (original source: `D:\mm7Src_eng\MM7\Code\Show.cpp`).

### 5.1 Video Containers

| Container | Path | Entries | Content |
|-----------|------|---------|---------|
| `magic7.vid` | `anims\magic7.vid` | 14 | Cutscenes (1 SMK, 13 BIK) |
| `might7.vid` | `anims\might7.vid` | 162 | NPC/location animations (all SMK) |

### 5.2 Intro Sequence

Configurable via `-nointro` string check and engine flags:

| Order | Filename | Format | Resolution | Frames | FPS | Duration |
|-------|----------|--------|-----------|--------|-----|----------|
| 1 | `3DOLOGO.SMK` | Smacker (SMK2) | 640x480 | 81 | 15.0 | 5.4s |
| 2 | `JVC.BIK` | Bink (BIKf) | 320x240 | 90 | 10.0 | 9.0s |
| 3 | `INTRO.BIK` | Bink (BIKf) | 320x240 | 2,640 | 15.0 | 176.0s |

### 5.3 Video APIs

**Smacker** (loaded from `smackw32.dll`):

```cpp
SmackOpen -> SmackDoFrame -> SmackNextFrame -> SmackWait ->
SmackToBuffer -> SmackBlit -> SmackClose

```

**Bink** (loaded from `binkw32.dll`):

```cpp
BinkOpen -> BinkDoFrame -> BinkNextFrame -> BinkWait ->
BinkCopyToBuffer -> BinkClose

```

Audio routing:

- `SmackSoundUseMSS` -- routes Smacker audio through Miles Sound System
- `BinkSetSoundSystem(BinkOpenMiles, ...)` -- routes Bink audio through Miles

Error string: `"Invalid movie requested in CShow::Run()"` at `0x004efea4`.

Original sources:

- `D:\mm7Src_eng\MM7\Code\Show.cpp`
- `D:\mm7Src_eng\MM7\Code\Video.cpp`

See [video-system.md](video-system.md) for full video format details.

---

## 6. Title Screen Loop (0x004627f4, 1245 bytes)

### 6.1 Setup

1. Sets `CurrentScreenMode (DAT_004e28d8) = 0`
2. Creates UI overlay window at 640x480 via `FUN_0041c3db`
3. Loads `title.pcx` background via `FUN_0040f420("title.pcx", 0)`
4. Calls `FUN_00466c44()` to redraw screen

### 6.2 Button Creation

Four buttons are loaded from `icons.lod` and created on the title screen:

| Texture Name | LOD Lookup | X | Y | Message ID | Button Index |
|-------------|-----------|---|---|------------|--------------|
| `title_new` | `FUN_0040fb2c("title_new", 2)` | 0x1EF (495) | 0xAC (172) | 0x36 | 0 |
| `title_load` | `FUN_0040fb2c("title_load", 2)` | 0x1EF (495) | 0xE3 (227) | 0x37 | 1 |
| `title_cred` | `FUN_0040fb2c("title_cred", 2)` | 0x1EF (495) | 0x11A (282) | 0x38 | 2 |
| `title_exit` | `FUN_0040fb2c("title_exit", 2)` | 0x1EF (495) | 0x151 (337) | 0x39 | 3 |

All buttons positioned at X=495. Vertical spacing: 55 pixels between each
button. These textures are transparent clickable regions -- the actual
visible button text comes from the `title.pcx` background image.

### 6.3 Message Pump

The title screen runs a standard Win32 message pump:
`PeekMessageA` / `TranslateMessage` / `DispatchMessageA`, with
`WaitMessage` called when the application is paused.

### 6.4 Button Hover Handling

Iterates the linked list of buttons at `window + 0x4C`. For each button,
checks if the cursor position `(iVar2, iVar4)` falls within button bounds.
On hover, renders the highlight texture via
`FUN_004a5e42(0x1EF, buttonY, texturePtr)`.

### 6.5 Load Game from Title

When `GameFlowState == 3`:

- Sets `CurrentScreenMode = 0x0C` (load screen)
- Loads `lsave640.pcx` as background via `FUN_0040f420`
- Creates a new UI window
- Calls `FUN_0045e39a` (InitLoadGameScreen)

---

## 7. Create New Game (0x004608a7)

Triggered when `GameFlowState == 1` from the title screen.

### 7.1 File Setup

1. Copies template files from `data\new.lod` to `data\games.lod`
2. Sets initial map: `"out01.odm"` (Emerald Island outdoor map)

### 7.2 Party Starting Position

Initial coordinates are set to Emerald Island spawn point:

| Global | Value | Meaning |
|--------|-------|---------|
| `DAT_00acd500` | `0x3108` (12552) | Party X position |
| `DAT_00acd4ec` | `0x3108` (12552) | Stored X backup |
| `DAT_00acd504` | `0x718` (1816) | Party Y position |
| `DAT_00acd4f0` | `0x718` (1816) | Stored Y backup |
| `DAT_00acd50c` | `0x200` (512) | Party Z position |
| `DAT_00acd4f8` | `0x200` (512) | Stored Z backup |

Angles and facing direction are initialized to 0.

### 7.3 Party/Character Initialization

Calls `FUN_0045f4a2` (2919 bytes) to initialize the party structure and
default character attributes.

Original source: `D:\mm7Src_eng\MM7\Code\Party.cpp`

---

## 8. Character Creation Screen

The character creation UI uses `CurrentScreenMode = 10`.

### 8.1 Background and Frame Assets

| Asset | Source | Purpose |
|-------|--------|---------|
| `makeme.pcx` | `bitmaps.lod` | Character creation background |
| `FACEMASK` | `icons.lod` | Face portrait mask overlay |

### 8.2 Class Icons

The character creation screen displays icons for the five base classes:

| Texture | Class |
|---------|-------|
| `IC_KNIGHT` | Knight |
| `IC_THIEF` | Thief |
| `IC_MONK` | Monk |
| `IC_RANGER` | Ranger |
| `IC_DRUID` | Druid |

### 8.3 UI Border and Frame Textures

Three visual theme sets (A, B, C) with consistent naming:

**Border pieces:**

- `ib-l-A.pcx`, `ib-t-A.pcx`, `ib-b-A.pcx`, `ib-r-A.pcx` (and B, C variants)

**Footer:**

- `IB-Foot-a.pcx`, `IB-Foot-b.pcx`, `IB-Foot-c.pcx`

**Stat quality indicators:**

| Texture | Meaning |
|---------|---------|
| `IB-InitR-a` | Red -- poor stat quality |
| `IB-InitY-a` | Yellow -- average stat quality |
| `IB-InitG-a` | Green -- good stat quality |

**Selection and stat display:**

| Texture Pattern | Purpose |
|-----------------|---------|
| `IB-selec-A`, `IB-selec-B`, `IB-selec-C` | Selection highlight |
| `ib-statR`, `ib-statY`, `ib-statG`, `ib-statB` | Stat bar colors (Red/Yellow/Green/Blue) |

**Menu buttons:**

- `ib-m1d-a` through `ib-m6d-a` (6 menu items with down states)

**Skill tooltip:**

- `ib-td1-A` through `ib-td5-A`

**Portrait auto-select:**

- `ib-autin-a`, `ib-autout-a`, `ib-autmask-a`

### 8.4 Party Structure at Creation

Four characters, each 7004 bytes. Base address: `DAT_00acd804`.

| Field | Offset | Size | Notes |
|-------|--------|------|-------|
| Sex | +0xB8 | 1 byte | 0=Male, 1=Female |
| Class ID | +0xB9 | 1 byte | Class identifier |

Portrait cycling: portrait index is taken modulo 20 (10 male + 10 female
portraits available).

---

## 9. Main Game Loop (0x00463186, 1687 bytes)

Per-frame gameplay loop with its own message pump.

### 9.1 Frame Structure

```text
1. PeekMessageA / TranslateMessage / DispatchMessageA / WaitMessage
2. Video sync: SmackWait / BinkWait for active video playback
3. Event dispatch: FUN_00435737 (2699 bytes) -- processes input command queue
4. GameplaySubState transitions:
   - SubState 1: break (transition complete)
   - SubState 3/4: screen redraw via FUN_00466c44
   - SubState 7: reset via FUN_004ab69f
5. Render scene based on MapType:
   - MapType 1 (indoor): FUN_00427db8 (27,569 bytes)
   - MapType 2 (outdoor): FUN_004304d6 (19,695 bytes)

```

### 9.2 Renderer Selection

| MapType Value | Map Format | Renderer Function | Size |
|---------------|------------|-------------------|------|
| 1 | Indoor (BLV) | `FUN_00427db8` | 27,569 bytes |
| 2 | Outdoor (ODM) | `FUN_004304d6` | 19,695 bytes |

---

## 10. Window Procedure (0x00463828, 3144 bytes)

The WndProc handles standard Win32 messages:

| Message | Value | Behavior |
|---------|-------|----------|
| `WM_CREATE` | 0x01 | Color depth check, requires 256+ colors |
| `WM_PAINT` | 0x0F | `BeginPaint`/`EndPaint`, triggers screen redraw |
| `WM_ACTIVATEAPP` | 0x1C | Pause/resume (toggles engine flag 0x100), pause/resume CD audio |
| `WM_KEYDOWN` | 0x100 | `MapVirtualKeyA`, forwards to input handler |
| `WM_SYSCOMMAND` | 0x112 | Blocks screensaver (0xF140) and monitor power-off (0xF170) |
| `WM_CLOSE` / `WM_QUIT` | -- | `PostQuitMessage` |

---

## 11. Key Global Variables

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| `0x006A0BC4` | GameFlowState | u32 | Top-level flow control (0=title, 1=new game, 2=credits, 3=load, 9=exit) |
| `0x006A0BC8` | GameplaySubState | u32 | Sub-state within gameplay |
| `0x004E28D8` | CurrentScreenMode | u32 | Active UI screen (0=game, 10=chargen, 12=load) |
| `0x006BE174` | MainWindowHandle | HWND | Win32 window handle |
| `0x006BE1E0` | MapType | u32 | 1=indoor (BLV), 2=outdoor (ODM) |
| `0x006BE1E4` | EngineFlags | u32 | Engine behavior bitfield |
| `0x006BE1E8` | DebugFlags | u32 | Debug/window mode bitfield |
| `0x006BE16C` | hInstance | HINSTANCE | Application instance handle |
| `0x006BE158` | WindowClassName | char[] | Registered Win32 window class name |
| `0x0071FE88` | UseDefsFlag | u32 | 1=use text data tables instead of binary |
| `0x0071FE8C` | NoAnimFlag | u32 | 1=animations disabled |
| `0x00576EAC` | RedrawFlag | u32 | 1=force screen redraw |
| `0x00ACD804` | PartyBase | char[28016] | Four characters x 7004 bytes each |

---

## 12. State Machine

### 12.1 GameFlowState (DAT_006a0bc4)

Controls top-level transitions from the outer loop in WinMain:

```text
                     +----------+
                     |  Launch  |
                     +----+-----+
                          |
                          v
                  +-------+--------+
          +------>| Title Screen   |<---------+
          |       | (0x004627f4)   |          |
          |       +---+--+--+--+--+          |
          |           |  |  |  |             |
          |     State:1  2  3  9/5           |
          |           |  |  |  |             |
          |           v  |  |  v             |
          |    +------+  |  |  +------+      |
          |    | New  |  |  |  | Exit |      |
          |    | Game |  |  |  +------+      |
          |    +--+---+  |  |                |
          |       |      |  v                |
          |       |      | +------+          |
          |       |      | | Load |          |
          |       |      | | Game |          |
          |       |      | +--+---+          |
          |       |      |    |              |
          |       |      v    |              |
          |       |  +--------+              |
          |       |  |Credits |              |
          |       |  +---+----+              |
          |       |      |                   |
          |       +------+----->+            |
          |                     |            |
          |              +------v------+     |
          |              | Main Game   |     |
          |              | Loop        +-----+
          |              | (0x00463186)|
          |              +------+------+
          |                     |
          +---------------------+
              (back to title)

```

| State | Meaning | Action |
|-------|---------|--------|
| 0 | Title screen idle | Remain in title loop |
| 1 | New Game | Stop CD audio, call `FUN_004917c6`, `FUN_004608a7` (create new game), enter `FUN_00463186` (main game loop) |
| 2 | Credits | Stop CD audio, run credits sequence, return to title |
| 3 | Load Game | Handled within TitleScreenLoop: show load screen (`lsave640.pcx`), call `FUN_0045e39a` |
| 4 | Continue | Set `GameplaySubState = 1`, continue to gameplay |
| 5 | Clean up / back to title | Cleanup, loop back |
| 9 | Exit | Cleanup, break from outer loop |
| 10 | Debug: Open BLV | `GetOpenFileNameA` dialog, load BLV file directly |

### 12.2 GameplaySubState (DAT_006a0bc8)

Controls transitions within the main game loop:

| SubState | Behavior |
|----------|----------|
| 1 | Break (transition complete) |
| 3 | Screen redraw via `FUN_00466c44` |
| 4 | Screen redraw via `FUN_00466c44` |
| 7 | Reset via `FUN_004ab69f` |

### 12.3 CurrentScreenMode (DAT_004e28d8)

Tracks which UI screen is currently active:

| Value | Hex | Screen |
|-------|-----|--------|
| 0 | 0x00 | Main game view |
| 10 | 0x0A | Character creation |
| 12 | 0x0C | Load game screen |

---

## 13. Original Source Files

Debug strings embedded in the binary reveal the original source tree at
`D:\mm7Src_eng\MM7\Code\`:

| Source File | Purpose |
|-------------|---------|
| `am_nw.cpp` | Automap / minimap |
| `Core3D.cpp` | 3D math, debug line drawing |
| `Damage.cpp` | Damage calculation |
| `DirectInputKeyboard.cpp` | Keyboard input via DirectInput |
| `DirectInputMouse.cpp` | Mouse input via DirectInput |
| `Events.cpp` | Event engine |
| `Font.cpp` | Font rendering |
| `Game.cpp` | Main game logic |
| `GammaControl.cpp` | Display gamma correction |
| `Generate.cpp` | World generation |
| `Itemdata.cpp` | Item definitions |
| `KeyboardAsync.cpp` | Async keyboard polling |
| `Light.cpp` | Lighting system |
| `LoadSave.cpp` | Save/load system |
| `MobileLightStack.cpp` | Dynamic light stack |
| `Mouse.cpp` | Mouse cursor management |
| `MouseAsync.cpp` | Async mouse polling |
| `Odbuild.cpp` | Outdoor map builder |
| `Odmap.cpp` | Outdoor map data |
| `Odspan.cpp` | Outdoor span buffer |
| `Party.cpp` | Party management, character struct |
| `Polydata.cpp` | Polygon data / BSP |
| `PolyProjector.cpp` | Vertex projection, decals |
| `Screen16.cpp` | DirectDraw setup, screenshots |
| `screen16_3d.cpp` | D3D render state, texture loading |
| `screen16blt.cpp` | Software scanline rasterizer |
| `seffects.cpp` | Special effects |
| `Show.cpp` | Video playback (CShow class) |
| `Sound.cpp` | Audio system |
| `StationaryLightStack.cpp` | Static light stack |
| `Video.cpp` | Video container (VID files) |
| `Vis.cpp` | Visibility / culling |

---

## 14. Key Functions

| Address | Size | Proposed Name | Purpose |
|---------|------|---------------|---------|
| 0x00462cd1 | 1,205 | `WinMain` | Application entry, outer loop |
| 0x00465245 | 2,539 | `InitializeEngine` | Full engine bootstrap |
| 0x004627f4 | 1,245 | `TitleScreenLoop` | Title menu loop and state dispatch |
| 0x004608a7 | -- | `CreateNewGame` | Copy template LOD, set spawn position |
| 0x0045f4a2 | 2,919 | `InitPartyCharacters` | Party/character initialization |
| 0x00463186 | 1,687 | `MainGameLoop` | Per-frame gameplay loop |
| 0x00463828 | 3,144 | `WndProc` | Win32 window procedure |
| 0x00435737 | 2,699 | `EventDispatch` | Input command queue processor |
| 0x00427db8 | 27,569 | `RenderIndoor` | Indoor (BLV) renderer |
| 0x004304d6 | 19,695 | `RenderOutdoor` | Outdoor (ODM) renderer |
| 0x00466c44 | -- | `RedrawScreen` | Full screen redraw |
| 0x0041c3db | 2,313 | `GUIWindow::Create` | Create UI overlay window |
| 0x0040fb2c | -- | `Icon::FindByName` | Look up texture by name in icons.lod |
| 0x0040f420 | -- | `LoadPCX` | Load PCX background image |
| 0x004a5e42 | -- | `BlitTexture` | Render texture at screen coordinates |
| 0x0045e39a | -- | `InitLoadGameScreen` | Initialize load game UI |
| 0x004647e8 | -- | `CursorInit` | Initialize mouse cursor |
| 0x004a0e27 | -- | `SetViewport` | Set 3D rendering viewport bounds |
| 0x0044ea43 | -- | `LoadFrameTables` | Load sprite/texture frame tables |
| 0x00465d0f | -- | `LoadContentTables` | Load game content data tables |
| 0x004917c6 | -- | `StopCDAudio` | Stop CD audio playback |
| 0x004ab69f | -- | `ResetGameState` | Reset game state (SubState 7) |
| 0x00422698 | 4,628 | `LoadAdditionalData` | Post-init data loading |
| 0x004bf6fc | -- | `VideoAudioInit` | Video and audio subsystem init |
| 0x00464d6f | -- | `RendererSetup` | Additional renderer configuration |

---

## Integration notes

### Architecture mapping

| Original Concept | RuneHarbor Approach |
|-----------------|---------------------|
| WinMain + Win32 message pump | SDL3 event loop (`SDL_PollEvent`) |
| `FindWindowA` single-instance check | File lock or omit (modern OS handles this) |
| `GetPrivateProfileInt` INI parsing | Cross-platform config (TOML/INI) |
| `RegisterClassExA` / `CreateWindowExA` | `SDL_CreateWindow` |
| `GetDeviceCaps(BITSPIXEL)` | SDL3 display info |
| `GameFlowState` global state machine | `IGameFlowService` with state pattern |
| Global variable soup | Dependency-injected services |
| 640x480 hardcoded resolution | Resolution-independent rendering with scale factor |

### State machine design

The original GameFlowState / GameplaySubState / CurrentScreenMode trio
should map to a hierarchical state machine:

```text
IGameFlowService
  +-- TitleState (handles menu, video playback)
  +-- CharacterCreationState
  +-- GameplayState
  |     +-- IndoorState (BLV renderer)
  |     +-- OutdoorState (ODM renderer)
  +-- LoadGameState
  +-- CreditsState

```

### Key considerations

1. **Initialization order matters.** LOD archives must be opened before any
   texture lookups. The original enforces this by sequencing calls in
   `InitializeEngine`.

2. **The template LOD.** New games copy `data\new.lod` to `data\games.lod`.
   RuneHarbor should support this copy-on-new-game pattern or use an
   equivalent template system.

3. **Spawn coordinates.** The Emerald Island spawn point (12552, 1816, 512)
   is hardcoded. This should be data-driven in the reimplementation.

4. **Character creation portrait cycling.** The modulo-20 portrait index
   wraps across 10 male and 10 female portraits. The reimplementation
   should preserve this mapping.

5. **Video skipping.** All intro videos are skippable via keypress (the
   event dispatcher `FUN_00435737` checks for skip input during playback).

*All trademarks belong to their respective owners.*
