---
title: "Indoor Rendering (BLV)"
summary: "Indoor rendering uses sectors, portals, and BSP traversal to produce visible geometry."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Indoor Rendering (BLV)

Indoor rendering uses sectors, portals, and BSP traversal to produce visible geometry.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

Indoor levels (BLV maps) are rendered using a **BSP tree / portal** system. Each
indoor level is divided into sectors, and each sector may contain a BSP tree that
partitions its faces for front-to-back rendering. Portal faces connect sectors and
control visibility propagation.

The indoor renderer is the largest single subsystem in the rendering engine. The
main indoor rendering function (FUN_00427db8) is 27,569 bytes -- the largest
function in the entire binary.

**Original source files** (from debug strings):

- `Light.cpp` -- Indoor lighting calculations
- `Polydata.cpp` -- BSP/face/map data buffer allocation
- `PolyProjector.cpp` -- Vertex transformation and projection

---

## 1. Indoor Render Entry Point

### RenderIndoor (FUN_00441bf7, 299 bytes)

Called when `DAT_006be1e0 == 1` (indoor map mode). Orchestrates the full
indoor rendering pass:

```text
1. FUN_00402cae()    -- Compute sin/cos lookup for party angles
2. FUN_00440c10()    -- Main indoor scene rendering
3. FUN_0047a819()    -- Process billboards / sprites
4. FUN_00440f2a()    -- Particle effects update

```

### Indoor::RenderScene (FUN_00440c10, 203 bytes)

This function orchestrates the indoor rendering pass:

1. Call `FUN_0043f3c1()` -- BSP traversal and face collection.
2. Conditionally call `FUN_004b08ca()` -- Software span wireframe debug overlay
   (debug mode only).
3. Iterate over visible faces in the span buffer and draw visible edges
   (SW mode only).

---

## 2. BSP Tree Walk

### Indoor::TraverseBSPAndCollectFaces (FUN_0043f3c1, 375 bytes)

The main indoor rendering dispatcher. This function ties together camera setup,
BSP traversal, face collection, and sprite gathering:

```text
1. Camera::SetupViewParams()               -- FUN_004407fc
2. Portal/sector setup (if sector valid)   -- FUN_00440b67
3. Software span rendering of faces        -- FUN_00440cdb
4. Initialize span/portal buffer           -- FUN_0043f976
5. Collect sprites in visible sectors      -- FUN_004402b2
6. Collect monsters/objects in sectors     -- FUN_0043fe10
7. Loop over visible sectors:
     Per face: FUN_0043fa56()
8. Assign depth to billboards              -- FUN_0043f538
9. Finalize frame                          -- FUN_0044f1be

```

Indoor lighting is set up by `FUN_00467d8c()`, which computes ambient lighting
from the sector's ambient color values (`DAT_00ae306c/3068/3064` for RGB) plus
a time-of-day modulation factor.

### Indoor::BSPTreeWalk (FUN_004406df, 285 bytes) -- CRITICAL FUNCTION

This is the **recursive BSP tree traversal** function that determines face
rendering order for correct visibility.

**Algorithm:**

1. Look up the current sector from the sector array:
   `DAT_0051c5a0 + sectorIndex * 0x8CC`

2. Look up the current BSP node:
   `DAT_006be4ec + nodeIndex * 8`
   (BSP nodes are 8 bytes each)

3. Get the splitting face for this node:
   `faceIndex * 0x60 + DAT_006be4c4`
   (Face stride is 0x60 = 96 bytes)

4. Compute the **dot product** of the camera position against the splitting
   plane:

```text
   dot = face.normalX * cameraX
       + face.normalY * cameraY
       + face.normalZ * cameraZ
       + face.planeDistance
   ```

   The face normal is at offsets `+0x10, +0x14, +0x18` (fixed-point integers),
   and the plane distance is at `+0x1C`.

5. **Portal check:** If the face has the portal flag set (bit 0 of the byte at
   offset `+0x2C`), and the portal's target sector differs from the current
   sector, **negate** the dot product. This ensures correct traversal order
   when crossing portals.

6. **Traverse based on sign of dot product:**
   - **Positive (camera in front):** Visit front child first, draw current
     node's coplanar faces, then visit back child.
   - **Negative (camera behind):** Visit back child first, draw current
     node's coplanar faces, then visit front child.

7. **Draw faces** in the face list at the current node:
   `psVar1[2]` = face start index, `psVar1[3]` = face count.

8. **Termination:** When a child index is `-1`, that branch is a leaf
   (no further traversal).

---

## 3. Data Structures

### BSP Node (8 bytes)

Stored in an array at `DAT_006be4ec`.

```text
Offset  Size    Field
0x00    int16   frontChild       -- Index of front child node (-1 = leaf)
0x02    int16   backChild        -- Index of back child node (-1 = leaf)
0x04    int16   faceStartIndex   -- Start index into sector's face list
0x06    int16   faceCount        -- Number of coplanar faces at this node

```

### BLV Face (0x60 = 96 bytes)

Stored in an array at `DAT_006be4c4`.

```text
Offset  Size    Field
0x00    --      Vertex data / indices
0x10    int32   normalX (fixed-point, scale 65536)
0x14    int32   normalY (fixed-point, scale 65536)
0x18    int32   normalZ (fixed-point, scale 65536)
0x1C    int32   planeDistance
0x1D    byte    orientationType
                  bit 0x20 = NoDraw
                  bit 0x40 = portal flag
0x1E    byte    flags2
                  bit 0x40 = animated texture
0x2C    uint32  faceAttributes
                  bit 0  = isPortal
                  bit 2  = flow (water/lava animation)
                  bit 4  = translucent
                  bit 5  = flow2 (secondary flow)
                  bit 11 = noLight
                  bit 13 = invisible
0x30    int32*  vertexIndexList pointer
0x34    int32*  UV list pointer 1
0x38    int32*  UV list pointer 2
0x3C    int32*  UV list pointer 3
0x40    int32*  UV list pointer 4
0x44    int16   textureFrameIndex
0x48    int16   sector/texture index
0x4A    int16   texture handle (resolved at load time)
0x4C    int16   sectorID (which sector this face belongs to)
0x4D    byte    light level
0x5D    byte    vertexCount

```

### BLV Sector (0x74 = 116 bytes)

Stored in an array at `DAT_006be4d4` with stride `0x74`.

```text
Offset  Size    Field
0x00    byte    flags
                  bit 4 = hasBSP (sector has a BSP tree)
0x18    int16   numDecorations
0x2E    int16   numFaces
0x30    int32*  faceIndexList pointer
0x44    int16   numDecorations (duplicate/secondary count)

```

The sector's flags byte at offset `+0x00` determines whether BSP traversal
is needed: if bit 4 is set, the sector has a BSP tree and `BSPTreeWalk` is
called recursively. Otherwise, faces are rendered in list order.

---

## 4. Portal Culling

Portal faces are the mechanism for inter-sector visibility. When the BSP
traversal encounters a portal face:

1. The portal face's attribute bit 0 (`isPortal`) is checked.
2. The portal's target sector is compared to the current rendering sector.
3. If they differ, the dot product sign is negated to ensure the traversal
   enters the connected sector correctly.
4. The portal's screen-space bounding rectangle is used to clip rendering
   in the target sector, preventing overdraw of geometry not visible
   through the portal opening.

The span/portal buffer (initialized by `Indoor::InitSpanBuffer`, FUN_0043f976,
142 bytes) tracks which screen spans are visible through each portal. This
allows the renderer to skip faces in sectors that are not visible through
any portal chain from the camera's current sector.

---

## 5. Sector Rendering

### Indoor::RenderSectorFaces (FUN_0044065c, 131 bytes)

Iterates over the faces in a sector and dispatches each to the appropriate
renderer:

- **Software path:** calls `FUN_004afae9()` per face.
- **D3D path:** calls `FUN_004b0e0b()` per face.

The sector data is accessed at `DAT_006be4d4` with stride `0x74` (116 bytes
per sector). The face count is at offset `+0x2E` and the face list pointer
at `+0x30`.

### Indoor::ProcessFace (FUN_0043fa56, 954 bytes)

Processes a single indoor face for rendering:

1. Check face visibility flags (NoDraw, invisible).
2. Look up the face's texture from the texture frame table.
3. Transform face vertices from world to view space.
4. Apply portal clipping if the face is visible through a portal chain.
5. Compute texture mapping coordinates.
6. Submit the face to the software span buffer or D3D render list.

For faces with fire/light decorations, particle effects are created using
the `effpar01` texture (referenced at 0x004e5d28).

---

## 6. Face Rendering and Texture Mapping

### Texture mapping basis vectors

**PolyProjector::ComputeFaceOrientation** (FUN_00436921, 169 bytes) determines
the texture coordinate basis vectors for a face based on its orientation:

| Orientation type | Texture U basis | Texture V basis |
|-----------------|-----------------|-----------------|
| 1 (vertical wall) | `(-normalY, normalX, 0)` | `(0, 0, 1)` |
| 3, 5 (horizontal floor/ceiling) | `(1, 0, 0)` | `(0, 1, 0)` |
| 4, 6 (sloped surface) | Wall mapping if steep, floor mapping if shallow | -- |

### Vertex transformation pipeline

1. **World to view:** `TransformVertexWorldToView` (FUN_00436512) applies
   the camera's yaw and pitch rotation using precomputed sin/cos values.

2. **View to screen:** `ProjectVertexToScreen` (FUN_00436ba6) performs
   perspective division to convert view-space coordinates to screen
   coordinates.

3. **Vector normalization:** `NormalizeVector3` (FUN_004369ca, 73 bytes)
   normalizes a 3D vector: `v = v / sqrt(x*x + y*y + z*z)`.

### Face attribute flags

The face attributes word at offset `+0x2C` controls rendering behavior:

| Bit | Hex | Meaning |
|-----|-----|---------|
| 0 | 0x0001 | Portal (connects to another sector) |
| 2 | 0x0004 | Flow (water/lava texture animation) |
| 4 | 0x0010 | Translucent (alpha blended) |
| 5 | 0x0020 | Flow2 (secondary animation pattern) |
| 11 | 0x0800 | NoLight (face receives no lighting) |
| 13 | 0x2000 | Invisible (not rendered, collision only) |

The flags2 byte at offset `+0x1E` uses bit 0x40 for animated texture
sequences (texture frame table lookup).

---

## 7. Indoor Lighting

Indoor lighting combines **sector ambient light** with **per-face light
contributions** from both stationary and mobile light sources.

### Sector ambient light

Each sector has ambient RGB values stored in globals:

- `DAT_00ae306c` -- Ambient Red
- `DAT_00ae3068` -- Ambient Green
- `DAT_00ae3064` -- Ambient Blue

These are set by `FUN_00467d8c()` (from `MobileLightStack.cpp`), which also
applies a time-of-day modulation factor.

### Per-face lighting

Each face has a light level byte at offset `+0x4C` in the face structure. Faces
with the `noLight` attribute bit (bit 11 = 0x0800) skip all dynamic lighting
computations.

The string `"Lightpoly builder native indoor clipping not implemented"` (at
0x004e9380, referenced by both FUN_0045bebf and FUN_0049b719) indicates that
the indoor lighting polygon clipper has a fallback path when native clipping
is not available.

Additional light-related error strings:

- `"Error: Failed to build light polygon"` (FUN_0045bc40)
- `"Invalid light type!"` (FUN_0045bebf)
- `"Invalid light type detected!"` (FUN_0045cc45)
- `"Invalid lightmap detected!"` (FUN_0045da8f, FUN_0045d788)

See [lighting.md](lighting.md) for the full lighting system documentation.

---

## 8. Polydata Buffers

### Polydata::AllocateBuffers (FUN_00498c94)

Allocates the rendering buffers used during indoor BSP traversal and face
collection. The allocation names come from debug strings in the binary.

| Offset | Size (bytes) | Debug name | Purpose |
|--------|-------------|------------|---------|
| `+0x274` | 90,000 | -- | Face vertex data |
| `+0x27C` | 960,000 | -- | Large polygon buffer |
| `+0x284` | 180,000 | -- | Additional polygon data |
| `+0x28C` | 59,392 (0xE800) | -- | Intermediate data |
| `+0x294` | 6,400 (0x1900) | -- | Small buffer |
| `+0x29C` | 16,000 | -- | Sector data |
| `+0x2A4` | 40,000 | `"L.BSP"` | BSP tree data |
| `+0x2A8` | 84,004 (0x14824) | `"L.Map"` | Map data |

The `"L.BSP"` and `"L.Map"` names (at 0x004ee8c0) confirm that these buffers
hold BSP tree and map-level rendering data respectively.

---

## 9. Software Span Rendering

In the software path, indoor faces are rendered through a **span buffer**
system rather than per-pixel Z-buffering. The span buffer records horizontal
pixel runs (spans) for each visible face, allowing the renderer to composite
faces in the correct order without a per-pixel depth test.

### Span buffer initialization

`Indoor::InitSpanBuffer` (FUN_0043f976, 142 bytes) clears and prepares the
span buffer at the start of each frame.

### Span rendering

`FUN_00440cdb` performs the software span rendering of collected faces. For
each face in the span buffer:

1. Look up texture data (palette, pixel data, dimensions).
2. For each span (horizontal run of pixels):
   - Interpolate texture coordinates across the span.
   - Apply lighting (sector ambient + dynamic lights).
   - Write 16-bit pixels to the back buffer.

### Debug overlay

`Indoor::DrawSpanDebugOutline` (FUN_004b08ca, 190 bytes) draws wireframe
edges over the span buffer output. The string `"SPANS"` (at 0x004ec2ec,
referenced by FUN_00486a2c) is associated with span-related debug output.

---

## 10. Indoor Billboard Collection

### Indoor::CollectBillboards (FUN_004402b2, 938 bytes)

Gathers item and decoration sprites from visible indoor sectors. Processes
sprites at stride `0x34` (52 bytes per entry) using `DAT_006650ac` as the
sprite count. Handles indoor-specific animation timings.

### Indoor::CollectMonsterBillboards (FUN_0043fe10, 1186 bytes)

Gathers monster sprites from visible indoor sectors. For each monster:

1. Check if the monster is in a visible sector.
2. Compute screen-space position from world coordinates.
3. Select the appropriate animation frame based on facing direction and
   animation state.
4. Add to the billboard render list.

### Depth assignment

`FUN_0043f538` assigns final depth values to all collected billboards after
BSP traversal is complete. This ensures correct depth sorting when billboards
are composited over the span-rendered faces.

---

## Key Function Reference

| Address | Size | Suggested Name | Description |
|---------|------|----------------|-------------|
| 0x00427db8 | 27,569 | `Indoor::RenderMain` | Largest indoor render function |
| 0x0043f3c1 | 375 | `Indoor::TraverseBSPAndCollect` | BSP traversal + face collection |
| 0x0043f538 | -- | `Indoor::AssignBillboardDepth` | Depth assignment for billboards |
| 0x0043f976 | 142 | `Indoor::InitSpanBuffer` | Initialize portal/span buffer |
| 0x0043fa56 | 954 | `Indoor::ProcessFace` | Process single indoor face |
| 0x0043fe10 | 1186 | `Indoor::CollectMonsterBillboards` | Gather monster sprites |
| 0x004402b2 | 938 | `Indoor::CollectBillboards` | Gather item/decoration sprites |
| 0x0044065c | 131 | `Indoor::RenderSectorFaces` | Dispatch faces to renderer |
| 0x004406df | 285 | `Indoor::BSPTreeWalk` | BSP tree recursive traversal |
| 0x00440b67 | -- | `Indoor::PortalSectorSetup` | Portal/sector initialization |
| 0x00440c10 | 203 | `Indoor::RenderScene` | Indoor scene orchestrator |
| 0x00440cdb | -- | `Indoor::RenderSpans` | Software span rendering |
| 0x00441bf7 | 299 | `RenderIndoor` | Indoor render entry point |
| 0x004afae9 | -- | `Indoor::RenderFace_SW` | Software face rasterizer |
| 0x004b08ca | 190 | `Indoor::DrawSpanDebugOutline` | SW span debug wireframe |
| 0x004b0e0b | -- | `Indoor::RenderFace_D3D` | D3D face renderer |

---

## Integration notes

1. **BSP traversal is essential.** The BSP tree walk determines face rendering
   order and must be faithfully reimplemented. The 8-byte BSP node format is
   simple and well-understood.

2. **Portal culling is a key optimization.** Sectors not visible through any
   portal chain from the camera's current sector should be skipped entirely.

3. **Fixed-point normals.** Face normals use a scale factor of 65536
   (16-bit fixed-point). The reimplementation should convert to floating-point
   at load time.

4. **Replace span buffer with Z-buffer.** Modern GPUs handle per-pixel depth
   testing in hardware, making the software span buffer unnecessary. However,
   the portal visibility system should still be used for culling.

5. **Face attribute flags drive rendering behavior.** The attribute bitmask
   at offset `+0x2C` controls portal traversal, transparency, animation, and
   lighting -- all must be interpreted correctly.

6. **Sector ambient lighting.** Each sector has its own ambient light level
   that must be applied to all faces within it.
