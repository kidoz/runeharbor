---
title: "Sprite and Billboard Rendering"
summary: "Sprite frames become camera-facing billboards selected by animation and view direction."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Sprite and Billboard Rendering

Sprite frames become camera-facing billboards selected by animation and view direction.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

Sprites in MM7 are 2D images that represent monsters, items, decorations, and
spell effects in the 3D world. They are rendered as **billboards** -- camera-facing
quads that always face the viewer. The engine uses an **8-directional** sprite
system where each sprite has up to 8 facing variants, with the appropriate variant
selected based on the angle between the sprite and the camera.

Sprites are organized through three layers:

1. **Sprite Frame Table** -- defines animation sequences and frame timing.
2. **Billboard entries** -- screen-space rendering data for visible sprites.
3. **Particle system** -- lightweight effects (fire, magic, sparks).

**Original source files** (from debug strings):

- `CSpriteFrameTable` -- Sprite animation frame definitions
- `Vis.cpp` -- Visibility / picking (z-buffer selection for sprites)
- `seffects.cpp` -- Spell visual effects

---

## 1. Sprite Frame Table

### CSpriteFrameTable::Load (referenced at ~address 55800)

Loads sprite animation frame definitions from a text file. The loading
function allocates three parallel arrays:

| Array name | Entry size | Purpose |
|-----------|-----------|---------|
| `"S_Frames"` | 60 bytes (0x3C) per entry | Sprite frame data |
| `"E_Frames"` | 2 bytes per entry | End frame indices |
| `"P_Frames"` | 4 bytes per entry | Frame pointer table |

Error strings:

- `"CSpriteFrameTable::load - Out of Memory!"` (at 0x004e7e98)
- `"CSpriteFrameTable::load - Unable to open file: %s."` (at 0x004e7ec4)

### Sprite frame structure (0x3C = 60 bytes)

Each sprite frame entry is 60 bytes and contains animation timing data,
texture references, and display flags. The binary format corresponds to
the `dsft.bin` data table (or its text equivalent `sft.txt` in developer
mode).

### 8-directional facing system

Sprites have up to 8 facing variants representing 45-degree increments around
the full 360-degree circle:

```text
Direction 0: Front (facing camera)
Direction 1: Front-right (45 degrees)
Direction 2: Right (90 degrees)
Direction 3: Back-right (135 degrees)
Direction 4: Back (facing away)
Direction 5: Back-left (225 degrees)
Direction 6: Left (270 degrees)
Direction 7: Front-left (315 degrees)

```

The facing direction is computed by `FUN_0045284a(deltaX, deltaY)`, which
calculates the angle between the sprite's world position and the camera.
The frame selection formula is:

```text
directionIndex = (facing + timeOffset) >> 8 & 7

```

This selects one of 8 directional variants based on the relative angle,
with `timeOffset` allowing smooth rotation animation over time.

### LOD sprite archives

Sprite pixel data is stored in LOD archives:

- `data\sprites.lod` -- Main sprite data (full resolution)
- `data\spriteLO.lod` -- Low-detail sprite data (reduced resolution)

---

## 2. Billboard Structure

### Billboard render entry (0x34 = 52 bytes)

Billboards are the screen-space representations of sprites. They are stored
in a render list at `DAT_005120C0` with a count at `DAT_00518660`.

```text
Offset  Size    Field
0x00    int16   screenXLeft
0x02    int16   screenXRight (subtract from left to get width)
0x04    int16   screenYTop
0x06    int16   screenYBottom
0x08    int16   depthOrZ (view-space depth for sorting)
0x0A    uint16  flags
                  bit 0x02 = translucent
                  bit 0x40 = mirror / shimmer effect
                  bit 0x80 = oscillate effect
0x0C    int32   texturePtr or palettePtr
0x18    int32   spriteFrameData (frame table reference)
0x22    int16   referenceSpriteIndex
0x2C    int16   facingDirection

```

### Billboard world position

The billboard's world position is stored at negative offsets from the
per-entry base pointer (a pattern used throughout the engine):

```text
Offset -0x14   int32   worldX
Offset -0x12   int32   worldY
Offset -0x0C   --      texture/palette data
Offset -0x08   --      spriteFrameIndex
Offset -0x05   byte    flags (0x02=translucent, 0x40=mirror, 0x80=oscillate)
Offset -0x01   --      screen Y top
Offset +0x00   --      screen Y bottom
Offset +0x01   --      screen height
Offset +0x02   --      texture handle

```

---

## 3. Billboard Collection

### Outdoor::CollectAndRenderBillboards (FUN_0047b430, 1699 bytes)

Processes all outdoor sprites (monsters, items, decorations) and converts
them to screen-space billboards.

**Algorithm for each sprite:**

1. Get the sprite's world position (X, Y, Z at offsets `psVar20[-2], [-1], [0]`
   from the sprite entry at `DAT_005ff06a`, stride 0x34).
2. Compute facing direction relative to camera:
   `FUN_0045284a(deltaX, deltaY)`.
3. Select animation frame from the 8-direction frame set:
   `directionIndex = (facing + timeOffset) >> 8 & 7`.
4. Get sprite frame data from the frame table.
5. Handle special monster states:
   - **Dead:** Face sprite upward (flat on ground).
   - **Flying:** Add altitude offset above base Z.
6. Compute screen-space position and size using perspective projection.
7. Store billboard data in the render list at `DAT_00518660`.

The sprite count is stored at `DAT_006650a8` for outdoor sprites.

### Indoor::CollectBillboards (FUN_004402b2, 938 bytes)

Same process as outdoor billboard collection but for indoor sprites. Uses
`DAT_006650ac` as the sprite count. Handles indoor-specific animation timings
and sector-based visibility.

### Indoor::CollectMonsterBillboards (FUN_0043fe10, 1186 bytes)

Gathers monster sprites specifically from visible indoor sectors. For each
monster:

1. Check if the monster is in a visible sector.
2. Compute screen-space position.
3. Select appropriate animation frame.
4. Add to the billboard render list.

### Billboard depth assignment

`FUN_0043f538` (indoor) assigns final depth values to all collected
billboards after BSP traversal. This ensures correct depth sorting during
compositing.

---

## 4. Billboard Effects

### Translucent sprites (flag 0x02)

When the translucent flag is set, the sprite is blended with the background
using the 50% alpha blending formula:

```text
pixel = (src >> 1 & blendMask) + (dst >> 1 & blendMask)

```

Where `blendMask` is `0x7BEF` (RGB565) or `0x3DEF` (RGB555). This produces
a 50% transparent appearance used for ghosts, magical effects, and
semi-transparent objects.

### Mirror / shimmer effect (flag 0x40)

When the mirror flag is set, the sprite undergoes palette cycling based on
the system tick count. This creates a shimmering, reflective appearance used
for water reflections, magical auras, and crystalline surfaces.

The effect modulates the sprite's color palette indices on a per-frame basis,
producing a dynamic visual shimmer.

### Oscillate effect (flag 0x80)

When the oscillate flag is set, the sprite's screen position receives a small
random offset each frame. This creates a bobbing or vibrating appearance
used for floating objects, hovering creatures, and unstable magical effects.

---

## 5. Billboard Compositing

### ProcessBillboards (FUN_0047a819, 16 bytes)

A thin dispatcher that calls `FUN_0048abd9` for the actual billboard
processing and sorting.

### Outdoor::CompositeSpans (FUN_0047bad3, 416 bytes)

Composites all collected billboards into the final framebuffer or D3D scene.
Dispatches based on the renderer mode:

- **Software path:** `FUN_004acb9b()` -- software sprite blitter. Draws
  sprite pixels directly into the back buffer with chroma-key transparency
  and optional blending effects.

- **D3D path:** `FUN_004a3fb3()` -- D3D textured quad submission. Creates
  a camera-facing textured quad and submits it through the D3D device with
  appropriate render states (alpha blending, z-testing).

### Z-sorting

Billboards are depth-sorted before compositing to ensure correct back-to-front
rendering order. The `depthOrZ` field at offset `+0x08` of each billboard
entry stores the view-space depth used for sorting.

For the software renderer, billboards are sorted by depth and drawn
back-to-front (painter's algorithm). For the D3D path, the hardware Z-buffer
handles depth testing, but billboards are still submitted in approximate
front-to-back order for early-Z optimization.

---

## 6. D3D Sprite Hardware Support

### Hardware sprite files

The D3D path uses pre-processed sprite textures:

- `data\d3dsprite.hwl` -- Hardware-accelerated sprite texture lookup
  (loaded at init by FUN_0049e922).

### Hardware sprite pools

Debug strings reveal the sprite allocation pools:

- `"sprites08"` (at 0x004f01d0) -- 8-bit sprite reference format.
- `"hardSprites"` (at 0x004f01dc) -- Hardware sprite allocation pool
  (maximum capacity for pre-loaded sprite textures).

### D3D texture loading for sprites

`"HiScreen16::LoadTexture - D3Drend->CreateTexture() failed: %x"` (at
0x004efc04) is the error string when D3D texture creation fails for sprite
data. Referenced by FUN_004a4d71 and FUN_004a4fd8.

### D3D texture name logging

`"D3D texture name: %s offset: %x\n"` (at 0x004e8314, referenced by
FUN_0045274b) is used for debug logging of D3D texture loading.

---

## 7. Outdoor Decorations

### Outdoor::RenderDecorations (FUN_0047a962, 1459 bytes)

Decorations are static sprite objects placed in the outdoor world (trees,
rocks, signs, torches, fountains).

**Decoration table:**

- Count: `DAT_0069ac50`
- Array base: `DAT_00683558`
- Type table: `DAT_0069ac58` with stride `0x54` (84 bytes per decoration type)

**Per-decoration processing:**

1. Check visibility flags:
   - Bit 6 = hidden (do not render).
   - Bit 5 = no-draw (skip rendering).
2. Read decoration type from the type table.
3. **Fire/light decorations:** Create particle effects using the `effpar01`
   texture (at 0x004e5d28). Additional effect textures include `effpar02`
   (at 0x004efc70) and `effpar03` (at 0x004e92fc).
4. **Standard decorations:** Compute billboard position and facing direction
   relative to the camera, then add to the billboard render list.

---

## 8. Particle System

### Particle effects update

**UpdateParticles** (FUN_00440f2a, 85 bytes) is called once per frame after
all 3D geometry and sprites have been rendered. It updates all active
particle emitters and draws their particles.

### Particle textures

Particle effects use dedicated textures:

- `effpar01` -- Primary effect particle (fire, sparks, light flares).
  Referenced by FUN_0047a962 (decorations), FUN_0043fa56 (indoor faces),
  FUN_00471c07, FUN_004a9030, FUN_00471370, FUN_004a7618.
- `effpar02` -- Secondary effect particle. Referenced by FUN_004a7e19,
  FUN_004a9030.
- `effpar03` -- Tertiary effect particle. Referenced by FUN_00471c07,
  FUN_004a9030, FUN_00471370, FUN_0045baef.

### Particle frame table

Particle animation frames are loaded from `dpft.bin` (binary) or `pft.def`
(text) via `FUN_00494c25` / `FUN_00494c83`. This is the Particle Frame Table,
analogous to the Sprite Frame Table but for particle effects.

### Spell visual effects

Source file `seffects.cpp` handles spell-specific visual effects, which
include particles, screen flashes, and projectile sprites.

The `"Elemental Light A/B/C"` strings (at 0x004e8218/822c/8240, referenced
by FUN_0044fa78) relate to elemental spell lighting effects.

---

## 9. Sprite Outline

The string `"Sprite outline currently Unsupported"` (at 0x004e805c,
referenced by FUN_0044eb86) indicates that the sprite outline feature
(used for mouse-over highlighting) has an incomplete or disabled
implementation path.

---

## Key Function Reference

| Address | Size | Suggested Name | Description |
|---------|------|----------------|-------------|
| 0x0043f538 | -- | `Indoor::AssignBillboardDepth` | Billboard depth assignment |
| 0x0043fa56 | 954 | `Indoor::ProcessFace` | Indoor face + decoration particles |
| 0x0043fe10 | 1186 | `Indoor::CollectMonsterBillboards` | Indoor monster sprites |
| 0x004402b2 | 938 | `Indoor::CollectBillboards` | Indoor item/decoration sprites |
| 0x0044dabe | -- | `CSpriteFrameTable::Load` | Sprite frame table loading |
| 0x0044eb86 | -- | `SpriteOutline` | Sprite outline (unsupported) |
| 0x0044fa78 | -- | `ElementalLightEffect` | Elemental light A/B/C effects |
| 0x0045274b | -- | `LogD3DTextureName` | D3D texture name debug logging |
| 0x0045284a | -- | `ComputeFacingAngle` | Angle between sprite and camera |
| 0x0047a819 | 16 | `ProcessBillboards` | Billboard processing dispatch |
| 0x0047a962 | 1459 | `Outdoor::RenderDecorations` | Outdoor decoration sprites |
| 0x0047b430 | 1699 | `Outdoor::CollectBillboards` | Outdoor sprite collection |
| 0x0047bad3 | 416 | `Outdoor::CompositeSpans` | Final billboard compositing |
| 0x004a3fb3 | -- | `D3D::SubmitBillboardQuad` | D3D textured quad submission |
| 0x004a4d71 | -- | `HiScreen16::LoadTexture` | D3D sprite texture loading |
| 0x004acb9b | -- | `SW::BlitSprite` | Software sprite blitter |
| 0x004ac6f8 | -- | `LoadSprites08` | 8-bit sprite loader |
| 0x004ac723 | -- | `AllocHardSprites` | Hardware sprite pool allocation |
| 0x0048abd9 | -- | `SortAndProcessBillboards` | Billboard sorting/processing |

---

## Integration notes

1. **8-directional sprites are core to visual fidelity.** The facing
   computation and frame selection must be reimplemented exactly to match
   the original appearance of monsters and NPCs.

2. **Billboard effects are simple but important.** The three effect flags
   (translucent, mirror, oscillate) define the visual character of many
   game elements. The translucent blending is a simple 50% mix; the mirror
   effect uses palette cycling; oscillation adds random position jitter.

3. **Depth sorting is required.** Even with modern Z-buffering, billboards
   with translucent effects must be sorted back-to-front for correct
   alpha blending.

4. **Particle textures are shared.** The three `effpar` textures are used
   extensively across decorations, spells, and indoor/outdoor effects. They
   should be loaded once and shared.

5. **Sprite LOD.** The dual LOD archive system (`sprites.lod` and
   `spriteLO.lod`) provides low-resolution fallbacks. The reimplementation
   should support this for consistency, even if modern hardware can handle
   full-resolution sprites at all distances.

6. **The Sprite Frame Table is loaded from data files.** The `dsft.bin` /
   `sft.txt` format defines all animation sequences and must be parsed
   correctly.
