---
title: "Outdoor Rendering (ODM)"
summary: "Outdoor rendering combines heightmapped terrain, buildings, sky, sprites, and visibility tests."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Outdoor Rendering (ODM)

Outdoor rendering combines heightmapped terrain, buildings, sky, sprites, and visibility tests.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

Outdoor levels (ODM maps) use a **heightmap-based terrain grid** with
**level-of-detail (LOD) banding**, combined with **3D building models** placed
on the terrain. The outdoor renderer is the second largest rendering subsystem,
with the main function (FUN_004304d6) spanning 19,695 bytes.

The terrain is rendered as horizontal spans in software mode and as subdivided
triangle meshes in D3D mode. Buildings are standalone 3D models with BSP-ordered
face lists. Sky rendering uses texture mapping or a color gradient depending on
configuration.

**Original source files** (from debug strings):

- `Odmap.cpp` -- Outdoor map loading and structure
- `Odbuild.cpp` -- Outdoor building face rendering
- `Odspan.cpp` -- Outdoor terrain span rendering (software)

---

## 1. Outdoor Render Entry Point

### RenderOutdoor (FUN_00441d22, 57 bytes)

Called when `DAT_006be1e0 == 2` (outdoor map mode). Dispatches to:

```cpp
1. FUN_0047a5a2()    -- Main outdoor scene processing
2. FUN_0047a819()    -- Process billboards (-> FUN_0048abd9)
3. FUN_004c2e6c()    -- Additional outdoor processing
4. FUN_00440f2a()    -- Particle effects update

```

### Outdoor::RenderScene (FUN_0047a5a2, 631 bytes)

The main outdoor rendering orchestrator. It performs the following steps:

```text
 1. Compute camera world position from party position + view angle offsets
 2. FUN_00481ebb(), FUN_00486012(), FUN_00481edd()
      -- Terrain visibility setup (heightmap lookups, tile classification)
 3. FUN_0047f44f() / FUN_0047f45c()
      -- Convert world position to tile grid coordinates
 4. FUN_00487dc2(), FUN_00487dad()
      -- Terrain normal / height computations
 5. FUN_004892cc()
      -- Sky texture update (when sky texture ID changes)
 6. FUN_004893a7()
      -- Sky rendering
 7. FUN_0043823f()
      -- Shared utility

 [Software path (DAT_00e31af0 == 0):]
 8a. FUN_004789e2()  -- Render outdoor building faces (SW)
 8b. FUN_0047f5ca()  -- Render terrain spans
 8c. FUN_00486f96()  -- Outdoor span edge processing (active edge table)
 8d. FUN_00487359()  -- Terrain triangle rasterization

 [D3D path (DAT_00e31af0 != 0):]
 8a. FUN_00479547()  -- D3D outdoor terrain rendering
 8b. FUN_00478411()  -- D3D building rendering
 8c. FUN_0047f5ca()  -- Shared terrain data preparation

 9.  Clear billboard counts
10.  FUN_0047b430()  -- Collect and render outdoor sprite billboards
11.  FUN_0047a962()  -- Render decorations (if DAT_006bdf48 == 0)
12.  FUN_0047af15()  -- Additional outdoor objects
13.  FUN_0047bad3()  -- Software span compositor (billboard compositing)
14.  FUN_00485f57()  -- Finalize outdoor frame

```

---

## 2. Terrain Grid System

### Grid coordinate conversion

Two functions convert between world coordinates and tile grid positions:

- **FUN_0047f44f** -- Convert world X to tile column index.
- **FUN_0047f45c** -- Convert world Y to tile row index.

The outdoor terrain is organized as a regular grid of tiles. Each tile has a
height value (from the heightmap), a texture index, and normal data for
lighting.

### Terrain normal and height computation

- **FUN_00487dc2** -- Compute terrain normals at grid vertices.
- **FUN_00487dad** -- Look up terrain height at a given world position.

These are used both for rendering (lighting, texture mapping) and for gameplay
(collision detection, movement).

---

## 3. LOD Bands

The outdoor terrain uses a **three-band LOD system** to reduce rendering cost
at distance. The band distances are configurable via the INI file:

```ini
[outdoor]
gridband1 = 10    ; Near band (tiles), full detail
gridband2 = 15    ; Medium band, reduced detail
gridband3 = 25    ; Far band, lowest detail before fog

```

### Band behavior

| Band | Distance (tiles) | Detail level |
|------|------------------|--------------|
| Band 1 | 0 -- 10 | Full detail: all terrain subdivisions rendered |
| Band 2 | 10 -- 15 | Medium detail: fewer subdivisions per tile |
| Band 3 | 15 -- 25 | Low detail: minimal geometry, heavy fog/mist |
| Beyond | > 25 | Not rendered (fully fogged or clipped) |

The bands work in conjunction with the **distance shading system** (see
[lighting.md](lighting.md)):

```ini
[shading]
dist_shade     = 0x800   (2048)   ; Shading starts
dist_shademist = 0x1000  (4096)   ; Mist blending starts
dist_mist      = 0x2000  (8192)   ; Full mist/fog

```

---

## 4. Terrain Subdivision

The software renderer subdivides terrain tiles into smaller triangles for
texture mapping accuracy. The subdivision parameters are configurable:

```ini
[texmapping]
terrain_subdivsize = 16    ; 16 pixels per terrain tile segment
terrain_subdivpow2 = 4     ; log2(16) = 4 bits shift
building_subdivsize = 32   ; 32 pixels per building face segment
building_subdivpow2 = 5    ; log2(32) = 5 bits shift

```

### Mipmap distances

The engine selects mipmap levels based on distance from the camera:

```ini
[mipmapping]
ter_mm1 = 0x800    (2048)   ; Terrain mipmap level 1 distance
ter_mm2 = 0x1000   (4096)   ; Terrain mipmap level 2 distance
ter_mm3 = 0x2000   (8192)   ; Terrain mipmap level 3 distance
bld_mm1 = 0x400    (1024)   ; Building mipmap level 1 distance
bld_mm2 = 0x800    (2048)   ; Building mipmap level 2 distance
bld_mm3 = 0x1000   (4096)   ; Building mipmap level 3 distance

```

Terrain uses more conservative (larger) mipmap distances than buildings,
reflecting the fact that terrain textures are viewed at shallower angles and
benefit from higher resolution at greater distances.

---

## 5. Terrain Span Rendering (Software)

### Outdoor::ProcessTerrainSpans (FUN_00486f96, 963 bytes)

Processes the active edge table for outdoor terrain rendering. Uses a sorted
edge list stored at globals `DAT_0080c858` and `DAT_0080c824` with insertion
sort.

The terrain is rendered as **horizontal pixel spans** between terrain polygon
edges. Each span carries:

- Depth value (for depth sorting)
- Texture coordinates (interpolated)
- Lighting information (ambient + distance shading)

This function corresponds to `Odspan.cpp` (debug string at 0x004ec328).

### Terrain triangle rasterization

**FUN_00487359** rasterizes individual terrain triangles into the span buffer.
This is called after edge processing to fill in the terrain surface.

### Terrain data preparation

**FUN_0047f5ca** prepares terrain data for rendering. This function is shared
between the software and D3D paths -- it sets up tile visibility, texture
assignments, and vertex positions.

The string `"SPANS"` (at 0x004ec2ec, referenced by FUN_00486a2c) relates to
the span buffer system used for outdoor terrain.

### Texture frame table

The string `"The Texture Frame Table is not a supported feature."` (at
0x004ec2f4, referenced by FUN_00486b52) indicates that animated texture
sequences on terrain tiles have a fallback/error path.

---

## 6. Sky Rendering

### Sky texture management

**FUN_004892cc** updates the sky texture when the sky texture ID changes.
**FUN_004893a7** performs the actual sky rendering.

Sky texture names from the binary:

- `plansky1` -- Default sky texture (INI `[textures] sky = plansky1`)
- `plansky3` -- Alternative sky texture
- `plansky%d` -- Parameterized sky texture name (supports multiple skies)
- `sky043` -- Specific named sky texture
- `MAKESKY` -- Sky generation/composition command (at 0x004ee818)

The error string `"Invalid Sky Tex Handle"` (at 0x004ec180, referenced by
FUN_0047cde6) is emitted when the sky texture cannot be loaded.

### Sky color gradient

When the `nosky` option is disabled, the sky is rendered as a vertical color
gradient that interpolates between configurable day/night colors:

```ini
[outdoor]
RGBDayTop_r    = 0x51 (81)     RGBDayTop_g    = 0x79 (121)   RGBDayTop_b    = 0xEC (236)
RGBDayBottom_r = 0x99 (153)    RGBDayBottom_g = 0xC1 (193)   RGBDayBottom_b = 0xED (237)
RGBNightTop_r  = 0x00 (0)      RGBNightTop_g  = 0x00 (0)     RGBNightTop_b  = 0x00 (0)
RGBNightBottom_r = 0x0B (11)   RGBNightBottom_g = 0x29 (41)  RGBNightBottom_b = 0x81 (129)

```

The engine interpolates between day and night colors based on the game clock's
time-of-day value. This creates dawn/dusk transitions.

### Configuration

```ini
[outdoor]
nosky = 0          ; Set to 1 to disable sky rendering entirely
nowavywater = 0    ; Set to 1 to disable water surface animation

```

---

## 7. Building Face Rendering

### Outdoor building model structure

Building models are stored at `DAT_006a0d48` with a count at `DAT_006a0d20`.

```cpp
Offset  Size    Field
+0x40   uint32  flags (bit 0 = rendered this frame)
+0x4C   int32   numFaces
+0x50   int32*  faceList pointer

```

### Outdoor::RenderBuildingFaces_SW (FUN_004789e2, 1697 bytes)

Renders outdoor 3D building models (non-terrain geometry such as houses,
walls, towers) in the software path.

**Algorithm for each building:**

1. Call visibility test (`FUN_0047908d`) to check if building is in view.
2. Iterate over faces (at building offset `+0x4C`, face count at
   `*building[0x4C]`).
3. Per-face processing:
   - Check face flags (bits in `piVar20[7]`) for portal, transparent, etc.
   - Look up texture from the texture frame table.
   - Transform vertices via `FUN_00436512` (world-to-view transform).
   - Clip against near/far planes.
   - Project to screen coordinates via `FUN_00436ba6`.
   - Collect face into render list with texture, lighting, and flag data.

The assertion string `"D3D version of RenderBuildings called in software!"`
(at 0x004ec088) fires if the D3D building renderer (`FUN_00478411`) is
accidentally invoked in software mode.

### Outdoor face structure

```cpp
Offset  Size    Field
+0x07   uint32  faceFlags
                  bit 0x04  = flow (water texture animation)
                  bit 0x10  = translucent
                  bit 0x20  = flow2 (secondary flow)
                  bit 0x40  = flow texture
                  bit 0x800 = no light
                  bit 0x2000 = invisible
+0x08   uint16* vertexIndices
+0x12   byte    orientationType (0x40 = animated)
+0x1E   byte    flags2 (0x40 = animated texture)

```

### D3D building rendering

**FUN_00478411** renders buildings using Direct3D. It uses the same face data
but submits triangles through the D3D device rather than rasterizing into the
span buffer.

### Outdoor brightness adjustment

```ini
[outdoor]
ter_gamma = 0    ; Terrain brightness adjustment
bld_gamma = 0    ; Building brightness adjustment

```

These gamma values are added to the base lighting for terrain tiles and
building faces respectively, allowing global brightness tuning.

---

## 8. Outdoor Span Rendering (Odbuild / Odspan)

### Active edge table

The software outdoor renderer uses an **active edge table (AET)** algorithm.
`Outdoor::ProcessTerrainSpans` (FUN_00486f96, 963 bytes) maintains sorted
edge lists:

- `DAT_0080c858` -- Primary edge list
- `DAT_0080c824` -- Secondary edge list

Edges are inserted into the AET using insertion sort, then processed
scanline-by-scanline to generate horizontal spans. Each span records:

- Start and end X coordinates
- Interpolated Z (depth)
- Interpolated texture U, V
- Lighting level

### Span compositor

**Outdoor::CompositeSpans** (FUN_0047bad3, 416 bytes) performs the final
compositing of all spans and billboards:

- **Software path:** Calls `FUN_004acb9b()` -- software sprite blitter.
- **D3D path:** Calls `FUN_004a3fb3()` -- D3D textured quad submission.

---

## 9. D3D Outdoor Rendering

### D3D terrain rendering

**FUN_00479547** renders the outdoor terrain using Direct3D. Instead of span
rasterization, it submits terrain tile triangles as D3D primitives with
appropriate texture and lighting state.

### D3D render state setup

The D3D outdoor path uses the same render state configuration as indoor
rendering (see [rendering-pipeline.md](rendering-pipeline.md) section 2), with additional states
for terrain-specific effects.

### Colored lights

The string `"Colored Lights"` (at 0x004e46b8) is referenced by both the main
outdoor render function (FUN_004304d6) and the D3D initialization
(FUN_0049e922), indicating support for colored lighting in the D3D path.

---

## 10. Outdoor Decorations and Objects

### Outdoor::RenderDecorations (FUN_0047a962, 1459 bytes)

Renders static decoration sprites (trees, rocks, signs, flames) in the
outdoor world. Iterates over `DAT_0069ac50` decorations stored at
`DAT_00683558`.

For each decoration:

1. Check visibility flags:
   - Bit 6 = hidden (do not render)
   - Bit 5 = no-draw (skip rendering)
2. Read decoration type from `DAT_0069ac58 + decorType * 0x54`.
3. Fire/light decorations create particle effects using the `effpar01`
   texture.
4. Standard decorations: compute billboard position and facing relative
   to the camera.

Decoration rendering can be globally disabled:

```ini
[render]
nodecorations = 0    ; Set to 1 to disable all decoration rendering

```

Decoration rendering is also controlled by `DAT_006bdf48`: when non-zero,
`Outdoor::RenderDecorations` is skipped entirely.

### Outdoor::RenderObjects (FUN_0047af15)

Renders additional outdoor objects that are not decorations (dropped items,
interactive objects, etc.).

---

## Key Function Reference

| Address | Size | Suggested Name | Description |
|---------|------|----------------|-------------|
| 0x004304d6 | 19,695 | `Outdoor::RenderMain` | Largest outdoor render function |
| 0x00441d22 | 57 | `RenderOutdoor` | Outdoor render entry point |
| 0x00478411 | -- | `Outdoor::RenderBuildings_D3D` | D3D building rendering |
| 0x00479547 | -- | `Outdoor::RenderTerrain_D3D` | D3D terrain rendering |
| 0x004789e2 | 1697 | `Outdoor::RenderBuildingFaces_SW` | SW building face rendering |
| 0x00486a2c | -- | `Outdoor::SpanDebug` | Span buffer debug output |
| 0x00486b52 | -- | `Outdoor::TextureFrameCheck` | Texture frame table validation |
| 0x00486f96 | 963 | `Outdoor::ProcessTerrainSpans` | Active edge table processing |
| 0x00487359 | -- | `Outdoor::RasterizeTerrainTriangle` | Terrain triangle fill |
| 0x00487dad | -- | `Outdoor::GetTerrainHeight` | Terrain height lookup |
| 0x00487dc2 | -- | `Outdoor::ComputeTerrainNormals` | Terrain normal computation |
| 0x004892cc | -- | `Outdoor::UpdateSkyTexture` | Sky texture change handler |
| 0x004893a7 | -- | `Outdoor::RenderSky` | Sky rendering |
| 0x0047a5a2 | 631 | `Outdoor::RenderScene` | Outdoor scene orchestrator |
| 0x0047a962 | 1459 | `Outdoor::RenderDecorations` | Outdoor decoration sprites |
| 0x0047af15 | -- | `Outdoor::RenderObjects` | Additional outdoor objects |
| 0x0047b430 | 1699 | `Outdoor::CollectBillboards` | Outdoor sprite collection |
| 0x0047bad3 | 416 | `Outdoor::CompositeSpans` | Final span/billboard composite |
| 0x0047f44f | -- | `Outdoor::WorldXToTileCol` | World X to tile column |
| 0x0047f45c | -- | `Outdoor::WorldYToTileRow` | World Y to tile row |
| 0x0047f5ca | -- | `Outdoor::PrepareTerrainData` | Shared terrain data setup |
| 0x00485f57 | -- | `Outdoor::FinalizeFrame` | End-of-frame outdoor cleanup |

---

## Integration notes

1. **Replace span rendering with GPU triangles.** The active-edge-table span
   renderer is a software optimization that is unnecessary with modern GPU
   rasterization. Terrain tiles should be submitted as triangle meshes.

2. **LOD bands remain useful.** The three-band LOD system provides a natural
   framework for terrain mesh simplification at distance. The band distances
   should be configurable.

3. **Sky rendering is straightforward.** The vertical gradient between day/night
   colors with time-of-day interpolation can be implemented as a simple
   full-screen shader pass.

4. **Building models are independent from terrain.** Buildings have their own
   face lists and BSP ordering, separate from the terrain grid. They should be
   rendered as distinct meshes.

5. **Decoration particle effects.** Fire and light decorations emit particles
   using the `effpar01` texture. This particle system must be reimplemented
   for visual fidelity.

6. **Mipmap distances define texture quality.** The configurable mipmap
   distances control when texture resolution decreases with distance. Modern
   hardware handles this automatically with anisotropic filtering, but the
   original distances define the expected visual behavior.
