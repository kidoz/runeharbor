---
title: "Visibility and Object Picking (CVis System)"
summary: "The CVis system combines depth-buffer queries and ray tests for visibility and object picking."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Visibility and Object Picking (CVis System)

The CVis system combines depth-buffer queries and ray tests for visibility and object picking.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

The CVis (Visibility) system handles object picking -- determining which game object
the player is pointing at or clicking on. It uses a combination of z-buffer queries
and ray-based selection to identify objects under the cursor. The system supports
faces, sprites, actors, and interactive objects.

**Source file reference:** `D:\mm7Src_eng\MM7\Code\Vis.cpp` (at `004f1060`)
**Functions:** `FUN_004c1b1c`, `FUN_004c2503`, `FUN_004c0703`

---

## 1. Architecture

The CVis system is a class instantiated at runtime and accessed through the game
object (`CGame`). The game's `Pick()` method delegates to CVis for all object
selection.

### Entry Point

```cpp
CGame::Pick() -> CVis methods

```

Error if CVis is not initialized:
`"The 'Vis' object pointer has not been instatiated, but CGame::Pick() is trying to
call through it."` (at `004e7ff8`, function `FUN_0044ea8a`)

---

## 2. Z-Buffer Based Selection

### 2.1 get_object_zbuf_val()

**Function:** `FUN_004c1b1c`

Retrieves the z-buffer value for a given screen-space object. The z-buffer encodes
both depth and object identity:

```text
Z-buffer value encoding:
  High bits: Depth (distance from camera)
  Low bits:  Object ID / type identifier

```

Error: `"Undefined type requested for: CVis::get_object_zbuf_val()"` (at `004f1080`)
when an unknown object type is passed.

The z-buffer value is read from the hardware z-buffer (Direct3D) or software z-buffer
at the pixel coordinates corresponding to the object's screen position.

### 2.2 Z-Buffer String Reference

The engine references z-buffer in multiple rendering contexts:

- `"Z Buf."` (at `004ef9a8`) -- z-buffer debug label
- `"Init - Failed to create z-buffer.\n"` (at `004eea4c`)
- `"Init - Failed to attach z-buffer to back buffer.\n"` (at `004eea18`)
- `"Init - Failed to enumerate Z buffer formats.\n"` (at `004eeae4`)
- `"DDERR_NOZBUFFERHW"` (at `004ea8d4`) -- hardware z-buffer not available

---

## 3. Object Type Selection

### 3.1 is_part_of_selection()

**Function:** `FUN_004c0703`

Determines whether a given object should be included in the current selection set.
This is a filter function that checks object type and event eligibility.

Error: `"Unsupported "exclusion if no event" type in CVis::is_part_of_selection"`
(at `004f1018`) when an unsupported exclusion type is encountered.

Error: `"Default case reached in VIS"` (at `004f0ffc`) when an unknown object type
falls through the selection switch.

### 3.2 Object Types

The CVis system recognizes multiple object types for selection:

| Type            | Description                                  |
|-----------------|----------------------------------------------|
| Face            | Map geometry face (wall, floor, ceiling)     |
| Sprite          | 2D billboard sprite (item, decoration)       |
| Actor/Monster   | Active game entity (NPC, monster)            |
| Decoration      | Static 3D decoration object                  |

### 3.3 CObjectInfo

The `CObjectInfo` class represents a selectable object. It carries type information
used by the outline and selection systems.

Error: `"Undefined CObjectInfo type requested in CGame::outline_selection()"`
(at `004e8088`, function `FUN_0044eb86`) when an invalid object type is passed to
the outline renderer.

Additional: `"Sprite outline currently Unsupported"` (at `004e805c`) indicates that
sprite outline rendering was not fully implemented.

---

## 4. outline_selection()

**Function:** `FUN_0044eb86`

When the player hovers over or selects an object, the engine draws an outline around
it to provide visual feedback. This function:

1. Gets the object type from the CObjectInfo
2. For faces: highlights the face edges with a colored outline
3. For actors/monsters: draws a bounding outline around the entity
4. For sprites: currently unsupported (produces warning)
5. For unknown types: produces error message

---

## 5. Pick Operation Flow

The complete picking operation:

```text
1. Player moves mouse or clicks
2. CGame::Pick() is called with screen coordinates
3. CVis reads the z-buffer at those coordinates
4. Z-buffer value is decoded to get object type + ID
5. is_part_of_selection() filters the object:
   - Check if object type is selectable
   - Check if object has an associated event
   - Apply exclusion rules
6. If valid: object is selected, outline is drawn
7. On click: dispatch interaction event to game logic

```

---

## 6. Mouse-Over Detection

The CVis system is called every frame to update the mouse-over state:

- **No object under cursor**: Default cursor, no tooltip
- **Interactive face**: Highlight face, show interaction cursor
- **Monster/NPC**: Show name tooltip, highlight entity
- **Item on ground**: Show item name, pickup cursor
- **Clickable decoration**: Show interaction cursor

The mouse-over object ID is stored globally and used by:

- The tooltip system (show object name/description)
- The cursor system (change cursor icon)
- The click handler (dispatch appropriate action)

---

## 7. Selection Exclusion Rules

The `is_part_of_selection()` function supports exclusion filters:

- **"exclusion if no event"**: Objects without associated events are excluded from
  selection. This prevents the player from clicking on non-interactive geometry.
- **Type-based exclusion**: Certain object types can be excluded from selection
  based on the current game mode (e.g., during casting, only valid targets are
  selectable).

---

## 8. Integration with Rendering

The CVis system depends on the rendering pipeline:

- **Z-buffer**: Must be populated by the renderer before CVis can query it
- **Screen coordinates**: CVis works in screen space, not world space
- **Object encoding**: The renderer must encode object identity in z-buffer values
  when writing pixels

For the software renderer, z-buffer values are maintained in a software buffer.
For Direct3D, the hardware z-buffer is queried.

---

## Integration notes

- The z-buffer object encoding is the key mechanism -- depth and object ID are packed
  into a single value per pixel
- SDL3 + custom renderer can use a similar approach with a pick buffer or
  color-coded render pass
- Modern alternatives include ray casting from camera through screen point into
  world space, intersecting with scene geometry
- The selection filter system should be data-driven based on current game mode
- Outline rendering needs per-face edge detection or post-processing
- Sprite outline was not implemented in the original; consider implementing it
- The `CObjectInfo` type system maps to a discriminated union or `std::variant`
