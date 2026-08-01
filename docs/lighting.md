---
title: "Lighting System"
summary: "Lighting combines ambient levels, fixed and moving light sources, fog, and gamma control."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Lighting System

Lighting combines ambient levels, fixed and moving light sources, fog, and gamma control.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

MM7's lighting system combines **static ambient lighting** (per-sector for indoor,
global for outdoor) with **dynamic light sources** managed through two stack-based
systems: `MobileLightStack` for moving lights and `StationaryLightStack` for fixed
lights. Distance-based shading and fog further modulate outdoor lighting.

The `Light.cpp` source file (debug string at 0x004e935c) is referenced over 40 times
in the binary, making it one of the most heavily used source files in the engine.

**Original source files** (from debug strings):

- `Light.cpp` -- Dynamic lighting calculations
- `MobileLightStack.cpp` -- Moving light sources (torches, spells)
- `StationaryLightStack.cpp` -- Fixed light sources
- `GammaControl.cpp` -- Display gamma correction

---

## 1. Light Source Types

### MobileLightStack

**Source file:** `MobileLightStack.cpp` (debug string at 0x004ead28)

Mobile lights are dynamic light sources that move with entities. They are
used for:

- Torchlight carried by the party
- Spell effects (fireballs, lightning bolts, magical auras)
- Monster abilities (fire breath, glowing eyes)
- Temporary environmental effects

Mobile lights use a **stack-based allocation** model: lights are pushed onto
the stack when created and popped when they expire. This avoids dynamic memory
allocation during gameplay.

**Overflow protection:**
The error string `"Too many mobile lights!"` (at 0x004ead10, referenced by
FUN_00467d8c) is emitted when the mobile light stack exceeds its maximum
capacity.

### StationaryLightStack

**Source file:** `StationaryLightStack.cpp` (debug string at 0x004f0204)

Stationary lights are fixed light sources placed in the map data. They are
used for:

- Wall sconces and torches
- Campfires and braziers
- Magical light sources (enchanted objects, glowing crystals)
- Ambient area lighting

Like mobile lights, stationary lights use a stack-based allocation.

**Overflow protection:**
The error string `"Too many stationary lights!"` (at 0x004f01e8, referenced
by FUN_004ad32b) is emitted when the stationary light stack overflows.

---

## 2. Indoor Lighting

### Sector ambient lighting

Indoor levels use **per-sector ambient light** as the base illumination level.
Each sector defines an ambient RGB color that is applied uniformly to all
faces within it.

**Ambient light globals:**

| Global | Channel |
|--------|---------|
| `DAT_00ae3064` | Ambient Blue |
| `DAT_00ae3068` | Ambient Green |
| `DAT_00ae306c` | Ambient Red |

These values are set by `FUN_00467d8c()` (from `MobileLightStack.cpp`),
which combines the sector's base ambient light with a time-of-day modulation
factor.

### Per-face lighting

Each BLV face has a **light level byte** at offset `+0x4C` in the 96-byte
face structure. This value provides a per-face lighting override that can
make individual faces brighter or darker than the sector ambient.

Faces with the `noLight` attribute (bit 11 = 0x0800 in the face attributes
word at offset `+0x2C`) skip all dynamic lighting computations. These faces
are rendered at a fixed brightness, typically used for self-illuminated
surfaces like glowing runes, lava, or magical portals.

### Indoor light polygon building

The engine builds **light polygons** -- projected regions on faces that
define where dynamic lights illuminate. This is essentially a lightmap
computation performed at runtime.

Key functions in the light polygon pipeline:

| Address | Suggested Name | Description |
|---------|----------------|-------------|
| FUN_0045bc40 | `BuildLightPolygon` | Main light polygon builder |
| FUN_0045bebf | `ClipLightPolygon` | Light polygon clipping |
| FUN_0045cc45 | `ValidateLightType` | Light type validation |
| FUN_0045d10e | `ApplyLightToFace` | Apply light contribution to face |
| FUN_0045d788 | `ValidateLightmap` | Lightmap validation |
| FUN_0045da8f | `CheckLightmap` | Lightmap integrity check |
| FUN_0045db21 | `LightCalc` | Core lighting calculation |
| FUN_0045dce2 | `LightFinalize` | Finalize lighting pass |

**Error and warning strings:**

- `"Error: Failed to build light polygon"` (at 0x004e9308, FUN_0045bc40)
- `"Lightpoly builder native indoor clipping not implemented"` (at 0x004e9380,
  FUN_0045bebf and FUN_0049b719)
- `"Invalid light type!"` (at 0x004e93dc, FUN_0045bebf)
- `"Invalid light type detected!"` (at 0x004e93f0, FUN_0045cc45)
- `"Invalid lightmap detected!"` (at 0x004e942c, FUN_0045da8f and FUN_0045d788)

The `"Lightpoly builder native indoor clipping not implemented"` message
indicates that the indoor light polygon clipper has a software fallback path
that does not fully implement native clipping. This same string is also
referenced by the decal geometry builder (`FUN_0049b719` in
`PolyProjector.cpp`), suggesting shared clipping code between lighting and
decals.

---

## 3. Outdoor Lighting

### Distance-based shading

Outdoor lighting uses a **three-zone distance shading** system that
progressively fades geometry toward a fog/mist color as distance from the
camera increases.

**Shading distance thresholds** (from INI configuration):

```ini
[shading]
dist_shade     = 0x800   (2048)   ; Distance where shading begins
dist_shademist = 0x1000  (4096)   ; Distance where mist blending starts
dist_mist      = 0x2000  (8192)   ; Full mist/fog distance

```

These are referenced by `FUN_00466086` (the render configuration loader).

### Shading zones

| Zone | Distance range | Effect |
|------|---------------|--------|
| **Near** | 0 -- 2048 | Full brightness, no distance attenuation |
| **Shade** | 2048 -- 4096 | Linear brightness falloff begins |
| **Shade+Mist** | 4096 -- 8192 | Brightness continues falling, fog color blending in |
| **Full mist** | > 8192 | Geometry fully fogged (or not rendered) |

The shading calculation is applied per-vertex or per-span during terrain and
building rendering. The interpolation between zones is linear.

### Mist/fog debug override

```ini
[debug]
noMist = 0    ; Set to 1 to disable distance fog entirely

```

The string `"noMist"` (at 0x004ea1b0, referenced by FUN_00466086) controls
this debug flag.

### Outdoor brightness adjustment

```ini
[outdoor]
ter_gamma = 0    ; Terrain brightness adjustment (additive)
bld_gamma = 0    ; Building brightness adjustment (additive)

```

These gamma values (at 0x004ea148 and 0x004ea154, referenced by FUN_00466086)
are added to the base lighting for terrain tiles and building faces
respectively, providing a global brightness offset.

---

## 4. Sky and Time-of-Day Lighting

### Day/Night sky colors

The outdoor sky color defines the ambient lighting tone for the entire
outdoor scene. The sky gradient colors are interpolated based on time of day:

**Day colors:**

```text
Top:    R=0x51 (81),  G=0x79 (121), B=0xEC (236)   -- bright blue
Bottom: R=0x99 (153), G=0xC1 (193), B=0xED (237)   -- lighter blue

```

**Night colors:**

```text
Top:    R=0x00 (0),   G=0x00 (0),   B=0x00 (0)     -- black
Bottom: R=0x0B (11),  G=0x29 (41),  B=0x81 (129)   -- dark blue

```

The engine interpolates between these two color sets based on the game clock,
creating smooth dawn and dusk transitions that affect:

- Sky gradient rendering
- Ambient light contribution to outdoor faces
- Fog/mist color blending

---

## 5. D3D Lighting Render States

### PolyProjector::SetD3DRenderStates (FUN_0049c28d, 588 bytes)

When using the D3D hardware path, specific render states control how lighting
interacts with geometry. The following states are set for the decal/effect
rendering pass:

| Render state code | Value | D3D constant | Meaning |
|------------------|-------|-------------|---------|
| `0x0C` | 3 | `D3DRENDERSTATE_FILLMODE` | `D3DFILL_SOLID` |
| `0x1B` | 1 | `D3DRENDERSTATE_CULLMODE` | `D3DCULL_CW` |
| `0x0E` | 0 | `D3DRENDERSTATE_ZWRITEENABLE` | Disabled (for decals) |
| `0x16` | 1 | `D3DRENDERSTATE_ALPHABLENDENABLE` | Enabled |
| `0x13` | 2 | `D3DRENDERSTATE_SRCBLEND` | `D3DBLEND_SRCALPHA` |
| `0x14` | 2 | `D3DRENDERSTATE_DESTBLEND` | `D3DBLEND_INVSRCALPHA` |
| `0x1A` | 0 | `D3DRENDERSTATE_ZENABLE` | Disabled |

This configuration is designed for rendering **decals and light overlays**:

- Z-write is disabled so decals do not modify the depth buffer.
- Alpha blending is enabled with source-alpha / inverse-source-alpha for
  proper transparency.
- Z-testing is disabled for overlays that should always be visible.

### Colored lights

The string `"Colored Lights"` (at 0x004e46b8) is referenced by both the main
outdoor render function (FUN_004304d6) and the D3D initialization
(FUN_0049e922). This confirms that the D3D path supports colored (non-white)
dynamic lighting, which the software renderer may handle through palette
manipulation.

---

## 6. Light Color Blending

### Software path blending

In the software renderer, lighting is applied by modifying the color of each
pixel during rasterization. The blending uses the half-intensity mask:

```text
blendMask = 0x7BEF  (RGB565)
blendMask = 0x3DEF  (RGB555)

```

The basic 50% blend formula:

```bash
result = (color1 >> 1 & blendMask) + (color2 >> 1 & blendMask)

```

This produces a simple averaging of two colors in 16-bit pixel space. It is
used for:

- Translucent sprite blending (50% opacity)
- Light overlay compositing
- Distance fog blending (mix scene color with fog color)

### D3D path blending

The D3D path uses standard Direct3D alpha blending with `SrcAlpha` /
`InvSrcAlpha` blend modes. Light contributions are applied through texture
stage operations or vertex color modulation.

---

## 7. Elemental Light Effects

Special elemental lighting effects are used for spell visuals:

- `"Elemental Light A"` (at 0x004e8218, FUN_0044fa78)
- `"Elemental Light B"` (at 0x004e822c, FUN_0044fa78)
- `"Elemental Light C"` (at 0x004e8240, FUN_0044fa78)

These correspond to different elemental types (fire, water/ice,
earth/lightning) and produce colored dynamic lights during spell casting
and effect application.

The generic `"light"` and `"Light"` strings (at 0x004e842c and 0x004e876c)
appear in item/spell type parsing, while `"LIGHT"` (at 0x004e87f8) appears
in uppercase comparison contexts. `"Lightning"` (at 0x004e87a8) refers to
the lightning damage/spell type.

---

## 8. Particle-Based Lighting

### Effect particle textures

Dynamic lights often accompany particle effects. The effect particle textures
serve as both visual particles and light source indicators:

| Texture name | Address | Primary use |
|-------------|---------|-------------|
| `effpar01` | 0x004e5d28 | Fire, sparks, light flares |
| `effpar02` | 0x004efc70 | Secondary effects |
| `effpar03` | 0x004e92fc | Tertiary effects, spell impacts |

These textures are referenced by multiple functions across both indoor and
outdoor rendering, indicating they are shared resources for all particle-based
lighting effects.

---

## 9. Gamma Control

**Source file:** `GammaControl.cpp` (debug string at 0x004e80cc)

The gamma control system adjusts the overall display brightness through
DirectDraw gamma ramp manipulation.

| Address | Suggested Name | Description |
|---------|----------------|-------------|
| FUN_0044f2de | `GammaControl::Init` | Initialize gamma control |
| FUN_0044f350 | `GammaControl::GetSet` | Get or set gamma value |
| FUN_0044f434 | `GammaControl::Apply` | Apply gamma ramp to display |

**Error string:** `"Gamma control not active"` (at 0x004e80f4) is emitted when
gamma adjustment is attempted on a display device that does not support
hardware gamma ramps.

**Configuration:**

- `gamma.pcx` -- UI element for gamma adjustment slider (at 0x004e4658)
- `GammaPos` -- Stored gamma position value (at 0x004e4698, referenced by
  both FUN_004304d6 and FUN_00465245)

---

## 10. Decal Lighting Interaction

Decals (blood splats, spell impact marks, footprints) interact with the
lighting system through the shared light polygon clipper. The function
`FUN_0049b719` (`PolyProjector::BuildDecalGeometry`, 1069 bytes) builds
projected decal polygons onto existing faces.

Decals are rendered with:

- Z-write disabled (they sit on top of existing geometry)
- Alpha blending enabled (for partial transparency)
- Z-test disabled or bias-adjusted (to prevent z-fighting)

Each decal is `0x28` (40 bytes) with position, UV coordinates, and target
face data. The decal list is stored at `local_c + 0x30c00c`.

The string `"Error: Failed to build decal geometry"` (at 0x004ee950,
referenced by FUN_0049b4c9) is the error for decal construction failures.

---

## Key Function Reference

| Address | Size | Suggested Name | Description |
|---------|------|----------------|-------------|
| 0x0044f2de | -- | `GammaControl::Init` | Initialize gamma control |
| 0x0044f350 | -- | `GammaControl::GetSet` | Get/set gamma value |
| 0x0044f434 | -- | `GammaControl::Apply` | Apply gamma ramp |
| 0x0044fa78 | -- | `ElementalLightEffect` | Elemental light A/B/C |
| 0x0045bc40 | -- | `Light::BuildLightPolygon` | Build light polygon |
| 0x0045bebf | -- | `Light::ClipLightPolygon` | Clip light polygon (fallback) |
| 0x0045cc45 | -- | `Light::ValidateLightType` | Validate light source type |
| 0x0045d10e | -- | `Light::ApplyToFace` | Apply light to face |
| 0x0045d788 | -- | `Light::ValidateLightmap` | Validate lightmap data |
| 0x0045da8f | -- | `Light::CheckLightmap` | Lightmap integrity check |
| 0x0045db21 | -- | `Light::Calculate` | Core lighting calculation |
| 0x0045dce2 | -- | `Light::Finalize` | Finalize lighting pass |
| 0x00466086 | -- | `LoadRenderConfig` | Load INI render settings |
| 0x00467d8c | -- | `MobileLightStack::SetAmbient` | Set ambient + add mobile light |
| 0x004ad32b | -- | `StationaryLightStack::Push` | Push stationary light |
| 0x0049b4c9 | -- | `BuildDecalGeometry` | Decal geometry builder (entry) |
| 0x0049b719 | 1069 | `PolyProjector::BuildDecalGeometry` | Decal polygon projection |
| 0x0049c28d | 588 | `PolyProjector::SetD3DRenderStates` | D3D light/decal states |

---

## Integration notes

1. **Separate ambient from dynamic lighting.** Indoor sectors have per-sector
   ambient RGB that forms the base illumination; dynamic lights are additive.
   This two-layer model should be preserved.

2. **Stack-based light allocation is efficient.** The original stack model
   avoids heap allocation. The reimplementation can use a fixed-capacity
   array or ring buffer for dynamic lights.

3. **Distance shading is critical for outdoor visuals.** The three-zone
   distance fog system (shade at 2048, shade+mist at 4096, full mist at 8192)
   defines the outdoor visual character. These distances should be
   configurable and respected.

4. **Light polygon building is the indoor lighting core.** The engine projects
   dynamic light influence onto faces as polygons. Modern implementations can
   use deferred lighting or lightmaps, but the per-face light level byte and
   sector ambient must still be honored.

5. **Gamma control needs a modern equivalent.** DirectDraw gamma ramps are
   replaced by shader-based post-processing in modern engines.

6. **Colored lights are D3D-only in the original.** The software renderer
   likely approximates colored lights through palette manipulation. The
   reimplementation should support colored lights uniformly.

7. **Time-of-day modulation affects both sky and ambient.** The day/night
   color interpolation changes the overall scene mood and must be
   synchronized with the game clock system (see [time-calendar.md](time-calendar.md)).
