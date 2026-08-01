---
title: "Rendering Pipeline"
summary: "The rendering pipeline supports software and Direct3D paths within a fixed frame composition order."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Rendering Pipeline

The rendering pipeline supports software and Direct3D paths within a fixed frame composition order.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

The MM7 rendering engine is a **dual-path architecture** supporting both a software
16-bit scanline rasterizer and a hardware-accelerated Direct3D path. A single global
pointer (`DAT_00e31af0`) selects the active renderer at runtime: when zero the software
path runs; when non-zero it points to the D3D renderer object.

The game renders at a fixed **640 x 480** resolution in 16-bit color, supporting both
RGB565 (6-bit green) and RGB555 (5-bit green) pixel formats. Frame composition follows
a strict back-to-front layer order: sky, terrain/walls, buildings, decals, sprites,
particles, UI overlay, cursor.

**Original source files** (from debug strings embedded in the binary):

- `Screen16.cpp` -- DirectDraw setup, surface management, screenshot capture
- `screen16_3d.cpp` -- D3D render state management, texture loading
- `screen16blt.cpp` -- Software scanline rasterizer ("blit" engine)
- `Core3D.cpp` -- 3D math primitives, debug line drawing
- `Polydata.cpp` -- Polygon data allocation (BSP, faces, map data)
- `PolyProjector.cpp` -- Vertex transformation and projection, decal geometry
- `GammaControl.cpp` -- Display gamma correction

---

## 1. DirectDraw / Direct3D Initialization

### Function: Screen16::Init (FUN_0049dda4, 1584 bytes)

This is the main display initialization entry point. It performs the following
sequence:

1. **Create DirectDraw interface** -- calls `DirectDrawCreate()` to obtain an
   `IDirectDraw` interface, then queries for `IDirectDraw4` via `QueryInterface`
   (GUID at `DAT_004d9798`).

2. **Set cooperative level:**
   - Fullscreen: flags `0x415` (`DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT`)
   - Windowed: flags `0x408` (`DDSCL_NORMAL | DDSCL_FPUSETUP`)

3. **Set display mode** to 640 x 480 x 16 (`0x280, 0x1e0, 0x10`).

4. **Create primary surface** with capabilities:
   `DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX`

5. **Obtain back buffer** via `GetAttachedSurface()` with `DDSCAPS_BACKBUFFER`.

6. **Create D3D device** (if hardware acceleration is selected):
   - Obtain `IDirect3D2` interface from DirectDraw.
   - Enumerate Z-buffer formats and create a 16-bit depth surface.
   - Attach Z-buffer to the back buffer surface.
   - Create the D3D device via `IDirect3D2::CreateDevice()`.

7. **Create viewport and clipper** for the rendering rectangle.

### Renderer object layout

The renderer object (passed as `param_1`) stores COM interface pointers at
known offsets (in int-sized units):

| Offset | Content |
|--------|---------|
| `[0]` | Mode flag (software vs. hardware) |
| `[3]` | Window handle (HWND) |
| `[7]` | Device enumeration data pointer |
| `[8]` | `IDirectDraw4*` |
| `[9]` | `IDirect3D2*` |
| `[0xB]` | `IDirect3DDevice2*` |
| `[0xC]` | Primary (front) surface |
| `[0xD]` | Z-buffer surface |
| `[0xE]` | D3D device handle |
| `[0x12]` | Error string buffer |

### Error strings (initialization failures)

The init function uses descriptive error strings for each failure point:

```text
"Init - Failed to create DirectDraw interface."
"Init - Failed to create DirectDraw4 interface."
"Init - Failed to set cooperative level."
"Init - Desktop isn't in 16 bit mode."
"Init - Failed to set display mode."
"Init - Failed to create front buffer."
"Init - Failed to create back buffer."
"Init - Failed to create clipper."
"Init - Failed to enumerate Z buffer formats."
"Init - Failed to create z-buffer."
"Init - Failed to attach z-buffer to back buffer."
"Init - Failed to get D3D interface."
"Init - Failed to create D3D device."
"Init - Failed to create viewport."

```

### Display mode enumeration

**Screen16::EnumerateDisplayModes** (FUN_004a1074, 133 bytes) enumerates
available display modes via `DirectDrawEnumerateA`. The engine string
`"Reference Rasterizer"` (at 0x004ee9a4) indicates it can fall back to the
software reference device when no hardware is available.

---

## 2. Dual Renderer Architecture

### The global renderer switch

The variable `DAT_00e31af0` is checked **hundreds of times** throughout the
codebase. Every rendering function branches on it:

- **`DAT_00e31af0 == 0`** -- Software mode: write pixels directly to the
  locked framebuffer, use span buffers for terrain, perform scanline
  rasterization.
- **`DAT_00e31af0 != 0`** -- Hardware mode: the value is a pointer to the D3D
  renderer object. Submit geometry via Direct3D API calls.

### D3D renderer object structure

When the D3D path is active, the renderer object provides access to the
`IDirect3DDevice2` vtable:

| Offset from object | Vtable method offset | D3D method |
|--------------------|--------------------|------------|
| `+0x38` | (vtable pointer) | `IDirect3DDevice2` vtable |
| -- | `+0x58` | `SetRenderState()` |
| -- | `+0x80` | `Clear()` (clear z-buffer / framebuffer) |
| -- | `+0x98` | `SetLightState()` |
| -- | `+0xa0` | `SetTransform()` |

### Hardware resource files

The D3D path uses pre-processed texture files to avoid runtime conversion:

- `data\d3dbitmap.hwl` -- Pre-processed D3D bitmap textures (walls, terrain)
- `data\d3dsprite.hwl` -- Pre-processed D3D sprite textures

### D3D capabilities checking

At startup, the D3D path validates device capabilities (FUN_0049ff8b /
FUN_004a0583). Known capability checks:

- Non-square texture support -- `"Direct3D renderer: The device doesn't support non-square textures."`
- Alpha blending modes -- `"Direct3D renderer: The device doesn't support the necessary alpha blending modes."`
- General capability query -- `"Direct3D renderer: The device failed to return capabilities."`

### Command-line / registry options

- `-window` or `startinwindow` -- Start in windowed mode
- `window X` / `window Y` -- Window position
- `Use D3D` -- Enable Direct3D renderer
- `D3D Device` -- Selected D3D device
- `Detail Level` -- Graphics quality preset

---

## 3. Pixel Format Detection (RGB565 vs RGB555)

The engine supports two 16-bit pixel formats and detects which is in use at
runtime by inspecting the green channel bit count.

### Pixel format globals

| Global | Purpose |
|--------|---------|
| `DAT_00e31b34` | Red bits count (always 5) |
| `DAT_00e31b38` | Green bits count (5 or 6) |
| `DAT_00e31b3c` | Blue bits count (always 5) |
| `DAT_00e31b40` | Red channel mask |
| `DAT_00e31b44` | Green channel mask |

### Blending mask computation

The half-intensity blending mask used throughout the software renderer is
computed as:

```text
mask = ((greenBits != 6) - 1 & 0x4A00) + 0x31EF

```

This yields:

- **`0x7BEF`** when `greenBits == 6` (RGB565 format): masks out LSBs of
  each channel for safe right-shift averaging.
- **`0x3DEF`** when `greenBits == 5` (RGB555 format): same purpose, different
  bit layout.

### Pixel layout

```text
RGB565:  RRRRR GGGGGG BBBBB   (R:5, G:6, B:5 = 16 bits)
RGB555:  0RRRRR GGGGG BBBBB   (1 unused, R:5, G:5, B:5 = 16 bits)

```

---

## 4. Frame Composition and Rendering Order

### Main frame loop

The rendering pipeline executes in this order each frame (orchestrated from
the main game loop):

```text
1. [Setup]
   Camera::SetupViewParams()              -- FUN_004407fc (847 bytes)
   Core3D::BuildFrustumPlanes()           -- FUN_004374d7 (287 bytes)

2. [Begin Frame]
   Screen16::BeginScene()                 -- FUN_004a515b (178 bytes)
     D3D path: D3DDevice::BeginScene()
     SW path:  Lock back buffer surface

3. [Clear]
   D3D path: D3DDevice::Clear() (z-buffer + color buffer)
   SW path:  Implicit via full overdraw (sky fills entire viewport)

4. [3D World Rendering]
   Indoor (mode == 1): RenderIndoor()     -- FUN_00441bf7
   Outdoor (mode == 2): RenderOutdoor()   -- FUN_00441d22
   (See docs 05 and 06 for details)

5. [Sprites / Billboards]
   ProcessBillboards()                    -- FUN_0047a819
   CompositeSpans()                       -- FUN_0047bad3
     SW: Software sprite blitter          -- FUN_004acb9b
     D3D: D3D textured quad submission    -- FUN_004a3fb3

6. [Decals / Effects]
   D3D path: Set alpha blend render states, render decals with z-write off

7. [Particle Effects]
   UpdateParticles()                      -- FUN_00440f2a (85 bytes)

8. [UI Overlay]
   GUIWindow::DrawBackBuffer()            -- FUN_00469e3f (105 bytes)

9. [End Frame]
   Screen16::EndScene()                   -- FUN_004a520d (116 bytes)
     D3D path: D3DDevice::EndScene()
     SW path:  Unlock surfaces
   Flip / present:
     D3D path: DirectDraw::Flip()
     SW path:  DirectDraw::Blt() (back to front)

```

### Layer composition order (back to front)

1. Sky (gradient or texture)
2. Terrain / indoor walls (z-buffered)
3. Building / model faces (z-buffered)
4. Decals (alpha blended, z-write disabled)
5. Sprites / billboards (sorted by depth)
6. Particle effects
7. UI windows (2D overlay, no z-test)
8. Cursor

### Map mode selection

The global `DAT_006be1e0` determines which rendering path is used:

- `1` -- Indoor (BLV) rendering via `RenderIndoor()`
- `2` -- Outdoor (ODM) rendering via `RenderOutdoor()`

---

## 5. Display Surface Management

### Screen buffer globals

| Global | Purpose |
|--------|---------|
| `DAT_00e31b54` | Back buffer pixel data pointer (uint16*) |
| `DAT_00e31b58` | Screen stride in pixels (640) |
| `DAT_00e31b4c` | Rendering enabled flag |
| `DAT_00e31a9c` | Secondary buffer reference |
| `DAT_0050575c` | Current source surface pointer |
| `DAT_00505754` | First known surface format |
| `DAT_00505758` | Second known surface format |
| `DAT_00505784` | Stride for first surface format |
| `DAT_005057ac` | Stride for second surface format |

### BeginScene / EndScene

**Screen16::BeginScene** (FUN_004a515b, 178 bytes):

- D3D path: calls `IDirect3DDevice2::BeginScene()`.
- SW path: locks the DirectDraw back buffer to obtain a pixel pointer and
  stores it in `DAT_00e31b54`.

**Screen16::EndScene** (FUN_004a520d, 116 bytes):

- D3D path: calls `IDirect3DDevice2::EndScene()`.
- SW path: unlocks the back buffer surface.

### Screenshot capture

**Screen16::SaveScreenshot** (FUN_0049f14c, 998 bytes) writes the current
back buffer to a PCX file. The strings `"24bit PCX Only!"` and `"16bit PCX"`
indicate support for both 24-bit and 16-bit PCX output.

### Viewport management

**Screen16::SetViewport** (FUN_004a0e27) configures the rendering viewport
rectangle. The viewport boundaries are stored in the camera/view parameter
structure at offsets `[0x15]` through `[0x18]` (left, top, right, bottom).

---

## 6. Software Blitting System

### Blit_Chroma (FUN_0040d7fb, 453 bytes)

Chroma-keyed 16-bit blit: copies a rectangular region from a source surface to
the back buffer, skipping pixels that match the transparency color key.

**Algorithm:**

1. Compute the transparency color from the current pixel format masks.
2. Determine source stride from one of two known surface formats
   (`DAT_00505754` or `DAT_00505758`).
3. For each scanline, for each pixel:
   - **Opaque mode** (`param_4 == 2`): copy the source pixel if it does not
     match the chroma key.
   - **Blended mode** (otherwise): average source and destination pixels:

```text
     dst = (src >> 1 & blendMask) + (dst >> 1 & blendMask)
     ```
     where `blendMask` is `0x7BEF` (RGB565) or `0x3DEF` (RGB555).

The error string `"Problem in Blit_Chroma"` (at 0x004e1a4c) is used for
assertion failures.

### Blit_Copy (FUN_0040d9c0, 351 bytes)

Non-keyed 16-bit blit: same as `Blit_Chroma` but without transparency
checking. Has both opaque and blended copy modes using the same blending
formula.

The error string `"Problem in Blit_Copy"` (at 0x004e1a88) is used for
assertion failures.

### Window-to-screen blit

**GUIWindow::DrawBackBuffer** (FUN_00469e3f, 105 bytes) copies a pre-rendered
pixel buffer from a UI window to the DirectDraw back buffer. It operates
line-by-line, copying 16-bit pixels from the window's back buffer
(at offset `+0x3C`) to the screen buffer at `DAT_00e31b54`.

The viewport boundaries come from the window structure:
- `+0x40`: left
- `+0x44`: top
- `+0x48`: right
- `+0x4C`: bottom

The screen stride is read from `DAT_00e31b58` (640 pixels = 1280 bytes in
16-bit mode).

---

## 7. Z-Buffer

### Z-buffer creation

The Z-buffer is created during Direct3D initialization as a 16-bit depth
surface. The creation sequence:

1. Enumerate available Z-buffer formats via `IDirect3D2`.
2. Create a Z-buffer surface matching the back buffer dimensions (640 x 480).
3. Attach the Z-buffer surface to the back buffer.

Error strings for Z-buffer failures:
- `"Init - Failed to enumerate Z buffer formats."`
- `"Init - Failed to create z-buffer."`
- `"Init - Failed to attach z-buffer to back buffer."`

The string `"Z Buf."` (at 0x004ef9a8, referenced by FUN_0049fd4f) appears in
debug/status output related to Z-buffer operations.

### Software depth handling

In the software renderer, depth is handled through the span-buffer system
rather than a per-pixel Z-buffer. Terrain and indoor faces are rendered via
active edge tables and sorted spans, with billboard sprites depth-sorted
before compositing.

The CVis system (`Vis.cpp`) uses a Z-buffer value lookup for object picking:
`"Undefined type requested for: CVis::get_object_zbuf_val()"` (at 0x004f1080).

---

## 8. Camera and View Setup

### Camera::SetupViewParams (FUN_004407fc, 847 bytes)

Initializes camera parameters for the current frame. Copies party position
data and computes trigonometric values for view angles.

**View parameter structure layout (offsets in int-sized units):**

| Offset | Content |
|--------|---------|
| `[2]` | Party X position |
| `[3]` | Party Y position |
| `[4]` | Party Z position |
| `[5]` | Yaw (horizontal rotation) |
| `[6]` | Pitch (vertical rotation) |
| `[7]` | Current sector ID |
| `[8]-[B]` | Precomputed sin/cos values |
| `[C]` | sin(yaw) |
| `[D]` | cos(yaw) |
| `[E]` | sin(pitch) |
| `[F]` | cos(pitch) |
| `[0x15]-[0x18]` | Viewport boundaries (left, top, right, bottom) |
| `[0x19]` | Focal length / FOV scale |
| `[0x1B]` | Viewport height |
| `[0x1C]` | Viewport width |
| `[0x1D]` | Viewport center X |
| `[0x1E]` | Viewport center Y (adjusted by focal length) |

### Party position globals

| Global | Purpose |
|--------|---------|
| `DAT_00acd4ec` | Party X |
| `DAT_00acd4f0` | Party Y |
| `DAT_00acd4f4` | Party Z |
| `DAT_00acd4f8` | Party Yaw |
| `DAT_00acd4fc` | Party Pitch |
| `DAT_00acce44` | Eye level offset (default 160 / 0xA0) |
| `DAT_00acce50` | View distance / zoom factor |

### Rotation computation

The software and D3D paths differ in how they compute rotation angles:
- **Software path:** Uses fixed-point lookup tables (via FUN_00402cae).
- **D3D path:** Uses floating-point with `2*pi` scale.

### Core3D::BuildFrustumPlanes (FUN_004374d7, 287 bytes)

Builds the 4 view frustum planes from the camera parameters. Uses `atan2`,
`sin`, and `cos` to compute plane normals based on the field of view and
aspect ratio. The frustum planes are stored at offsets `+0x34`, `+0x4C`,
`+0x64`, `+0x7C` of the camera object (left, right, top, bottom).

---

## 9. Gamma Control

Source file: `GammaControl.cpp` (debug string at 0x004e80cc)

The gamma control system manages display gamma ramp adjustments. Key functions:

| Address | Size | Name |
|---------|------|------|
| FUN_0044f2de | -- | GammaControl initialization |
| FUN_0044f350 | -- | GammaControl get/set |
| FUN_0044f434 | -- | GammaControl apply |

The error string `"Gamma control not active"` (at 0x004e80f4) is emitted when
attempting to modify gamma on a device that does not support it.

The INI configuration uses `gamma.pcx` as a UI element and `GammaPos` as the
stored gamma position value.

---

## Key Function Reference

| Address | Size | Suggested Name | Description |
|---------|------|----------------|-------------|
| 0x0040d7fb | 453 | `Blit_Chroma` | SW chroma-keyed 16-bit blit |
| 0x0040d9c0 | 351 | `Blit_Copy` | SW non-keyed 16-bit blit |
| 0x004374d7 | 287 | `Core3D::BuildFrustumPlanes` | View frustum plane computation |
| 0x004407fc | 847 | `Camera::SetupViewParams` | Camera setup with trig |
| 0x00469e3f | 105 | `GUIWindow::DrawBackBuffer` | Copy window pixels to screen |
| 0x004a0e27 | -- | `Screen16::SetViewport` | Set rendering viewport rect |
| 0x004a1074 | 133 | `Screen16::EnumerateDisplayModes` | DD display mode enumeration |
| 0x004a1e46 | 299 | `Screen16::BeginFrame` | Begin frame rendering |
| 0x004a1f71 | 80 | `Screen16::EndFrame` | End frame rendering |
| 0x004a515b | 178 | `Screen16::BeginScene` | Begin scene (lock surfaces) |
| 0x004a520d | 116 | `Screen16::EndScene` | End scene (unlock/present) |
| 0x0049dda4 | 1584 | `Screen16::Init` | DirectDraw/D3D initialization |
| 0x0049f14c | 998 | `Screen16::SaveScreenshot` | PCX screenshot writing |

---

## Integration notes

1. **Replace DirectDraw/D3D with SDL3 + modern GPU API.** The dual-renderer
   pattern has been unified into a single hardware backend powered by `SDL_GPU`.
   Instead of using `SDL_RenderGeometry` on the CPU, the modern engine uses `SDL_shadercross`
   at runtime to dynamically transcompile offline GLSL/SPIR-V shaders into native bytecode
   (Metal, Vulkan, DX12) based on the host system.

2. **Pixel format flexibility.** The engine's RGB565/RGB555 detection logic is
   unnecessary with modern 32-bit framebuffers (`SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM`), but the blending constants
   (`0x7BEF`/`0x3DEF`) must be understood when porting the software blitter
   for asset loading.

3. **Fixed 640x480 resolution.** The reimplementation supports arbitrary
   resolutions via an offscreen `SDL_GPUTexture` render target that is blitted to the main window,
   maintaining compatibility with the original 640x480 layout for UI positioning while rendering the 3D
   world at native resolutions.

4. **Frame ordering matters.** The back-to-front layer order (sky, terrain,
   buildings, decals, sprites, particles, UI) must be preserved for correct
   visual output.

5. **Separate begin/end scene model.** The original engine's
   `BeginScene`/`EndScene` pattern maps naturally to modern `SDL_GPUCommandBuffer`
   recording and `SDL_GPURenderPass` execution.
