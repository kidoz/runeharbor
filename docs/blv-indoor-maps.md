---
title: "BLV Indoor Map Format"
summary: "BLV files store indoor geometry, spatial partitions, lighting, and gameplay records."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# BLV Indoor Map Format

BLV files store indoor geometry, spatial partitions, lighting, and gameplay records.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte file fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

---

## Overview

BLV (Building LeVel) files define indoor dungeon and building maps. They contain
geometry (vertices, faces), spatial partitioning (sectors, BSP nodes), environmental
data (decorations, lights), and gameplay data (spawn points, map outlines). The file
is stored within a LOD archive and loaded sequentially section by section.

**Source file reference:** BLV loader at `0x00498d93` (7,693 bytes)
**Map mode flag:** `DAT_006be1e0 == 1` indicates indoor mode

---

## 1. File Layout (Sequential Sections)

The BLV file is laid out as a flat sequence of count-prefixed arrays. The loader
at `0x00498d93` performs sequential reads:

1.  **Full Header (384 bytes):** Initial read into a large header structure.
2.  **Geometry Header (136 bytes):** Follow-up read into the geometry-specific header.

Each section is then read in order; there are no explicit section offsets in the header.

```text
Section   Contents                       Record Size    Notes
--------  -----------------------------  -------------  -------------------------
 1        Header                         136 bytes      Fixed-size header block
 2        Vertex count (u32)             4 bytes
          Vertex data                    6 bytes each   int16 X, Y, Z
 3        Face count (u32)               4 bytes
          Face data                      96 bytes each  Geometry + attributes
 4        Face aux data (flat int16[])   variable       Vertex IDs + UV coords
 5        Face texture names             10 chars each  One per face
 6        Face extras count (u32)        4 bytes
          Face extras                    36 bytes each  Additional face data
 7        Face extra texture names       10 chars each  One per extra
 8        Sector count (u32)             4 bytes
          Sector data                    116 bytes each Room definitions
 9        Sector data pool (flat u16[])  variable       Distributed to sectors
10        Sector light data (flat u16[]) variable       Distributed to sectors
11        Door count (u32)               4 bytes
          Door data                      variable       Door definitions
12        Decoration count (u32)         4 bytes
          Decoration data                32 bytes each  3D props and objects
13        Decoration names               32 chars each  One per decoration
14        Light count (u32)              4 bytes
          Light data                     16 bytes each  Light sources
15        BSP node count (u32)           4 bytes
          BSP node data                  8 bytes each   Binary space partitions
16        Spawn point count (u32)        4 bytes
          Spawn point data               variable       Party start positions
17        Map outline count (u32)        4 bytes
          Map outline data               12 bytes each  Automap line segments

```

---

## 2. BLV Header (136 bytes)

The header is read as a single 136-byte (0x88) block:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    4      version             Format version number
0x04    76     levelName[76]       Level display name (null-terminated)
0x50    16     skyTexture[16]      Sky texture name (null-terminated)
0x60    8      reserved1           Reserved / unused
0x68    4      faceDataSize        Total size of face auxiliary data section (bytes)
0x6C    4      sectorRDataSize     Total size of sector runtime data section (bytes)
0x70    4      sectorLRDataSize    Total size of sector light data section (bytes)
0x74    4      doorsDataSize       Total size of door data section (bytes)
0x78    16     reserved2           Reserved / padding (zeroed)

```

**Total: 0x88 = 136 bytes**

---

## 3. Vertices (6 bytes each)

Each vertex is a triplet of signed 16-bit integers:

```text
Offset  Size   Field    Description
------  -----  -------  ---------------------------
0x00    2      x        X coordinate (int16_t)
0x02    2      y        Y coordinate (int16_t)
0x04    2      z        Z coordinate (int16_t)

```

**Total: 6 bytes per vertex**

---

## 4. Faces (96 bytes each)

Faces are the primary geometric primitives for indoor maps. Each face contains two
plane representations: one in floating-point and one in fixed-point.

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    4      normalX_f           Plane normal X (float, IEEE 754)
0x04    4      normalY_f           Plane normal Y (float)
0x08    4      normalZ_f           Plane normal Z (float)
0x0C    4      distance_f          Plane distance (float)
0x10    4      normalX_i           Plane normal X (fixed-point, x65536)
0x14    4      normalY_i           Plane normal Y (fixed-point, x65536)
0x18    4      normalZ_i           Plane normal Z (fixed-point, x65536)
0x1C    4      distance_i          Plane distance (fixed-point, x65536)
0x20    4      zCalc1              Z-calculation coefficient 1
0x24    4      zCalc2              Z-calculation coefficient 2
0x28    4      zCalc3              Z-calculation coefficient 3
0x2C    4      attributes          Face attribute flags (uint32_t bitfield, see below)
0x30    4      pVertexIDs          Pointer to vertex index list (runtime)
0x34    4      pXInterceptDisp     Pointer to X intercepts (runtime)
0x38    4      pYInterceptDisp     Pointer to Y intercepts (runtime)
0x3C    4      pZInterceptDisp     Pointer to Z intercepts (runtime)
0x40    4      pVertexUIDs         Pointer to U texture coords (runtime)
0x44    4      pVertexVIDs         Pointer to V texture coords (runtime)
0x48    2      faceExtraID         Index into face extras array
0x4A    2      bitmapID            Texture bitmap index
0x4C    2      sectorID            Sector this face belongs to
0x4E    2      backSectorID        Adjacent sector (for portals)
0x50    2      bboxMinX            Bounding box minimum X
0x52    2      bboxMaxX            Bounding box maximum X
0x54    2      bboxMinY            Bounding box minimum Y
0x56    2      bboxMaxY            Bounding box maximum Y
0x58    2      bboxMinZ            Bounding box minimum Z
0x5A    2      bboxMaxZ            Bounding box maximum Z
0x5C    1      polygonType         Polygon type / orientation
0x5D    1      numVertices         Vertex count (3-20)
0x5E    2      padding             Alignment padding

```

**Total: 0x60 = 96 bytes per face**

### Face Attribute Flags (offset 0x2C, uint32_t)

| Bit   | Hex      | Name             | Meaning                                    |
|-------|----------|------------------|--------------------------------------------|
| 0     | 0x0001   | Portal           | Sector boundary / doorway                  |
| 1     | 0x0002   | CanSaturate      | Face can receive saturation effects        |
| 2     | 0x0004   | Floor            | This is a floor face                       |
| 3     | 0x0008   | Water            | Water surface                              |
| 4     | 0x0010   | Lava             | Lava surface                               |
| 5     | 0x0020   | Specular         | Specular (shiny) reflection                |
| 6     | 0x0040   | CannotPickup     | Items on this face cannot be picked up     |
| 7     | 0x0080   | SecretDoor       | Secret door                                |
| 8     | 0x0100   | Invisible        | Face is not rendered                       |
| 9     | 0x0200   | Animated         | Animated texture                           |
| 10    | 0x0400   | Event            | Triggers event on interaction              |
| 11    | 0x0800   | TriggerEvent     | Triggers event on step/proximity           |
| 12    | 0x1000   | Outdoor          | Outdoor face                               |
| 13    | 0x2000   | HasPulsingLight  | Pulsing light effect                       |
| 14    | 0x4000   | IsBitmap         | Uses bitmap texture                        |
| 15    | 0x8000   | Indoor           | Indoor face                                |
| 16    | 0x10000  | Ceiling          | Ceiling face                               |
| 17    | 0x20000  | Clickable        | Can be clicked / interacted with           |
| 18    | 0x40000  | Pressure         | Pressure plate / trigger floor             |
| 19    | 0x80000  | Ethereal         | Pass-through / ghostly face                |

### Plane Representations

The face stores the plane equation `Nx*x + Ny*y + Nz*z + D = 0` in two forms:

- **Float plane** (offsets 0x00-0x0C): IEEE 754 single-precision, used for rendering
- **Fixed-point plane** (offsets 0x10-0x1C): Scaled by 65,536 (x << 16), used for
  collision detection and BSP operations

---

## 5. Face Auxiliary Data (flat int16 array)

After all face records, the face auxiliary data is stored as a single flat array of
`int16_t` values. The total byte size is given by `header.faceDataSize`.

For each face with `N` vertices, the data is distributed sequentially:

```text
Per face: 6 * (N + 1) entries of int16_t

  [vertexIDs: N+1]      Vertex indices (last is repeat of first for closure)
  [xIntercepts: N+1]    X-axis intercept values
  [yIntercepts: N+1]    Y-axis intercept values
  [zIntercepts: N+1]    Z-axis intercept values
  [uCoords: N+1]        U texture coordinates
  [vCoords: N+1]        V texture coordinates

```

The runtime pointer fields in each face struct (offsets 0x30-0x44) are populated by
pointing into this flat buffer at the appropriate offsets during load.

---

## 6. Face Texture Names (10 bytes each)

One 10-character texture name per face (null-padded), resolved to a texture handle via
the BITMAPS.LOD icon lookup at load time.

---

## 7. Face Extras (36 bytes each)

Additional per-face data for faces that require extended properties:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    2      flags               Extra attribute flags
0x02    2      faceIndex           Index of the parent face
0x04    2      textureAnimId       Animated texture frame ID
0x06    2      field_06            Unknown
0x08    2      field_08            Unknown
0x0A    2      field_0A            Unknown
0x0C    2      eventId             Event trigger ID
0x0E    2      field_0E            Unknown
0x10    2      field_10            Unknown
0x12    2      field_12            Unknown
0x14    8      reserved            Reserved
0x1C    4      field_1C            Unknown
0x20    4      field_20            Unknown

```

**Total: 0x24 = 36 bytes per extra**

---

## 8. Sectors (116 bytes each)

Sectors define connected rooms or regions within the indoor map. Each sector has lists
of floor faces, wall faces, ceiling faces, and portal faces that are distributed from
flat data pools.

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    4      numFloorFaces       Count of floor face indices
0x04    4      floorFacesPtr       Pointer to floor face index list (runtime)
0x08    4      numWallFaces        Count of wall face indices
0x0C    4      wallFacesPtr        Pointer to wall face index list (runtime)
0x10    4      numCeilingFaces     Count of ceiling face indices
0x14    4      ceilingFacesPtr     Pointer to ceiling face index list (runtime)
0x18    4      numPortalFaces      Count of portal face indices
0x1C    4      portalFacesPtr      Pointer to portal face index list (runtime)
0x20    4      numFluids           Fluid face count
0x24    4      fluidFacesPtr       Fluid face list pointer (runtime)
0x28    4      numCogFaces         Interactive face count
0x2C    4      cogFacesPtr         Interactive face list pointer (runtime)
0x30    4      numDecorations      Decoration count in sector
0x34    4      decorationsPtr      Decoration list pointer (runtime)
0x38    4      numMarkers          Marker count
0x3C    4      markersPtr          Marker list pointer (runtime)
0x40    4      numLights           Light count in sector
0x44    4      lightsPtr           Light list pointer (runtime)
0x48    2      waterLevel          Water surface height
0x4A    2      mistLevel           Mist/fog level
0x4C    4      lightLevel          Ambient light level
0x50    4      field_50            Unknown
0x54    4      minBBoxX            Bounding box min X
0x58    4      maxBBoxX            Bounding box max X
0x5C    4      minBBoxY            Bounding box min Y
0x60    4      maxBBoxY            Bounding box max Y
0x64    4      minBBoxZ            Bounding box min Z
0x68    4      maxBBoxZ            Bounding box max Z
0x6C    4      field_6C            Unknown (exit bitmap)
0x70    4      field_70            Unknown

```

**Total: 0x74 = 116 bytes per sector**

### Sector Data Distribution

The sector data pool (`sectorRDataSize` bytes) is a flat `uint16_t` array. Each
sector's count fields determine how many entries to consume from the pool. At load
time, the engine iterates through sectors and assigns runtime pointers into this flat
buffer based on each sector's count fields.

Similarly, the sector light data pool (`sectorLRDataSize` bytes) is distributed among
sectors using the light count fields.

---

## 9. Decorations (32 bytes each)

Static 3D props (torches, barrels, plants, etc.) placed within the map:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    2      nameIndex           Decoration list name index
0x02    2      flags               Decoration flags
0x04    4      posX                X position (world coordinates)
0x08    4      posY                Y position (world coordinates)
0x0C    4      posZ                Z position (world coordinates)
0x10    2      direction           Facing direction (0-2047)
0x12    2      sectorIndex         Sector containing this decoration
0x14    2      field_14            Unknown
0x16    2      field_16            Unknown
0x18    4      field_18            Unknown
0x1C    4      eventId             Associated event trigger ID

```

**Total: 0x20 = 32 bytes per decoration**

Decoration names are stored separately as 32-character strings, one per decoration.

---

## 10. Lights (16 bytes each)

Static light sources placed within the map:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    4      posX                X position
0x04    4      posY                Y position
0x08    4      posZ                Z position
0x0C    2      radius              Light radius / range
0x0E    1      red                 Light color red component
0x0F    1      green               Light color green component

```

**Total: 0x10 = 16 bytes per light**

---

## 11. BSP Nodes (8 bytes each)

Binary Space Partition nodes for indoor visibility and collision:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    2      frontChild          Index of front child node (-1 = leaf)
0x02    2      backChild           Index of back child node (-1 = leaf)
0x04    2      coplanarFace        Index of the splitting face
0x06    2      padding             Alignment padding

```

**Total: 0x08 = 8 bytes per node**

Note: For save game serialization, BSP node data uses an expanded 836-byte (0x344)
representation that includes runtime state.

---

## 12. Spawn Points

Spawn point markers define where the party can appear when entering the map. Each map
can have multiple named entry points:

| Index | Name            | Usage                                |
|-------|-----------------|--------------------------------------|
| 0     | `"Party Start"` | Default entry position               |
| 1     | `"North Start"` | Entry from north-connected map       |
| 2     | `"South Start"` | Entry from south-connected map       |
| 3     | `"East Start"`  | Entry from east-connected map        |
| 4     | `"West Start"`  | Entry from west-connected map        |

The function at `0x004498f8` (372 bytes) searches for the named spawn point and sets
the party position (`DAT_00acd4ec` X, `DAT_00acd4f0` Y, `DAT_00acd4f4` Z) from the
marker's coordinates.

The generic `"Spawn"` string (at `004ec270`) is referenced during both BLV and ODM
loading for monster/NPC spawn point processing.

---

## 13. Map Outlines (12 bytes each)

Automap line segments drawn on the in-game map screen:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    2      vertex1             Start vertex index
0x02    2      vertex2             End vertex index
0x04    2      face1               Adjacent face index 1
0x06    2      face2               Adjacent face index 2
0x08    2      flags               Visibility / discovered flags
0x0A    2      padding             Alignment padding

```

**Total: 0x0C = 12 bytes per outline**

---

## 14. Known BLV Maps

Maps referenced in the binary:

| Filename   | Context / Notes                          |
|------------|------------------------------------------|
| `d05.blv`  | Frequently referenced (save/render code) |
| `d10.blv`  | Level transition target                  |
| `d11.blv`  | Level transition target                  |
| `d18.blv`  | Special map handler                      |
| `d23.blv`  | Memory check during load                 |
| `d25.blv`  | Referenced in game logic                 |
| `d26.blv`  | Referenced in game logic                 |
| `d29.blv`  | UI-related reference                     |
| `d37.blv`  | Monster placement reference              |
| `d47.blv`  | Rendering pipeline reference             |
| `mdk01.blv`| Special dungeon                          |
| `mdr01.blv`| Special dungeon                          |
| `mdt*.blv` | Template dungeons (01,09,10,11,12,14,15) |
| `nwc.blv`  | Developer room / easter egg              |

---

## 15. Error Messages

| Error String                                    | Condition                    |
|-------------------------------------------------|------------------------------|
| `"File %s is not a BLV File"`                   | Invalid BLV header           |
| `"Unable to open %s"`                           | File not found               |
| `"Attempt to open new level before..."`         | Invalid state for level load |
| `"Out of memory loading indoor level"`          | Allocation failure           |
| `"Unable to find %s in Games.LOD"`              | Map not in archive           |
| `"Door Error\ndoor id: %i\nfacet no: %i\n..."` | Invalid door geometry        |

---

## Integration notes

- Read the file strictly sequentially; section offsets are implicit
- The face data flat array must be distributed per-face using vertex counts
- Sector data pools are similarly distributed using per-sector count fields
- Runtime pointer fields (in faces and sectors) must be populated post-load
- Fixed-point normals use a 65,536 scale factor (16-bit fractional precision)
- The file filter dialog string `"Indoor BLV Files (*.blv)"` confirms the extension
- Indoor rendering uses the `"Lightpoly builder native indoor"` lighting system
