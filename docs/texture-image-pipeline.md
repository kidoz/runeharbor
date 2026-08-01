---
title: "Texture & Image Pipeline"
summary: "The image pipeline loads archived textures, PCX images, palettes, sprites, and hardware caches."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Texture & Image Pipeline

The image pipeline loads archived textures, PCX images, palettes, sprites, and hardware caches.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte file fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

> Original source files:
>
> - `D:\mm7Src_eng\MM7\Code\Screen16.cpp` -- DirectDraw setup, surface management
> - `D:\mm7Src_eng\MM7\Code\screen16_3d.cpp` -- D3D texture loading
> - `D:\mm7Src_eng\MM7\Code\screen16blt.cpp` -- Software blit engine

---

## Overview

MM7's texture/image pipeline has multiple layers:

1. **LOD Archives** -- Container files holding textures, icons, sprites, palettes.
2. **PCX Decoder** -- Background images (title screen, character creation, loading screens).
3. **Indexed Texture System** -- 8-bit paletted textures for walls, terrain, icons.
4. **Sprite System** -- Compressed sprite frames from sprites.lod.
5. **D3D Hardware Texture Cache** -- Pre-processed textures in HWL files.
6. **Frame Tables** -- Animation frame sequences for sprites and textures.

---

## 1. LOD Archive System

Five LOD archives loaded at startup (in order from `InitializeEngine` at `0x00465245`):

| File | Opener | Chapter | Content |
|------|--------|---------|---------|
| `data\icons.lod` | `FUN_0040fafa` (268B) | `"icons"` | UI icons, button textures, cursor sprites |
| `data\events.lod` | `FUN_0040fafa` | (events) | Binary data tables, event scripts |
| `data\bitmaps.lod` | `FUN_0040fa3a` (270B) | `"bitmaps"` | Wall/terrain textures, palettes, PCX images |
| `data\sprites.lod` | `FUN_004ac6f8` (254B) | `"sprites08"` | Sprite frame images (hi-res) |
| `data\spriteLO.lod` | `FUN_004ac6f8` | (sprites) | Sprite frames (low-res fallback) |

Additional LODs used at runtime:

| File | Purpose |
|------|---------|
| `data\games.lod` | Save game container |
| `data\new.lod` | Template for new game (maps, initial data) |

### LOD Open Pattern

The openers follow the same pattern (decompiled from Ghidra):

```text
FUN_0040fafa(filename):
  FUN_0040fa6c()              // reset state
  FUN_00461812(filename, 1)   // open and parse LOD header
  FUN_004618c7("icons")       // set chapter sub-directory
  return success

```

LOD entry lookup: `FUN_0040fb2c(name, mode)` -- searches the LOD directory for a
named entry. Mode 2 = icon lookup. Returns entry index or -1 if not found.

### LOD Entry Structure

Each LOD entry has a 32-byte directory record:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    16     name[16]            Filename (null-terminated, case-insensitive)
0x10    4      offset              Absolute offset in file
0x14    4      size                Compressed size
0x18    4      decompressedSize    Original size (0 if uncompressed)
0x1C    4      flags               Compression flags

```

External-only archives (icons.lod, sprites.lod): entries contain a 48-byte
`ImageFileHeader` before the pixel data.

### ImageFileHeader (48 bytes)

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    16     name[16]            Texture name (null-terminated)
0x10    4      size                Total entry size
0x14    4      dataSize            Compressed pixel data size
0x18    2      width               Image width in pixels
0x1A    2      height              Image height in pixels
0x1C    2      widthLn2            Log2 of width (power-of-two dimension)
0x1E    2      heightLn2           Log2 of height (power-of-two dimension)
0x20    4      palette1            Primary palette index or offset
0x24    4      palette2            Secondary palette index
0x28    4      decompressedSize    Uncompressed size (0 = not compressed)
0x2C    4      flags               Image flags / attributes

```

**Total: 0x30 = 48 bytes**

---

## 2. PCX Image Loading

PCX images (`.pcx`) are used for full-screen backgrounds and UI screens.

### Known PCX Files

| Name | Source LOD | Usage | Resolution |
|------|-----------|-------|------------|
| `title.pcx` | bitmaps.lod | Title screen background | 640x480, 24-bit (3-plane RGB) |
| `makeme.pcx` | bitmaps.lod | Character creation background | 640x480 |
| `loading%d.pcx` | bitmaps.lod | Loading screen sequence (1-7) | Variable |
| `lsave640.pcx` | bitmaps.lod | Save/Load screen background | 640x480 |
| `gamma.pcx` | bitmaps.lod | Gamma correction test image | Unknown |
| `mm6title.pcx` | bitmaps.lod | MM6-compatibility title | 640x480 |
| `winbg.pcx` | bitmaps.lod | Windowed mode background | Unknown |
| `layout.pcx` | bitmaps.lod | UI layout template | Unknown |
| `sprites.pcx` | bitmaps.lod | Sprite sheet overview | Unknown |
| `image.pcx` | bitmaps.lod | General image viewer | Unknown |
| `lloyd%d%d.pcx` | games.lod | Lloyd's Beacon saved screenshots | Variable |
| `screen%02i.pcx` | filesystem | Screenshot output | 640x480 |

### PCX Loading Function

`FUN_0040f420(filename, mode)` -- Loads a PCX from bitmaps.lod and renders it to the
screen.

PCX format handling:

- **1-plane (8bpp):** Paletted, uses embedded 256-color VGA palette at EOF.
- **3-plane (24-bit):** R, G, B planes interleaved per scanline.
- **RLE decompression:** Bytes >= 0xC0 encode run length (count = byte & 0x3F).
- String `"16bit PCX"` found in binary at `0x004ee9e8` -- indicates 16-bit conversion
  path.

---

## 3. Palette System

### Palette Storage

Palettes stored in `bitmaps.lod` as entries named `PAL001`, `PAL002`, etc.

- Each palette: 768 bytes (256 colors x 3 bytes RGB).
- `PAL000` does NOT exist -- palette ID 0 means "use default" (fallback to PAL001).
- Textures reference palettes via the `palette1` field in `ImageFileHeader`.

### Palette Application

1. LOD textures use palette index 0 as **transparent color** (alpha = 0).
2. 8-bit indexed pixels are converted to 16-bit RGB565/RGB555 via palette lookup.
3. Color conversion formulas:
   - RGB565: `R5 = R8 >> 3`, `G6 = G8 >> 2`, `B5 = B8 >> 3`
   - RGB555: `R5 = R8 >> 3`, `G5 = G8 >> 3`, `B5 = B8 >> 3`

---

## 4. Texture Frame Tables

### CSpriteFrameTable

- **Loader:** `FUN_0044da03` (756 bytes)
- **Binary file:** `dsft.bin` (from events.lod) or `data\sft.txt` (text mode)
- **Entry size:** `0x3C` (60) bytes -- sprite frame definition
- **Fields:** Frame count, offsets, animation timing
- **Error strings:**
  - `"CSpriteFrameTable::load - Out of Memory!"`
  - `"CSpriteFrameTable::load - Unable to open file: %s."`
- **Internal names:** `"S Frames"`, `"P Frames"`, `"E Frames"`

### CTextureFrameTable

- **Loader:** `FUN_0044e0cc` (240 bytes)
- **Binary file:** `dtft.bin` (from events.lod) or `data\tft.def` (text mode)
- **Entry size:** `0x14` (20) bytes -- texture animation frame
- **Error strings:**
  - `"CTextureFrameTable::load, too few arguments, %s line %i."`
  - `"CTextureFrameTable::load - Out of Memory!"`
- **Internal name:** `"Txt Frames"`
- **Fallback string:** `"The Texture Frame Table is not a supported feature."` --
  emitted when the table feature is disabled or unavailable

### Other Frame Tables

| Table | Binary File | Text File | Entry Size | Internal Name |
|-------|------------|-----------|------------|---------------|
| Tile Table | `dtile.bin` | `data\tile.def` | Variable | N/A |
| Particle Frame Table | `dpft.bin` | `data\pft.def` | Variable | N/A |
| Icon Frame Table | `dift.bin` | `data\ift.txt` | Variable | `"I Frames"` |
| Player Frame Table | (embedded) | (embedded) | Variable | N/A |
| Decoration List | `ddeclist.bin` | `data\declist.txt` | 32 bytes | N/A |
| Object List | `dobjlist.bin` | `data\objlist.txt` | Variable | N/A |

---

## 5. D3D Hardware Texture System

### HWL Files

Pre-processed D3D textures stored in hardware-format files:

- `data\d3dbitmap.hwl` -- Wall/terrain textures for D3D
- `data\d3dsprite.hwl` -- Sprite textures for D3D

These files contain textures already converted to the native D3D texture format,
avoiding per-frame palette-to-RGB conversion.

### Texture Creation

`HiScreen16::LoadTexture` (referenced via string at `0x004eeb14`):

- Error: `"HiScreen16::LoadTexture - D3Drend->CreateTexture() failed: %x"`
- Source file: `D:\mm7Src_eng\MM7\Code\screen16_3d.cpp`

The D3D renderer creates textures via:

1. Look up texture name in HWL file.
2. If found: use pre-processed D3D surface data.
3. If not found: load from LOD, convert palette to RGB, create D3D texture.

String `"D3D texture name:  %s"` at `0x004eeb44` -- logged during texture loading
(note: double space is intentional, matches original).

### D3D Texture Management

At init: `FUN_0049ff8b` / `FUN_004a0583` (D3D capability validation):

- Non-square texture support check
- Alpha blending mode check
- Device capability query

Render state management: D3D vtable at renderer object `+0x38`.

---

## 6. Sprite System

### Sprite LOD

Sprites stored in `data\sprites.lod` (hi-res) or `data\spriteLO.lod` (lo-res).

LOD chapter name: `"sprites08"` (from string at `0x004f01d0`) or `"hardSprites"`
(from string at `0x004f01e0`).

### Sprite Rendering

Two paths depending on renderer:

- **Software:** `FUN_004acb9b` (2,542 bytes) -- direct pixel blitting with
  transparency.
- **D3D:** `FUN_004a3fb3` (2,186 bytes) -- textured quad submission.

Billboard processing: `FUN_0047a819` then `FUN_0047bad3` (composite spans).

### Sprite Outline

String: `"Sprite outline currently Unsupported"` -- indicates planned but
unimplemented feature.

---

## 7. UI Texture Assets

### Title Screen Buttons

Loaded from `icons.lod` via `FUN_0040fb2c`:

- `title_new` -- New Game clickable area (85x30, transparent)
- `title_load` -- Load Game clickable area
- `title_cred` -- Credits clickable area
- `title_exit` -- Exit Game clickable area

Button hover textures (separate, visible graphics):

- `New1` -- New Game hover text (214x40)
- `Load1` -- Load Game hover text
- `Quit1` -- Quit Game hover text

### Character Creation UI Elements

Class icons from `icons.lod`:

- `IC_KNIGHT`, `IC_THIEF`, `IC_MONK`, `IC_RANGER`, `IC_DRUID`
- `FACEMASK` -- portrait mask overlay

### In-Game UI Borders (IB- prefix)

Three visual styles: A (default), B, C.

| Category | Examples | Purpose |
|----------|----------|---------|
| Borders | `ib-l-A.pcx`, `ib-t-A.pcx`, `ib-b-A.pcx`, `ib-r-A.pcx` | Left/Top/Bottom/Right edges |
| Footers | `IB-Foot-a.pcx`, `IB-Foot-b.pcx` | Bottom panel |
| Init indicators | `IB-InitR-a`, `IB-InitY-a`, `IB-InitG-a` | Red/Yellow/Green stat quality |
| Selection | `IB-selec-A`, `IB-selec-B` | Selection highlight |
| Stat bars | `ib-statR`, `ib-statY`, `ib-statG`, `ib-statB` | Red/Yellow/Green/Blue stat colors |
| Menu items | `ib-m1d-a` through `ib-m6d-a` | Menu button down states |
| Tooltips | `ib-td1-A` through `ib-td5-A` | Skill/stat tooltip backgrounds |
| Compass | `IB-COMP-A`, `IB-COMP-B` | Compass overlay |
| NPC scroll | `IB-NPCLD-A`, `IB-NPCRD-A` | NPC dialog left/right scroll |
| Auto buttons | `ib-autin-a`, `ib-autout-a`, `ib-autmask-a` | Auto-select toggle |
| Main bar | `ib-mb-A`, `ib-mb-B` | Main action bar |
| Scroll buttons | `ib-bcu-a`, `ib-bcu-b` | Scroll up button |

---

## 8. Rendering Pipeline Integration

### Frame Composition Order (back to front)

1. **Sky** -- gradient or skybox texture.
2. **Terrain / Indoor walls** -- z-buffered, textured from bitmaps.lod or d3dbitmap.hwl.
3. **Buildings / Model faces** -- z-buffered.
4. **Decals** -- alpha-blended, z-write disabled.
5. **Sprites / Billboards** -- depth-sorted, from sprites.lod or d3dsprite.hwl.
6. **Particle effects** -- additive blend.
7. **UI windows** -- 2D overlay (no z-test).
8. **Cursor** -- topmost layer.

### Rendering Functions

| Function | Address | Size | Purpose |
|----------|---------|------|---------|
| Screen16::Init | `0x0049dda4` | 1,584B | DirectDraw/D3D initialization |
| Screen16::BeginScene | `0x004a515b` | 178B | Lock surfaces / D3D BeginScene |
| Screen16::EndScene | `0x004a520d` | 116B | Unlock / D3D EndScene + Flip |
| RenderIndoor | `0x00427db8` | 27,569B | Indoor (BLV) rendering pipeline |
| RenderOutdoor | `0x004304d6` | 19,695B | Outdoor (ODM) rendering pipeline |
| ProcessBillboards | `0x0047a819` | (small) | Billboard sorting/processing |
| CompositeSpans | `0x0047bad3` | Variable | Span buffer compositing |
| GUIWindow::DrawBackBuffer | `0x00469e3f` | 105B | UI window to screen blit |
| ScreenRedraw | `0x00466c44` | Variable | Force full screen refresh |

### Dual Renderer Switch

Global `DAT_00e31af0`:

- `== 0`: Software path (direct framebuffer writes)
- `!= 0`: D3D path (pointer to D3D renderer object)

All rendering functions branch on this global -- checked hundreds of times throughout
the codebase.

---

## 9. External Dependencies

| DLL | Functions Used | Purpose |
|-----|---------------|---------|
| DDRAW.dll | `DirectDrawCreate`, `DirectDrawEnumerateA` | Surface management |
| smackw32.dll | 22 functions (`SmackOpen`, `SmackDoFrame`, `SmackBlit`, etc.) | Smacker video |
| binkw32.dll | 14 functions (`BinkOpen`, `BinkDoFrame`, `BinkCopyToBuffer`, etc.) | Bink video |
| audio.dll (Miles) | 54 functions (`AIL_*`) | Audio (2D, 3D positional, CD) |
| GDI32.dll | `GetDeviceCaps`, `GetStockObject` | Display info |
| WINMM.dll | `mciSendStringA`, `timeGetTime` | CD audio, timing |
| DINPUT.dll | `DirectInputCreateA` | Keyboard/mouse |

---

## 10. Compression

Embedded zlib 1.1.3 (`deflate`/`inflate`):

- String: `"deflate 1.1.3 Copyright 1995-1998 Jean-loup Gailly"`
- String: `"inflate 1.1.3 Copyright 1995-1998 Mark Adler"`
- Used for LOD entry decompression.
- ADPCM decompression via Miles: `_AIL_decompress_ADPCM`.

---

## Integration notes

1. **LOD loading order matters.** The five archives must be initialized in the
   documented order because later systems depend on earlier archives being
   available. The reimplementation should respect this dependency chain.

2. **PCX format has two code paths.** The 1-plane (paletted) and 3-plane (24-bit
   RGB) paths must both be implemented. `title.pcx` is 24-bit, which is the most
   common failure case when only paletted PCX support is implemented.

3. **Palette ID 0 is a sentinel.** There is no `PAL000` entry in bitmaps.lod.
   Palette ID 0 means "use default" and should fall back to `PAL001`. Treating
   ID 0 as a real palette will produce lookup failures.

4. **Transparency via palette index 0.** All indexed textures treat palette
   index 0 as transparent. When converting to RGBA for modern renderers, set
   alpha = 0 for any pixel with index 0. SDL textures must have
   `SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND)` enabled for this to work.

5. **HWL files are optional.** The D3D hardware texture cache (`d3dbitmap.hwl`,
   `d3dsprite.hwl`) is an optimization for the original D3D path. The
   reimplementation can skip HWL support entirely and load all textures from
   LOD archives, converting palettes to RGBA at load time.

6. **Frame tables drive animation.** Texture and sprite animations are fully
   data-driven via the frame table system. The binary formats (`dsft.bin`,
   `dtft.bin`, etc.) and their text equivalents (`sft.txt`, `tft.def`) must
   be parsed to support animated textures (water, lava, flickering torches).

7. **The dual renderer switch is pervasive.** The global `DAT_00e31af0` is
   checked throughout the codebase. The reimplementation should use a single
   modern rendering backend (SDL3 GPU API) and eliminate this branching entirely.
