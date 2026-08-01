---
title: "Source File Map"
summary: "This map associates embedded source-path strings with functions, subsystems, and documentation."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Source File Map

This map associates embedded source-path strings with functions, subsystems, and documentation.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](../contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

> Mapping original MM7 source files to functions and documentation

## Overview

MM7 was compiled with MSVC 6.0 from approximately 32 source files under
`D:\mm7Src_eng\MM7\Code\`. Debug assert strings and other diagnostic references
embedded in the binary reveal these original file names. This document maps each
identified source file to the functions that reference it, the engine subsystem
it belongs to, and the corresponding RuneHarbor documentation.

The binary contains approximately 2,300 functions total. Of these, roughly 100
functions across 32 source files can be positively attributed via embedded debug
strings. Another 1,100 functions have been classified by subsystem through
behavioral analysis, while approximately 1,200 remain unclassified.

**Data sources:**

- `tools/ghidra/output/enhanced/strings_with_refs.tsv` -- string-to-function cross-references
- `tools/ghidra/output/enhanced/functions_enhanced.tsv` -- enhanced function metadata with referenced strings
- `tools/ghidra/output/functions/_classification.json` -- subsystem classification

---

## Source Files

### Core / Architecture

#### Game.cpp

- **System**: Core game logic
- **Documentation**: [architecture.md](../architecture.md)
- **String address**: `0x004E7FD8`
- **Function count**: 2 (378 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0044EA8A` | FUN_0044ea8a | 180 B |
  | `0x0044EB86` | FUN_0044eb86 | 198 B |

- **Known asserts**: `"The 'Vis' object pointer has not been instatiated, but CGame::Pick() is trying to call through it."`, `"Undefined CObjectInfo type requested in CGame::outline_selection()"`, `"Sprite outline currently Unsupported"`
- **Description**: CGame class -- object picking (`CGame::Pick`), selection outline rendering (`CGame::outline_selection`). Dispatches to the Vis subsystem for hit-testing.

#### GammaControl.cpp

- **System**: Display / Configuration
- **Documentation**: [ini-configuration.md](../ini-configuration.md)
- **String address**: `0x004E80CC`
- **Function count**: 3 (280 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0044F2DE` | FUN_0044f2de | 114 B |
  | `0x0044F350` | FUN_0044f350 | 83 B |
  | `0x0044F434` | FUN_0044f434 | 83 B |

- **Known asserts**: `"Gamma control not active"`
- **Description**: DirectDraw gamma ramp control. Initialize, get, and set display gamma correction via the `GammaPos` INI setting.

---

### Rendering

#### Core3D.cpp

- **System**: Software 3D rendering
- **Documentation**: [rendering-pipeline.md](../rendering-pipeline.md)
- **String address**: `0x004E48AC`
- **Function count**: 3 (1,676 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00437AA4` | FUN_00437aa4 | 481 B |
  | `0x00437C85` | FUN_00437c85 | 180 B |
  | `0x00437D39` | FUN_00437d39 | 1,015 B |

- **Known asserts**: `"draw_debug_line() not implemented for SW rendering"`
- **Description**: Software renderer core. Debug line drawing stubs and primitive rasterization for the software rendering path (selected when INI `graphicsmode` = `"SOFTWARE"`).

#### Screen16.cpp

- **System**: Direct3D / DirectDraw rendering
- **Documentation**: [rendering-pipeline.md](../rendering-pipeline.md)
- **String address**: `0x004EF974`
- **Function count**: 17 (7,127 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0049ED46` | FUN_0049ed46 | 1,030 B |
  | `0x0049F14C` | FUN_0049f14c | 998 B |
  | `0x0049FF8B` | FUN_0049ff8b | 1,528 B |
  | `0x004A0583` | FUN_004a0583 | 1,531 B |
  | `0x004A0ED0` | FUN_004a0ed0 | 242 B |
  | `0x004A0FC2` | FUN_004a0fc2 | 178 B |
  | `0x004A1074` | FUN_004a1074 | 133 B |
  | `0x004A10F9` | FUN_004a10f9 | 93 B |
  | `0x004A1156` | FUN_004a1156 | 86 B |
  | `0x004A11AC` | FUN_004a11ac | 177 B |
  | `0x004A125D` | FUN_004a125d | 205 B |
  | `0x004A132A` | FUN_004a132a | 346 B |
  | `0x004A1484` | FUN_004a1484 | 228 B |
  | `0x004A1671` | FUN_004a1671 | 77 B |
  | `0x004A1757` | FUN_004a1757 | 77 B |
  | `0x004A1814` | FUN_004a1814 | 113 B |
  | `0x004A1885` | FUN_004a1885 | 85 B |

- **Known asserts**: `"D3Drend->Init failed."`, `"Direct3D renderer: The device failed to return capabilities."`, `"Direct3D renderer: The device doesn't support the necessary alpha blending modes."`, `"Direct3D renderer: The device doesn't support non-square textures."`, `"There aren't any D3D devices to create."`
- **Description**: Primary hardware rendering subsystem. DirectDraw surface creation, Direct3D device initialization and validation, screenshot capture, surface locking/unlocking. The largest source file by function count.

#### screen16_3d.cpp

- **System**: Hardware 3D rendering (HiScreen16)
- **Documentation**: [rendering-pipeline.md](../rendering-pipeline.md)
- **String address**: `0x004EFBDC`
- **Function count**: 6 (6,076 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004A1E46` | FUN_004a1e46 | 299 B |
  | `0x004A1FE0` | FUN_004a1fe0 | 1,644 B |
  | `0x004A264C` | FUN_004a264c | 1,767 B |
  | `0x004A2F50` | FUN_004a2f50 | 1,423 B |
  | `0x004A4D71` | FUN_004a4d71 | 615 B |
  | `0x004A4FD8` | FUN_004a4fd8 | 328 B |

- **Known asserts**: `"Error executing scratch 3D operations"`, `"HiScreen16::LoadTexture - D3Drend->CreateTexture() failed: %x"`
- **Description**: HiScreen16 class -- hardware-accelerated 3D polygon rendering, texture loading via `D3Drend->CreateTexture()`, and scratch buffer operations for the D3D rendering path.

#### screen16blt.cpp

- **System**: Surface blitting
- **Documentation**: [rendering-pipeline.md](../rendering-pipeline.md)
- **String address**: `0x004EFC48`
- **Function count**: 4 (2,308 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004A520D` | FUN_004a520d | 116 B |
  | `0x004A5281` | FUN_004a5281 | 1,272 B |
  | `0x004A5779` | FUN_004a5779 | 404 B |
  | `0x004A590D` | FUN_004a590d | 516 B |

- **Description**: Blit operations for copying pixel buffers between surfaces. Used extensively by the save/load screen, video playback, and general UI rendering.

#### am_nw.cpp

- **System**: Blitting / compositing
- **Documentation**: [rendering-pipeline.md](../rendering-pipeline.md)
- **String address**: `0x004E1A64`
- **Function count**: 2 (804 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0040D7FB` | FUN_0040d7fb | 453 B |
  | `0x0040D9C0` | FUN_0040d9c0 | 351 B |

- **Known asserts**: `"Problem in Blit_Chroma"`, `"Problem in Blit_Copy"`
- **Description**: Chroma-keyed and plain blit routines. `Blit_Chroma` performs transparency-aware blitting (skipping a designated color key); `Blit_Copy` performs opaque surface copies.

#### Polydata.cpp

- **System**: Map geometry loading
- **Documentation**: [blv-indoor-maps.md](../blv-indoor-maps.md), [indoor-rendering.md](../indoor-rendering.md)
- **String address**: `0x004EE8F4`
- **Function count**: 1 (7,693 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00498D93` | FUN_00498d93 | 7,693 B |

- **Known asserts**: `"Can't load file!"`, `"Unable to find %s in Games.LOD"`
- **Description**: Indoor (BLV) map loader. Reads level geometry from LOD archives including face data (`L.FData`), sector data (`L.RData`, `L.RLData`), door data (`L.DData`), and spawn points. One of the largest single functions in the binary.

#### PolyProjector.cpp

- **System**: Polygon projection / clipping
- **Documentation**: [rendering-pipeline.md](../rendering-pipeline.md), [sprite-billboard.md](../sprite-billboard.md)
- **String address**: `0x004EE978`
- **Function count**: 5 (3,329 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0049B4C9` | FUN_0049b4c9 | 581 B |
  | `0x0049B719` | FUN_0049b719 | 1,069 B |
  | `0x0049BE13` | FUN_0049be13 | 523 B |
  | `0x0049C01E` | FUN_0049c01e | 568 B |
  | `0x0049C28D` | FUN_0049c28d | 588 B |

- **Known asserts**: `"Error: Failed to build decal geometry"`, `"Error: Failed to get the facet orientation"`, `"Undefined clip flag specified"`, `"Uknown strip type detected!"`, `"Lightpoly builder native indoor clipping not implemented"`
- **Description**: Polygon projection and clipping pipeline. Transforms 3D polygons to screen space, builds decal geometry, and clips light polygons against face boundaries.

---

### Lighting

#### Light.cpp

- **System**: Lighting engine
- **Documentation**: [lighting.md](../lighting.md)
- **String address**: `0x004E935C`
- **Function count**: 8 (5,327 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0045BC40` | FUN_0045bc40 | 628 B |
  | `0x0045BEBF` | FUN_0045bebf | 1,587 B |
  | `0x0045CC45` | FUN_0045cc45 | 427 B |
  | `0x0045D10E` | FUN_0045d10e | 754 B |
  | `0x0045D788` | FUN_0045d788 | 775 B |
  | `0x0045DA8F` | FUN_0045da8f | 146 B |
  | `0x0045DB21` | FUN_0045db21 | 449 B |
  | `0x0045DCE2` | FUN_0045dce2 | 561 B |

- **Known asserts**: `"Error: Failed to build light polygon"`, `"Error: Failed to get the facet orientation"`, `"Invalid light type!"`, `"Invalid light type detected!"`, `"Invalid lightmap detected!"`
- **Description**: Core lighting calculations. Builds light polygons from face geometry, validates lightmaps, handles multiple light types (point, directional), and clips light contributions to polygon boundaries.

#### MobileLightStack.cpp

- **System**: Dynamic lighting
- **Documentation**: [lighting.md](../lighting.md)
- **String address**: `0x004EAD28`
- **Function count**: 1 (170 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00467D8C` | FUN_00467d8c | 170 B |

- **Known asserts**: `"Too many mobile lights!"`
- **Description**: Mobile (dynamic) light stack management. Tracks lights attached to moving objects (spells, torches, projectiles) with an overflow guard.

#### StationaryLightStack.cpp

- **System**: Static lighting
- **Documentation**: [lighting.md](../lighting.md)
- **String address**: `0x004F0204`
- **Function count**: 1 (131 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004AD32B` | FUN_004ad32b | 131 B |

- **Known asserts**: `"Too many stationary lights!"`
- **Description**: Stationary (baked) light stack management. Tracks fixed light sources placed in the map with an overflow guard.

---

### Outdoor Maps

#### Odbuild.cpp

- **System**: Outdoor building rendering
- **Documentation**: [outdoor-rendering.md](../outdoor-rendering.md)
- **String address**: `0x004EC0BC`
- **Function count**: 1 (1,479 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00478411` | FUN_00478411 | 1,479 B |

- **Known asserts**: `"D3D version of RenderBuildings called in software!"`
- **Description**: Outdoor building/structure rendering. Renders 3D buildings placed on the outdoor terrain heightmap. Contains a guard against calling the D3D path from software mode.

#### Odmap.cpp

- **System**: Outdoor map loading
- **Documentation**: [odm-outdoor-maps.md](../odm-outdoor-maps.md)
- **String address**: `0x004EC224`
- **Function count**: 1 (7,195 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0047D0AA` | FUN_0047d0aa | 7,195 B |

- **Known asserts**: `"Can't load file!"`, `"Unable to find %s in Games.LOD"`
- **Description**: Outdoor (ODM) map loader. Reads terrain data, sky textures, spawn points, and building geometry from LOD archives. One of the largest single functions in the binary.

#### Odspan.cpp

- **System**: Outdoor terrain span rendering
- **Documentation**: [outdoor-rendering.md](../outdoor-rendering.md)
- **String address**: `0x004EC328`
- **Function count**: 1 (1,092 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00486B52` | FUN_00486b52 | 1,092 B |

- **Known asserts**: `"The Texture Frame Table is not a supported feature."`
- **Description**: Outdoor terrain span rasterization. Draws horizontal terrain spans with texture mapping, using the tile table for terrain type lookup.

---

### Combat / Spells

#### Damage.cpp

- **System**: Combat damage calculation
- **Documentation**: [combat-system.md](../combat-system.md)
- **String address**: `0x004E4AB4`
- **Function count**: 1 (2,483 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00439463` | FUN_00439463 | 2,483 B |

- **Description**: Main damage calculation function. Computes damage from physical attacks and spells, applying resistances, buffs, armor, and character skill modifiers. Called by the combat system and spell effects.

#### seffects.cpp

- **System**: Spell visual effects
- **Documentation**: [spell-system.md](../spell-system.md)
- **String address**: `0x004EFD48`
- **Function count**: 1 (1,027 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004A8BB7` | FUN_004a8bb7 | 1,027 B |

- **Known asserts**: `"spell84"`
- **Description**: Spell visual effect rendering. Manages animated spell overlays and particle effects displayed during spell casting.

---

### Characters / Party

#### Party.cpp

- **System**: Party management
- **Documentation**: [party-management.md](../party-management.md)
- **String address**: `0x004EE5D4`
- **Function count**: 1 (351 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0048C6DC` | FUN_0048c6dc | 351 B |

- **Known asserts**: `"Invalid picture_name detected ::addItem()"`
- **Description**: Party inventory management. The `addItem()` method handles adding items to party member inventories with validation of item picture names.

---

### Items

#### Itemdata.cpp

- **System**: Item definitions and monster data parsing
- **Documentation**: [item-system.md](../item-system.md)
- **String address**: `0x004E8678`
- **Function count**: 2 (5,882 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0045490E` | FUN_0045490e | 978 B |
  | `0x0045504A` | FUN_0045504a | 4,904 B |

- **Description**: Item and monster data table parser. Reads `monsters.txt` and item definition tables, parsing equipment types (WEAPON, ARMOR, SWORD, DAGGER, etc.), status effect strings (curse, poison, disease, etc.), and monster spell assignments.

---

### Events

#### Events.cpp

- **System**: Event scripting engine
- **Documentation**: [event-engine.md](../event-engine.md)
- **String address**: `0x004E7C7C`
- **Function count**: 3 (1,232 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x00444A74` | FUN_00444a74 | 574 B |
  | `0x00445A1C` | FUN_00445a1c | 307 B |
  | `0x00445B4F` | FUN_00445b4f | 351 B |

- **Known asserts**: `"No transition text found!"`, `"NPC id exceeds MAX_DATA!"`
- **Description**: Event script execution and NPC data lookup. Processes map transition events with associated text, and validates NPC identifiers against the data table maximum.

---

### World Generation

#### Generate.cpp

- **System**: Random content generation
- **Documentation**: [generation.md](../generation.md)
- **String address**: `0x004E8194`
- **Function count**: 1 (1,116 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0044F5A8` | FUN_0044f5a8 | 1,116 B |

- **Known asserts**: `"Can't create random monster: '%s'! See MapStats.txt and Monsters.txt!"`
- **Description**: Random monster generation for map encounters. Cross-references `MapStats.txt` with `Monsters.txt` to spawn level-appropriate creatures.

---

### Save / Load

#### LoadSave.cpp

- **System**: Save/load game
- **Documentation**: [save-load.md](../save-load.md)
- **String address**: `0x004E946C`
- **Function count**: 3 (5,152 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0045E073` | FUN_0045e073 | 562 B |
  | `0x0045EEC3` | FUN_0045eec3 | 1,503 B |
  | `0x0045F4A2` | FUN_0045f4a2 | 3,087 B |

- **Known asserts**: `"Unable to find: %s!"`
- **Description**: Game state serialization. Saves and loads `header.bin`, `party.bin`, `clock.bin`, `overlay.bin`, `npcdata.bin`, `npcgroup.bin` to/from save archives. Manages Lloyd's Beacon screenshots (`lloyd%d%d.pcx`), autosave (`autosave.mm7`), and the `new.lod` template for fresh saves.

---

### Input

#### DirectInputKeyboard.cpp

- **System**: Keyboard input (DirectInput)
- **Documentation**: [input-system.md](../input-system.md)
- **String address**: `0x004E4AD8`
- **Function count**: 5 (510 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0043B790` | FUN_0043b790 | 118 B |
  | `0x0043B854` | FUN_0043b854 | 74 B |
  | `0x0043B89E` | FUN_0043b89e | 113 B |
  | `0x0043B90F` | FUN_0043b90f | 130 B |
  | `0x0043B991` | FUN_0043b991 | 75 B |

- **Known asserts**: `"Error: No keyboard found"`, `"Invalid Device pointer, bailing out of update_data()"`, `"Invalid Device pointer, bailing out of set_acquire()"`
- **Description**: DirectInput keyboard wrapper. Creates the DI keyboard device, acquires/unacquires it, and polls keystroke data. Guards against null device pointers.

#### DirectInputMouse.cpp

- **System**: Mouse input (DirectInput)
- **Documentation**: [input-system.md](../input-system.md)
- **String address**: `0x004E4BB4`
- **Function count**: 4 (532 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0043BA22` | FUN_0043ba22 | 129 B |
  | `0x0043BAF1` | FUN_0043baf1 | 74 B |
  | `0x0043BB3B` | FUN_0043bb3b | 113 B |
  | `0x0043BBAC` | FUN_0043bbac | 216 B |

- **Known asserts**: `"Error: No mouse found"`
- **Description**: DirectInput mouse wrapper. Creates the DI mouse device, acquires/unacquires it, and reads mouse state (position, buttons).

#### KeyboardAsync.cpp

- **System**: Asynchronous keyboard input
- **Documentation**: [input-system.md](../input-system.md)
- **String address**: `0x004E924C`
- **Function count**: 4 (1,178 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0045B2E0` | FUN_0045b2e0 | 130 B |
  | `0x0045B362` | FUN_0045b362 | 123 B |
  | `0x0045B3EF` | FUN_0045b3ef | 233 B |
  | `0x0045B5B6` | FUN_0045b5b6 | 692 B |

- **Known asserts**: `"Invalid DI_Keyboard, bailing out of resume()"`, `"Invalid DI_Keyboard, bailing out of suspend()"`, `"Invalid DI_Keyboard, bailing out of update_keyboard_data()"`, `"Uknown key detected!"`
- **Description**: Threaded keyboard input manager. Runs keyboard polling on a separate thread with suspend/resume control via `CriticalSection`. Translates DirectInput scan codes to game key identifiers.

#### Mouse.cpp

- **System**: Mouse cursor / UI interaction
- **Documentation**: [input-system.md](../input-system.md)
- **String address**: `0x004EADA0`
- **Function count**: 1 (888 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0046A338` | FUN_0046a338 | 888 B |

- **Known asserts**: `"Warning: Invalid ID reached!"`
- **Description**: High-level mouse cursor handler. Processes mouse click events, determines which UI element or game object was clicked, and dispatches the appropriate action.

#### MouseAsync.cpp

- **System**: Asynchronous mouse input
- **Documentation**: [input-system.md](../input-system.md)
- **String address**: `0x004EADF4`
- **Function count**: 5 (907 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0046AE9B` | FUN_0046ae9b | 185 B |
  | `0x0046AF54` | FUN_0046af54 | 290 B |
  | `0x0046B153` | FUN_0046b153 | 142 B |
  | `0x0046B380` | FUN_0046b380 | 164 B |
  | `0x0046BBD4` | FUN_0046bbd4 | 126 B |

- **Known asserts**: `"Could not clip cursor to screen!"`, `"Could not load async mouse cursor image"`, `"DI_Mouse pointer invalid; bailing out from suspend()"`, `"DI_Mouse pointer invalid bailing out from update_mouse_data()"`
- **Description**: Threaded mouse input manager. Manages the async mouse cursor image, suspend/resume of the mouse polling thread, cursor clipping to the game window, and mouse data updates.

---

### UI / Text

#### Font.cpp

- **System**: Font rendering
- **Documentation**: [ui-windows.md](../ui-windows.md)
- **String address**: `0x004E7D88`
- **Function count**: 3 (1,684 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x0044C794` | FUN_0044c794 | 459 B |
  | `0x0044C95F` | FUN_0044c95f | 540 B |
  | `0x0044CE34` | FUN_0044ce34 | 685 B |

- **Known asserts**: `"Invalid string passed !"`, `"Invalid string passed!"`
- **Description**: Bitmap font text rendering. Measures string width, handles text wrapping, and renders character glyphs. Includes word-wrap and color-tag parsing for rich text display.

#### Show.cpp

- **System**: Cinematic / FMV display
- **Documentation**: [video-system.md](../video-system.md)
- **String address**: `0x004EFEA4`
- **Function count**: 1 (241 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004A94BD` | FUN_004a94bd | 241 B |

- **Known asserts**: `"No movie"`, `"Invalid movie requested in CShow::Run()"`
- **Description**: `CShow::Run()` dispatcher. Plays named video sequences (`3dologo`, `new world logo`, `Intro`, `Intro Post`, `losegame`, `end_seq1`) by name lookup with validation.

#### Vis.cpp

- **System**: Visibility / hit-testing
- **Documentation**: [visibility.md](../visibility.md)
- **String address**: `0x004F1060`
- **Function count**: 3 (652 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004C0703` | FUN_004c0703 | 396 B |
  | `0x004C1B1C` | FUN_004c1b1c | 71 B |
  | `0x004C2503` | FUN_004c2503 | 185 B |

- **Known asserts**: `"Default case reached in VIS"`, `"Undefined type requested for: CVis::get_object_zbuf_val()"`, `"Unknown pointer creation flag passed to ::create_object_pointers()"`, `"Unsupported \"exclusion if no event\" type in CVis::is_part_of_selection"`
- **Description**: CVis class -- visibility determination and object picking. Z-buffer-based hit testing (`get_object_zbuf_val`), object pointer creation for selection, and event-based exclusion filtering.

---

### Audio / Video

#### Sound.cpp

- **System**: Audio playback
- **Documentation**: [audio-system.md](../audio-system.md)
- **String address**: `0x004EFF88`
- **Function count**: 2 (782 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004A9756` | FUN_004a9756 | 397 B |
  | `0x004A9B4D` | FUN_004a9b4d | 385 B |

- **Known asserts**: `"Can't load sound file!"`, `"Sound %s is size %i bytes, sound buffer size is %i bytes"`
- **Description**: Sound file loading from the `sounds.lod` archive. Reads raw audio data with file pointer seeking, validates buffer sizes, and reports size mismatches.

#### Video.cpp

- **System**: Video playback
- **Documentation**: [video-system.md](../video-system.md)
- **String address**: `0x004F0F34`
- **Function count**: 2 (1,146 bytes total)
- **Functions**:

  | Address | Name | Size |
  |---------|------|------|
  | `0x004BF1F5` | FUN_004bf1f5 | 362 B |
  | `0x004BFD95` | FUN_004bfd95 | 784 B |

- **Known asserts**: `"Unsupported Bink playback!"`, `"Can't load %s"`
- **Description**: Smacker/Bink video playback. Opens `.smk` video files via `SmackToBuffer`/`SmackBufferOpen`, sets volume/panning via `SmackVolumePan`, and configures Bink buffer scaling and surface type detection.

---

## Address Range Summary

| Source File | System | Functions | Total Size | Address Range | Documentation |
|-------------|--------|-----------|------------|---------------|---------------|
| am_nw.cpp | Blitting | 2 | 804 B | `0x0040D7FB`--`0x0040D9C0` | [rendering-pipeline.md](../rendering-pipeline.md) |
| Core3D.cpp | SW Rendering | 3 | 1,676 B | `0x00437AA4`--`0x00437D39` | [rendering-pipeline.md](../rendering-pipeline.md) |
| Damage.cpp | Combat | 1 | 2,483 B | `0x00439463` | [combat-system.md](../combat-system.md) |
| DirectInputKeyboard.cpp | Input | 5 | 510 B | `0x0043B790`--`0x0043B991` | [input-system.md](../input-system.md) |
| DirectInputMouse.cpp | Input | 4 | 532 B | `0x0043BA22`--`0x0043BBAC` | [input-system.md](../input-system.md) |
| Events.cpp | Events | 3 | 1,232 B | `0x00444A74`--`0x00445B4F` | [event-engine.md](../event-engine.md) |
| Font.cpp | UI/Text | 3 | 1,684 B | `0x0044C794`--`0x0044CE34` | [ui-windows.md](../ui-windows.md) |
| Game.cpp | Core | 2 | 378 B | `0x0044EA8A`--`0x0044EB86` | [architecture.md](../architecture.md) |
| GammaControl.cpp | Display | 3 | 280 B | `0x0044F2DE`--`0x0044F434` | [ini-configuration.md](../ini-configuration.md) |
| Generate.cpp | Generation | 1 | 1,116 B | `0x0044F5A8` | [generation.md](../generation.md) |
| Itemdata.cpp | Items | 2 | 5,882 B | `0x0045490E`--`0x0045504A` | [item-system.md](../item-system.md) |
| KeyboardAsync.cpp | Input | 4 | 1,178 B | `0x0045B2E0`--`0x0045B5B6` | [input-system.md](../input-system.md) |
| Light.cpp | Lighting | 8 | 5,327 B | `0x0045BC40`--`0x0045DCE2` | [lighting.md](../lighting.md) |
| LoadSave.cpp | Save/Load | 3 | 5,152 B | `0x0045E073`--`0x0045F4A2` | [save-load.md](../save-load.md) |
| MobileLightStack.cpp | Lighting | 1 | 170 B | `0x00467D8C` | [lighting.md](../lighting.md) |
| Mouse.cpp | Input | 1 | 888 B | `0x0046A338` | [input-system.md](../input-system.md) |
| MouseAsync.cpp | Input | 5 | 907 B | `0x0046AE9B`--`0x0046BBD4` | [input-system.md](../input-system.md) |
| Odbuild.cpp | Outdoor | 1 | 1,479 B | `0x00478411` | [outdoor-rendering.md](../outdoor-rendering.md) |
| Odmap.cpp | Outdoor | 1 | 7,195 B | `0x0047D0AA` | [odm-outdoor-maps.md](../odm-outdoor-maps.md) |
| Odspan.cpp | Outdoor | 1 | 1,092 B | `0x00486B52` | [outdoor-rendering.md](../outdoor-rendering.md) |
| Party.cpp | Party | 1 | 351 B | `0x0048C6DC` | [party-management.md](../party-management.md) |
| Polydata.cpp | Maps/Indoor | 1 | 7,693 B | `0x00498D93` | [blv-indoor-maps.md](../blv-indoor-maps.md) |
| PolyProjector.cpp | Rendering | 5 | 3,329 B | `0x0049B4C9`--`0x0049C28D` | [rendering-pipeline.md](../rendering-pipeline.md) |
| Screen16.cpp | HW Rendering | 17 | 7,127 B | `0x0049ED46`--`0x004A1885` | [rendering-pipeline.md](../rendering-pipeline.md) |
| screen16_3d.cpp | HW 3D | 6 | 6,076 B | `0x004A1E46`--`0x004A4FD8` | [rendering-pipeline.md](../rendering-pipeline.md) |
| screen16blt.cpp | Blitting | 4 | 2,308 B | `0x004A520D`--`0x004A590D` | [rendering-pipeline.md](../rendering-pipeline.md) |
| seffects.cpp | Spells | 1 | 1,027 B | `0x004A8BB7` | [spell-system.md](../spell-system.md) |
| Show.cpp | Video/UI | 1 | 241 B | `0x004A94BD` | [video-system.md](../video-system.md) |
| Sound.cpp | Audio | 2 | 782 B | `0x004A9756`--`0x004A9B4D` | [audio-system.md](../audio-system.md) |
| StationaryLightStack.cpp | Lighting | 1 | 131 B | `0x004AD32B` | [lighting.md](../lighting.md) |
| Video.cpp | Video | 2 | 1,146 B | `0x004BF1F5`--`0x004BFD95` | [video-system.md](../video-system.md) |
| Vis.cpp | Visibility | 3 | 652 B | `0x004C0703`--`0x004C2503` | [visibility.md](../visibility.md) |

**Totals**: 32 source files, 101 mapped functions, 70,263 bytes of code

---

## Coverage Analysis

### Mapped vs Total

| Category | Count | Percentage |
|----------|-------|------------|
| Total functions in binary | ~2,300 | 100% |
| Classified by subsystem | ~1,100 | 48% |
| Mapped to original source file | 101 | 4.4% |
| Unclassified | ~1,200 | 52% |

### Source File Size Distribution

The largest source files by total code size (mapped functions only):

1. **Polydata.cpp** -- 7,693 B (indoor map loader)
2. **Odmap.cpp** -- 7,195 B (outdoor map loader)
3. **Screen16.cpp** -- 7,127 B (DirectDraw/D3D rendering, 17 functions)
4. **screen16_3d.cpp** -- 6,076 B (hardware 3D rendering)
5. **Itemdata.cpp** -- 5,882 B (item/monster data tables)
6. **Light.cpp** -- 5,327 B (lighting engine, 8 functions)
7. **LoadSave.cpp** -- 5,152 B (save/load system)

### Unmapped Functions

Approximately 1,200 functions (~52% of the binary) could not be mapped to any
original source file. These functions have no embedded debug strings containing
file paths. They fall into several categories:

- **CRT and standard library**: 39 functions from the MSVC 6.0 C runtime
  (malloc, strlen, memset, exception handling, etc.)
- **UI subsystem**: ~856 functions handling windows, dialogs, inventory screens,
  character sheets, spell book, and other interface elements -- the largest
  unattributed block
- **Rendering helpers**: Functions called by the mapped rendering code but not
  themselves containing assert strings
- **Game logic**: Character stats, skill checks, quest tracking, and other
  gameplay code without debug instrumentation

The functions that *are* mapped tend to be those with defensive assert checks
(bounds validation, null pointer guards, unsupported-feature stubs), which
naturally embed the source file name as part of the assertion macro.

---

## Notes

- All addresses refer to the v1.21 retail binary (`MM7-Rel.exe`)
- Function names are Ghidra auto-generated (`FUN_XXXXXXXX`) where no symbol information exists
- The `_classification.json` file contains no populated `"file"` fields; all source file attribution comes from embedded debug strings in `strings_with_refs.tsv` and `functions_enhanced.tsv`
- Some functions may belong to additional source files not identified here, as only functions with assert/debug strings reveal their origin
- The address ranges shown are the spans of the *known* functions; other functions from the same source file likely exist in the gaps between them
