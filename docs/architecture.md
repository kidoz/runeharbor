---
title: "Architecture & Initialization"
summary: "The engine is a monolithic Win32 application organized around a single main loop."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Architecture & Initialization

The engine is a monolithic Win32 application organized around a single main loop.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

The engine is a monolithic Win32 application compiled with Microsoft Visual C++ 6.0,
targeting 32-bit x86. It links against the following external libraries and subsystems:

| Subsystem | Library / DLL | Purpose |
|-----------|--------------|---------|
| Display surfaces | DDRAW.DLL (DirectDraw) | Primary/back buffer management, surface blitting |
| 3D rendering | Direct3D (via vtable-based renderer class) | Hardware-accelerated indoor/outdoor rendering |
| Software rendering | Built-in | Fallback when D3D is unavailable |
| Audio | AUDIO.DLL (Miles Sound System / AIL) | 2D and 3D positional sound, CD audio |
| Video (legacy) | SMACKW32.DLL (RAD Game Tools) | Smacker video playback (.smk) |
| Video (newer) | BINKW32.DLL (RAD Game Tools) | Bink video playback (.bik) |
| Input | DINPUT.DLL (DirectInput) | Keyboard and mouse polling |
| Compression | Embedded zlib 1.1.3 | inflate/deflate for LOD archive entries |
| Memory | MSVC runtime heap | HeapAlloc/HeapFree via CRT wrappers |

The binary contains approximately 2,300 functions. The largest single function is
the indoor rendering pipeline at roughly 27,500 bytes; the outdoor/game-frame
renderer is roughly 19,700 bytes.

Original source file paths embedded in assertions reference
`D:\mm7Src_eng\MM7\Code\*.cpp`, confirming the original build tree layout.

---

## Data Structures

### Primary State Machine Globals

The engine's runtime behavior is governed by three hierarchical state variables:

#### GameFlowState (address 0x006A0BC4, uint32)

Controls the top-level program flow (title screen vs. gameplay vs. exit):

| Value | Meaning |
|-------|---------|
| 0 | Title screen / idle |
| 1 | Start new game |
| 2 | Credits sequence |
| 3 | Load game (from title) |
| 4 | Continue to gameplay |
| 5 | Return to title |
| 6 | Level transition |
| 9 | Load game (from in-game) / exit |
| 10 | Load game via file browser |

#### CurrentScreenMode (address 0x004E28D8, uint32)

Controls which UI screen or mode is currently displayed. Referenced in hundreds
of places throughout the codebase:

| Value | Screen / Mode |
|-------|---------------|
| 0 | Normal gameplay (exploration / 3D view) |
| 2 | Game options / console |
| 3 | Modal dialog |
| 4 | NPC dialogue / interaction popup |
| 5 | Chest / inventory interaction |
| 10 | Character creation |
| 11 (0x0B) | Save game screen |
| 12 (0x0C) | Load game screen |
| 13 (0x0D) | Rest screen |
| 16 (0x10) | Map screen |
| 17 (0x11) | Spell book / cast spell |
| 18 (0x12) | Quick spell selection |
| 19 (0x13) | Autonotes / quest log |
| 21 (0x15) | Awards screen |
| 22 (0x16) | Text / scroll screen |

The previous screen mode is saved in PreviousScreenMode (0x005067F8) before
entering a modal screen, enabling return to the prior context.

#### GameplaySubState (address 0x006A0BC8, uint32)

Controls sub-states within the gameplay loop:

| Value | Sub-state |
|-------|-----------|
| 0 | Normal / idle |
| 1 | Transitioning (break out of inner loop) |
| 2 | Level loaded / entering |
| 3 | Saving |
| 4 | Loading |
| 6 | Level change |
| 7 | Reset / restart |
| 8 | Special transition |
| 9 | Exit to title |

### Additional Key Globals

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| 0x006BE174 | MainWindowHandle | HWND | Main game window handle |
| 0x006BE1E0 | MapType | u32 | 1 = indoor (BLV), 2 = outdoor (ODM) |
| 0x006BE1E4 | EngineFlags | u32 | Bitfield (0x10 = no sound, 0x40 = no animation) |
| 0x006BE1E5 | ActivityFlags | u8 | WM_ACTIVATEAPP pause/resume state |
| 0x006BE1EE | CDROMFlag | u8 | Non-zero if CD-ROM fallback is active |
| 0x006BE158 | WindowClassName | char[] | Registered window class name |
| 0x0071FE88 | UseDefsFlag | u32 | 1 = load text tables instead of binary |
| 0x00576EAC | RedrawFlag | u32 | 1 = trigger full screen redraw |

---

## Key Algorithms

### 1. Main Entry Point (WinMain equivalent)

**Location:** 0x00462CD1 (1,205 bytes)

This is the outermost game entry point, called from CRT startup code.

**Command-line argument processing:**

The function scans the command-line string for the following flags:

| Flag | Effect |
|------|--------|
| `-usedefs` | Sets UseDefsFlag = 1 (load text `.txt`/`.def` data tables instead of binary `.bin`) |
| `-window` | Enables windowed mode |
| `-nosound` | Sets engine flag bit 0x10 (disables audio) |
| `-noanim` | Sets engine flag bit 0x40 (disables animation) |

**High-level flow:**

```asm
WinMain
  |-- Parse command-line arguments
  |-- Call InitializeEngine (0x00465245)
  |-- Enter outer loop:
  |     |-- TitleScreenLoop (0x004627F4)
  |     |-- Based on GameFlowState:
  |     |     State 1 -> CreateNewGame (0x004608A7) -> GameplayLoop (0x00463186)
  |     |     State 2 -> PlayCredits -> return to title
  |     |     State 3 -> LoadGameScreen -> GameplayLoop (0x00463186)
  |     |     State 9 -> Cleanup -> Exit
  |     |     State 10 -> OpenFileDialog (GetOpenFileNameA) -> load BLV
  |     |-- Loop back to title unless exit requested
  |-- Shutdown and exit

```

### 2. Initialization Sequence (Engine Setup)

**Location:** 0x00465245 (2,539 bytes)

Called from WinMain. This single large function performs the entire engine
initialization in the following order:

1. **RNG seed**: Calls `GetTickCount()` and uses the result to seed the
   pseudo-random number generator.

2. **Disk space check**: Calls `GetDiskFreeSpaceA()` to verify sufficient disk
   space is available. If insufficient, displays `"More RAM Memory Required"` via
   `MessageBoxA` and exits.

3. **Memory allocation check**: Attempts to allocate 16 MB and 26 MB buffers.
   On failure, displays `"Unable to Allocate 16MB of RAM"` or
   `"Unable to Allocate 26MB of RAM"` and exits.

4. **Window class registration**: Calls `RegisterClassExA` with:
   - Class name stored at global address 0x006BE158
   - Window procedure (WndProc): function at 0x00463828
   - Icon: loaded via `LoadIconA` with resource name `"MM7_ICON"`
   - Background brush: via `GetStockObject`

5. **Window creation**: Calls `CreateWindowExA` with:
   - Style: `0xCA0000` (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
   - Dimensions: 640 x 480 base resolution
   - Adjusts position using `GetSystemMetrics`, `GetWindowRect`, `GetClientRect`,
     `MoveWindow` to properly center/size the window

6. **Process priority**: Calls `SetPriorityClass(GetCurrentProcess(), ...)` to
   elevate the process priority.

7. **INI file reading**: Reads configuration from `mm6.ini` (note: the filename
   is inherited from MM6; path formatted as `%s\mm6.ini`). Uses
   `GetPrivateProfileIntA` and `GetPrivateProfileStringA` to read:
   - `[settings]` section: `soundflag`, `musicflag`, `CharVoices`, `WalkSound`,
     `ShowDamage`, `TurnDelta`, `2dacceloff`
   - `[render]` section: `graphicsmode`, `Tinting`, `GammaPos`, `Bloodsplats`
   - `[debug]` section: `debug flags`
   - `[registry]` section: `resolution`
   - Window position: `window X`, `window Y`
   - `use_cd` flag for CD-ROM mode

8. **LOD file loading**: Opens the required LOD data archives in sequence:
   - `data\icons.lod` via LOD open wrapper (0x0040FAFA), chapter `"icons"`
   - `data\events.lod` via LOD open wrapper (0x0040FAFA)
   - `data\bitmaps.lod` via LOD open wrapper (0x0040FA3A), chapter `"bitmaps"`
   - `data\sprites.lod` or `data\spriteLO.lod` (resolution-dependent) via
     sprite-specific opener (0x004AC6F8)
   - If any LOD file is missing: `MessageBoxA` with
     `"Files Missing\n\nPlease Reinstall."` and exit

9. **Renderer initialization**: Based on the `graphicsmode` setting from INI:
   - If `"SOFTWARE"` string match: initializes the software renderer path
     (functions at 0x00464A2C, 0x00464B3F, 0x00464C2C, 0x00464D6F, 0x004650B2)
   - Otherwise: initializes Direct3D via 0x0049FF8B or 0x004A0583
   - D3D initialization loads hardware texture lists: `data\d3dbitmap.hwl`,
     `data\d3dsprite.hwl`
   - Display surfaces created at 640x480, 32-bit color via DirectDraw

10. **Data table loading**: Loads game definition tables in two modes:

    **Binary mode** (default, loaded from `events.lod`):

    | File | Loader | Content |
    |------|--------|---------|
    | `dsft.bin` | 0x0044DA03 | Sprite Frame Table |
    | `dtft.bin` | 0x0044E0CC | Texture Frame Table |
    | `dtile.bin` | 0x00487FE6 | Tile Table |
    | `dpft.bin` | 0x00494C25 | Particle Frame Table |
    | `dift.bin` | 0x0049506C | Icon Frame Table |
    | `ddeclist.bin` | 0x00458685 | Decoration List |
    | `dobjlist.bin` | 0x00459115 | Object List |
    | `dmonlist.bin` | 0x004598E8 | Monster List |
    | `dchest.bin` | 0x00458B88 | Chest definitions |
    | `doverlay.bin` | 0x00458E41 | Overlay definitions |
    | `dsounds.bin` | 0x004A9E19 | Sound definitions |

    **Text mode** (when `-usedefs` flag is set, loaded from filesystem):

    | File | Loader | Content |
    |------|--------|---------|
    | `data\sft.txt` | 0x0044DABE | Sprite Frame Table |
    | `data\tft.def` | 0x0044E218 | Texture Frame Table |
    | `data\tile.def` | 0x0048802D | Tile Table |
    | `data\pft.def` | 0x00494C83 | Particle Frame Table |
    | `data\ift.txt` | 0x004950B3 | Icon Frame Table |
    | `data\declist.txt` | 0x004586E9 | Decoration List |
    | `data\objlist.txt` | 0x0045915C | Object List |
    | `data\monlist.txt` | 0x00459935 | Monster List |
    | `data\chest.def` | 0x00458BD5 | Chest definitions |
    | `data\overlay.def` | 0x00458E88 | Overlay definitions |
    | `data\sounds.def` | 0x004A9E93 | Sound definitions |

11. **Content table loading**: After the core tables, additional game content
    tables are loaded from LOD via a common tab-separated value parser.
    These include: `global.txt`, `spells.txt`, `MapStats.txt`, `items.txt`,
    `monsters.txt`, `class.txt`, `stats.txt`, `skilldes.txt`, NPC data tables
    (`npcdata.txt`, `npctext.txt`, `npcgreet.txt`, etc.), and many others.
    All use the same pattern: load from LOD, tokenize on `\t`, skip header rows,
    switch on column index to assign fields.

12. **Enter title screen**: Transfers control to `TitleScreenLoop` (0x004627F4).

### 3. Title Screen Loop

**Location:** 0x004627F4 (1,245 bytes)

Implements the main menu with the following behavior:

1. Sets CurrentScreenMode to 0 (normal mode).
2. Creates a UI overlay window at 640 x 480.
3. Loads `title.pcx` from `bitmaps.lod` as the background image and blits it
   fullscreen.
4. Loads four button textures from `icons.lod` and creates interactive buttons:

   | Texture Name | Button Label | Position (X, Y) | Button ID | Message ID |
   |-------------|-------------|-----------------|-----------|------------|
   | `title_new` | NEW GAME | (495, 172) | 0 | 0x36 |
   | `title_load` | LOAD GAME | (495, 227) | 1 | 0x37 |
   | `title_cred` | CREDITS | (495, 282) | 2 | 0x38 |
   | `title_exit` | EXIT GAME | (495, 337) | 3 | 0x39 |

   Buttons are spaced 55 pixels apart vertically starting at Y=172.

5. Enters a message pump loop: `PeekMessageA` / `TranslateMessage` /
   `DispatchMessageA`. When idle, calls `WaitMessage` to yield CPU.
6. Processes UI events via the event dispatcher (0x00435737).
7. Responds to GameFlowState changes:
   - State 0: process button hover/click highlights
   - State 3: transition to load screen, sets CurrentScreenMode = 0x0C,
     loads `lsave640.pcx` background, calls InitLoadGameScreen (0x0045E39A)
   - State 9: exit game, clean up resources

### 4. Main Game Loop (Gameplay)

**Location:** 0x00463186 (1,687 bytes)

The per-frame gameplay loop. Contains its own message pump
(`PeekMessageA` / `TranslateMessage` / `DispatchMessageA` / `WaitMessage`)
and drives all in-game logic:

- Calls the main event dispatcher (0x00435737) to process the input command
  queue (queue count at 0x0050CA50, queue array at 0x0050CA54)
- Handles video synchronization: `SmackWait` / `BinkWait` for active video
  playback
- Manages GameplaySubState transitions:
  - SubState 1: break out of inner loop (transition complete)
  - SubState 3 or 4: trigger screen redraw via 0x00466C44
  - SubState 7: reset game state via 0x004AB69F
- Manages Lloyd's Beacon save data (`data\lloyd%d%d.pcx`)
- On GameFlowState == 9 from within gameplay: triggers save/load operations
- Dispatches to the indoor renderer (0x00427DB8, ~27,500 bytes) or outdoor
  renderer (0x004304D6, ~19,700 bytes) based on MapType

### 5. Window Procedure (WndProc)

**Location:** 0x00463828 (3,144 bytes)

Registered during window class creation. Handles Win32 messages:

| Message | Behavior |
|---------|----------|
| WM_PAINT | Calls `BeginPaint` / `EndPaint`, triggers redraw |
| WM_CLOSE / WM_QUIT | Posts quit message via `PostQuitMessage` |
| WM_ACTIVATEAPP | Toggles pause state in ActivityFlags (0x006BE1E5); pauses/resumes CD audio via `AIL_redbook_pause` / `AIL_redbook_resume` |
| WM_KEYDOWN | Maps virtual keys via `MapVirtualKeyA`, forwards to input handler |
| WM_SIZE / WM_MOVE | Updates window position via `SetWindowPos`, `ClipCursor` |
| Other | Falls through to `DefWindowProcA` |

Also manages Bink video buffer offsets (`BinkBufferSetOffset`) when the window
moves, and checks color depth via `GetDeviceCaps` (requires 256-color or
higher; displays error message if not met).

---

## Constants & Enums

### Command-Line Flags

| Flag string | Internal effect |
|-------------|----------------|
| `-usedefs` | UseDefsFlag (0x0071FE88) = 1; load text data tables |
| `-window` | Windowed mode (no exclusive fullscreen) |
| `-nosound` | EngineFlags bit 0x10 set |
| `-noanim` | EngineFlags bit 0x40 set |

### Window Style

The main window is created with style `0x00CA0000`:

- `WS_OVERLAPPED` (0x00000000)
- `WS_CAPTION` (0x00C00000)
- `WS_SYSMENU` (0x00080000)
- `WS_MINIMIZEBOX` (0x00020000)

Base resolution: 640 x 480 pixels.

### UI Button Message IDs

| ID (hex) | Action |
|----------|--------|
| 0x36 | New Game |
| 0x37 | Load Game |
| 0x38 | Credits |
| 0x39 | Exit Game |
| 0x71 | NPC dialogue option |
| 0x88 | Spell/ability selection |
| 0xA2 | Scroll up (save/load list) |
| 0xA3 | Scroll down (save/load list) |
| 0xA4 | Confirm (Load/Save action) |
| 0xA5 | Select save slot (parameter = slot index 0-6) |
| 0xA6 | Cancel |
| 0xA7 | Delete save |

### Pixel Format Detection

| Bit masks (R/G/B) | Format | Internal code |
|--------------------|--------|---------------|
| 0xF800 / 0x07E0 / 0x001F | RGB565 (16-bit) | 0xC0000000 |
| 0x7C00 / 0x03E0 / 0x001F | RGB555 (15-bit) | 0x80000000 |
| bitsPerPixel == 8 | 8-bit paletted | (separate path) |

---

## Integration Points

### LOD File Dependencies

The engine requires these LOD archives to be present in the `data\` directory.
Failure to find any of them results in an immediate error dialog and exit.

| File | Chapter | Content |
|------|---------|---------|
| `data\icons.lod` | `"icons"` | UI icons, button textures, cursor sprites |
| `data\events.lod` | (events) | Binary data tables, event scripts |
| `data\bitmaps.lod` | `"bitmaps"` | Texture bitmaps, palettes, PCX images |
| `data\sprites.lod` | (sprites) | Sprite frame images (high-res mode) |
| `data\spriteLO.lod` | (sprites) | Sprite frame images (low-res fallback) |
| `data\games.lod` | (saves) | Save game data container |
| `data\new.lod` | (template) | Template for new save files |

Additionally, the video archives in the `anims\` directory:

| File | Content |
|------|---------|
| `anims\might7.vid` | Primary video archive (searched first) |
| `anims\magic7.vid` | Secondary video archive (fallback) |

### INI Configuration (mm6.ini)

The configuration file is named `mm6.ini` (inherited from MM6). It is read via
`GetPrivateProfileIntA` / `GetPrivateProfileStringA` from the game directory.

| Section | Key | Purpose |
|---------|-----|---------|
| `[settings]` | `soundflag` | Enable/disable sound |
| `[settings]` | `musicflag` | Enable/disable music |
| `[settings]` | `CharVoices` | Character voice volume/enable |
| `[settings]` | `WalkSound` | Footstep sounds enable |
| `[settings]` | `ShowDamage` | Display damage numbers |
| `[settings]` | `TurnDelta` | Turn-based movement increment |
| `[settings]` | `2dacceloff` | Disable 2D acceleration |
| `[render]` | `graphicsmode` | `"SOFTWARE"` or D3D device name |
| `[render]` | `Tinting` | Enable tinting effects |
| `[render]` | `GammaPos` | Gamma correction level |
| `[render]` | `Bloodsplats` | Enable blood splatter effects |
| `[registry]` | `resolution` | Display resolution setting |
| `[debug]` | `debug flags` | Debug visualization flags |
| (root) | `window X` | Window X position |
| (root) | `window Y` | Window Y position |
| (root) | `use_cd` | Enable CD-ROM asset fallback |

### Subsystem Architecture

#### Renderer (dual-path)

The engine supports two mutually exclusive rendering backends:

- **Software renderer**: Selected when INI `graphicsmode` equals `"SOFTWARE"`.
  Uses direct pixel buffer manipulation. Initialization flows through functions
  at 0x00464A2C, 0x00464B3F, 0x00464C2C, 0x00464D6F, 0x004650B2.
  Original source: `D:\mm7Src_eng\MM7\Code\Core3D.cpp`.

- **Direct3D renderer**: The default hardware path. Initialization at
  0x0049FF8B / 0x004A0583 creates a D3D device, validates capabilities
  (non-square textures, alpha blending), and loads hardware texture lists from
  `data\d3dbitmap.hwl` and `data\d3dsprite.hwl`.
  Original source: `D:\mm7Src_eng\MM7\Code\Screen16.cpp`.

Both paths share the same DirectDraw surface infrastructure (initialized at
0x0049D700): enumerate adapters, create DirectDraw object, set cooperative
level, create primary surface with back buffer at 640x480x32.

#### Audio (Miles Sound System)

All audio is routed through `AUDIO.DLL` (Miles Sound System). Key operations:

- `AIL_startup` / `AIL_waveOutOpen` for digital audio initialization
- `AIL_redbook_open_drive` for CD audio
- `AIL_enumerate_3D_providers` / `AIL_open_3D_provider` for 3D positional audio
  (checks registry at `SOFTWARE\Aureal\A3D` for hardware support)
- Sample-based playback: `AIL_allocate_sample_handle`, `AIL_set_sample_file`,
  `AIL_start_sample`, `AIL_end_sample`
- 3D positional: `AIL_allocate_3D_sample_handle`, `AIL_set_3D_sample_file`

Digital audio driver handle stored at 0x00F79200; CD handle at 0x00F791FC.

#### Video (Smacker / Bink)

Video playback uses RAD Game Tools libraries via the VID container system.
Videos are stored in two VID archives (`might7.vid`, `magic7.vid`), each
containing a directory of 44-byte entries (40-byte name + 4-byte offset).

Playback attempts Bink format first (`.bik`, opened with flags `0x8800000`),
falling back to Smacker (`.smk`, opened with flags `0x7140`). MMX optimization
is enabled via `SmackUseMMX(1)`. Bink audio routing is set up via
`BinkSetSoundSystem(BinkOpenMiles, digitalAudioHandle)`.

#### Input (DirectInput)

Keyboard and mouse are handled through DirectInput (`DINPUT.DLL`):

- `DirectInputCreateA` to create the DirectInput object
- Keyboard: `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp`
  (functions at 0x0043B790, 0x0043B854, 0x0043B89E, 0x0043B90F, 0x0043B991)
- Mouse: `D:\mm7Src_eng\MM7\Code\DirectInputMouse.cpp`
  (functions at 0x0043BA22, 0x0043BAF1, 0x0043BB3B, 0x0043BBAC)

The WndProc also uses `GetAsyncKeyState` for immediate key state queries and
`MapVirtualKeyA` for scan code translation.

---

## Integration notes

### Architecture Mapping

The original engine is a tightly coupled monolith. RuneHarbor decomposes this
into service interfaces with dependency injection:

| Original | RuneHarbor Equivalent |
|----------|----------------------|
| Global state variables (GameFlowState, etc.) | `IGameStateMachine` service with explicit state/transition types |
| WinMain init sequence (0x00465245) | `Application::initialize()` with DI-provided `IFileSystem`, `IRenderer`, `IAudio` |
| WndProc (0x00463828) | SDL3 event loop in `Application::processEvents()` |
| DirectDraw/D3D renderer | `IRenderer` interface (SDL3 GPU backend) |
| Miles Sound System | `IAudio` interface (SDL3 audio or platform backend) |
| DirectInput | SDL3 event system (keyboard/mouse events) |
| LOD file I/O via global file handles | `IFileSystem` with `LodArchive` class |
| INI file via GetPrivateProfileInt | `IConfigProvider` reading TOML or INI |
| Smacker/Bink via DLLs | `IVideoPlayer` with built-in Smacker/Bink decoders |

### State Machine Design

The three-level state hierarchy (GameFlowState -> CurrentScreenMode ->
GameplaySubState) should map to a proper state machine with typed enums:

```cpp
enum class FlowState { Title, NewGame, LoadGame, Credits, Gameplay, LevelTransition, Exit };
enum class ScreenMode { Gameplay, Options, NpcDialogue, Chest, CharCreate, SaveScreen, LoadScreen, Rest, Map, SpellBook, QuestLog, Awards, ... };
enum class PlaySubState { Idle, Transitioning, LevelLoaded, Saving, Loading, LevelChange, Reset, ExitToTitle };

```

### Initialization Order

The original initialization order must be respected because later steps depend on
earlier ones. In particular:

1. Configuration must be loaded before renderer selection
2. LOD archives must be opened before any texture/data loading
3. Binary/text data tables must be loaded before entering the title screen
4. The renderer must be initialized before any screen blitting

RuneHarbor should enforce this via explicit initialization phases in
`Application::initialize()`, with each phase returning `std::expected` on failure
rather than calling `MessageBoxA` and `exit()`.

### Critical Differences

- **No global mutable state**: Replace the ~50+ global variables with service
  state encapsulated behind interfaces.
- **No CRT string functions**: Use `std::string` / `std::string_view` instead of
  `strcpy`/`sprintf`/`stricmp` wrappers.
- **No raw HeapAlloc**: Use RAII containers and smart pointers.
- **Error handling**: Replace `MessageBoxA` + `exit()` patterns with
  `std::expected<T, Error>` returns that propagate to the caller.
- **Platform abstraction**: SDL3 replaces DirectDraw, DirectInput, and the
  Win32 window/message system entirely.

---

### Function Cross-Reference

| Address | Size (bytes) | Suggested Name | Role |
|---------|-------------|----------------|------|
| 0x00462CD1 | 1,205 | WinMain / GameEntry | Application entry, command-line parsing, outer loop |
| 0x00465245 | 2,539 | InitializeEngine | Window creation, LOD loading, data table loading |
| 0x004627F4 | 1,245 | TitleScreenLoop | Title screen UI, button handling, message pump |
| 0x00463186 | 1,687 | GameplayLoop | Per-frame game update, rendering dispatch |
| 0x00463828 | 3,144 | WndProc | Win32 window message handler |
| 0x00435737 | 2,699 | EventDispatcher | Input command queue processing |
| 0x004608A7 | 526 | CreateNewGame | New game initialization |
| 0x00427DB8 | 27,569 | IndoorRenderer | Indoor (BLV) rendering pipeline |
| 0x004304D6 | 19,695 | OutdoorRenderer | Outdoor (ODM) / game frame rendering |
| 0x00422698 | 4,628 | RenderDispatcher | Render dispatch and scene setup |
| 0x0049D700 | -- | DDraw_Init | DirectDraw surface initialization |
| 0x0049FF8B | -- | D3D_Init (variant 1) | Direct3D device creation and validation |
| 0x004A0583 | -- | D3D_Init (variant 2) | Alternate D3D initialization path |
| 0x00466086 | -- | ReadINI | Read mm6.ini configuration values |
