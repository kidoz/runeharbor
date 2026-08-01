---
title: "INI Configuration System"
summary: "The configuration system reads and persists display, audio, input, and debug settings."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# INI Configuration System

The configuration system reads and persists display, audio, input, and debug settings.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

The engine reads configuration from `mm6.ini` (note: MM7 reuses the MM6 INI filename
for backward compatibility). The INI path is constructed as `<module-dir>\mm6.ini`
using the format string `"%s\mm6.ini"` (at `004e9fe4`). Settings are also
persisted back to the INI on exit.

**INI reader function:** `FUN_00466086` (ReadINISettings)
**INI writer function:** `FUN_00464637` (at game cleanup, writes window position
and debug flags)

The engine uses the Win32 `GetPrivateProfileInt` / `GetPrivateProfileString` /
`WritePrivateProfileString` APIs for INI access.

---

## 1. [settings] Section

Core application settings read during early initialization at `FUN_00465245`:

| Key              | Type | Default | Description                         |
|------------------|------|---------|-------------------------------------|
| `use_cd`         | i32  | 1       | Enable CD-ROM access for assets     |
| `registry`       | i32  | 1       | Use Windows registry for settings   |
| `resolution`     | i32  | 0       | 0=640x480, 1=low-resolution mode    |
| `mixerchannels`  | i32  | 16      | Audio mixer channels (range 0-16)   |
| `nointro`        | i32  | 0       | Skip intro video sequences          |
| `nosound`        | i32  | 0       | Disable all sound output            |
| `nowalksound`    | i32  | 0       | Disable footstep/walking sounds     |
| `nologo`         | i32  | 0       | Skip logo splash videos             |

### Engine Flags Mapping

Several `[settings]` keys map to bits in `DAT_006be1e4` (engine flags bitfield):

| INI Key       | Bit  | Hex    | Effect                          |
|---------------|------|--------|---------------------------------|
| `nointro`     | 2    | 0x04   | No intro videos                 |
| `nologo`      | 3    | 0x08   | No logo videos                  |
| `nosound`     | 4    | 0x10   | No sound                        |
| `nowalksound` | 5    | 0x20   | No walk sound                   |

### Resolution and Display

- `resolution=0` sets 640x480 display mode (standard)
- `resolution=1` selects a lower-resolution sprite LOD (`data\spriteLO.lod`)
- The `use_cd` flag (`DAT_006be1ee`) enables CD-ROM fallback for missing assets
- The `registry` flag (`DAT_006be1ed`) toggles Windows registry usage

### Gamma Position

| Key         | Type | Default | Description                      |
|-------------|------|---------|----------------------------------|
| `GammaPos`  | i32  | 4       | Gamma correction slider position |

Read at `FUN_00465245` and `FUN_004304d6`. Used with `gamma.pcx` dialog.

---

## 2. [screen] Section

Display viewport configuration:

| Key        | Type | Default   | Description                       |
|------------|------|-----------|-----------------------------------|
| (param 1)  | i32  | 8         | Viewport X offset from window     |
| (param 2)  | i32  | 8         | Viewport Y offset from window     |
| (param 3)  | i32  | 468 (0x1D4) | Viewport width                 |
| (param 4)  | i32  | 351 (0x15F) | Viewport height                |

The `[screen]` section (at `004ea31c`) also stores window position, written on exit:

| Key        | Type | Default | Description                        |
|------------|------|---------|------------------------------------|
| `window X` | i32  | 0       | Window X position (saved on exit)  |
| `window Y` | i32  | 0       | Window Y position (saved on exit)  |

The viewport defines the 3D rendering area within the 640x480 window. The remaining
border area displays the HUD, minimap, and party portraits.

---

## 3. [outdoor] Section

Outdoor rendering and environment settings, read in `FUN_00466086`:

### Sky Colors (RGB triplets)

| Key                | Type | Default | Description                |
|--------------------|------|---------|----------------------------|
| `RGBDayTop.r`      | i32  | 81      | Day sky top - red          |
| `RGBDayTop.g`      | i32  | 121     | Day sky top - green        |
| `RGBDayTop.b`      | i32  | 236     | Day sky top - blue         |
| `RGBDayBottom.r`   | i32  | 153     | Day sky bottom - red       |
| `RGBDayBottom.g`   | i32  | 193     | Day sky bottom - green     |
| `RGBDayBottom.b`   | i32  | 237     | Day sky bottom - blue      |
| `RGBNightTop.r`    | i32  | 0       | Night sky top - red        |
| `RGBNightTop.g`    | i32  | 0       | Night sky top - green      |
| `RGBNightTop.b`    | i32  | 0       | Night sky top - blue       |
| `RGBNightBottom.r`  | i32  | 11      | Night sky bottom - red     |
| `RGBNightBottom.g`  | i32  | 41      | Night sky bottom - green   |
| `RGBNightBottom.b`  | i32  | 129     | Night sky bottom - blue    |

Sky colors are stored at `DAT_006bdf88` through `DAT_006bdf93` (6 bytes for day,
6 bytes for night = 12 bytes total). The renderer interpolates between day and night
values based on the game clock.

### Terrain Parameters

| Key                  | Type | Default    | Description                    |
|----------------------|------|------------|--------------------------------|
| `nosky`              | i32  | 0          | Disable sky rendering          |
| `nowavywater`        | i32  | 0          | Disable water wave animation   |
| `gridband1`          | i32  | 10         | Near terrain LOD distance      |
| `gridband2`          | i32  | 15         | Medium terrain LOD distance    |
| `gridband3`          | i32  | 25         | Far terrain LOD distance       |
| `ter_gamma`          | i32  | 0          | Terrain gamma correction       |
| `bld_gamma`          | i32  | 0          | Building gamma correction      |
| `startmap`           | str  | `out01.odm`| Starting map filename          |
| `terrain_subdivpow2` | i32  | (varies)   | Terrain subdivision power of 2 |
| `terrain_subdivsize` | i32  | (varies)   | Terrain subdivision cell size  |

Grid bands control the terrain Level of Detail:

- `gridband1`: Closest band (highest detail)
- `gridband2`: Middle band (reduced detail)
- `gridband3`: Farthest band (lowest detail, fog fade)

The `startmap` key (at `004ea288`) sets the initial map loaded on new game. Stored at
`DAT_00510604`.

---

## 4. [render] Section

Rendering options:

| Key              | Type | Default | Description                       |
|------------------|------|---------|-----------------------------------|
| `nodecorations`  | i32  | 0       | Disable decoration rendering      |

The `[render]` section (at `004ea190`) currently has only the decoration toggle.
Render mode selection (software vs. hardware) is determined at runtime by
DirectDraw/Direct3D capability detection rather than INI configuration.

### Render Mode Detection

The renderer is selected based on hardware capabilities:

- If Direct3D device is available: hardware rendering via D3D
- Otherwise: software rendering fallback
- String reference: `"draw_debug_line() not implemented for SW rendering"`
  (at `004e48d0`)

---

## 5. [debug] Section

Debug and development settings, read in `FUN_00466086`:

| Key              | Type | Default | Description                       |
|------------------|------|---------|-----------------------------------|
| `startinwindow`  | i32  | 0       | Start in windowed mode            |
| `showFR`         | i32  | 0       | Show frame rate overlay           |
| `nomonster`      | i32  | 0       | Disable all monster spawning      |
| `nodamage`       | i32  | 0       | Disable damage to party           |
| `nodecoration`   | i32  | 0       | Disable decoration rendering      |
| `walkspeed`      | i32  | 384     | Party movement speed (0x180)      |
| `noMist`         | i32  | 0       | Disable mist/fog effects          |

### Debug Flags Bitfield (DAT_006be1e8)

Several `[debug]` keys map to bits in the debug flags global:

| Bit | Hex    | INI Key          | Effect                   |
|-----|--------|------------------|--------------------------|
| 0   | 0x01   | `startinwindow`  | Windowed mode            |
| 1   | 0x02   | `showFR`         | Frame rate display       |
| 2   | 0x04   | `nomonster`      | No monsters              |
| 3   | 0x08   | `nodecoration`   | No decorations           |
| 4   | 0x10   | `nodamage`       | No damage                |

The string `"debug flags"` (at `004e99b0`) is used when reading/writing this
bitfield to the INI.

### Walk Speed

Default walk speed is 384 (0x180). The `walkspeed` key (at `004ea294`) allows
overriding for debugging or accessibility.

---

## 6. INI Read/Write Flow

### Read (Initialization)

1. `FUN_00465245` reads `[settings]` during early startup
2. `FUN_00466086` reads `[outdoor]`, `[render]`, `[debug]` after LOD init
3. `[screen]` values are read for viewport setup

### Write (Shutdown)

1. `FUN_00464637` writes current state back to INI on exit:
   - `window X`, `window Y` (current window position)
   - `debug flags` (current debug bitfield)
   - `startinwindow` (windowed mode state)
   - `GammaPos` (gamma slider position)

---

## 7. Command-Line Overrides

Some INI settings can be overridden via command-line arguments processed in
`FUN_00462cd1`:

| Argument     | Effect                                 |
|--------------|----------------------------------------|
| `-window`    | Force windowed mode (overrides INI)    |
| `-nosound`   | Disable sound (sets engine flag 0x10)  |
| `-noanim`    | Disable animations (sets flag 0x40)    |
| `-usedefs`   | Use text data files instead of binary  |

---

## Integration notes

- The file is named `mm6.ini` (historical artifact); RuneHarbor should use a modern
  config format (TOML, INI, or JSON) while supporting import from mm6.ini
- `GetPrivateProfileInt` / `WritePrivateProfileString` are Win32 APIs; use a
  cross-platform INI parser instead
- Sky colors should be configurable per-map (not just globally)
- The debug flags bitfield is a compact way to toggle development features
- Walk speed as a configurable value enables accessibility tuning
- Grid band distances affect rendering performance on the terrain LOD system
- The `startmap` setting is useful for development/testing (jump to any map)
- Consider making `mixerchannels` dynamic rather than startup-only
- The `resolution` flag controls sprite LOD selection, not actual display resolution
