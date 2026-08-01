---
title: "ODM Outdoor Map Format"
summary: "ODM files store terrain, models, decorations, and spawn data for outdoor maps."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# ODM Outdoor Map Format

ODM files store terrain, models, decorations, and spawn data for outdoor maps.

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

ODM (OutDoor Map) files define outdoor terrain maps with height fields, tile maps,
BSP models (buildings), decorations, and spawn points. The format originates from the
MM6 engine (format string `"MM6 Outdoor v1.00"`) and is reused in MM7 with minimal
changes. Maps are stored within LOD archives.

**Source file reference:** ODM loader at `0x0047d0aa` (7,195 bytes), init at
`0x0047cde6`, reset at `0x0047cfa0`
**Map mode flag:** `DAT_006be1e0 == 2` indicates outdoor mode

---

## 1. ODM Structure Initialization

The loader at `0x0047d0aa` reads a **384-byte** header block followed by a
**176-byte** block sequentially.

### Sky Texture Generation
The engine dynamically generates sky texture names using:

- `plansky%d` (formatted with a sky ID)
- `plansky3` (fallback/alternative)
- `grastyl` (ground texture string used during load)

```text
Offset  Size   Field               Default Value
------  -----  ------------------  ---------------------------------
0x00    32     mapName[32]         "blank"
0x20    32     formatId[32]        "i6.odm"
0x40    32     versionString[32]   "MM6 Outdoor v1.00"
0x60    32     skyTexture[32]      (empty)
0x80    32     groundTexture[32]   (empty)
0xA0    2      field1              0
0xA4    2      field2              5
0xA8    2      field3              6
0xAC    2      field4              10
0xB0    4      modelCount          0
0xD4    4      heightMapPtr        -> allocated 0x8000 (32,768) bytes
0xD8    4      attributeMapPtr     (allocated)
0xDC    4      normalCount         0
0xE0    4      idListPtr           -> allocated 2 bytes ("IDLIST")
0xE4    4      tileMapPtr          -> allocated 0x10000 (65,536) bytes, zeroed
0xE8    4      skyTextureHandle    0
0xEC    4      groundTextureHandle 0

```

The reset function (`0x0047cfa0`) restores the version string to
`"MM6 Outdoor v1.00"` and the format ID to `"default.odm"`.

---

## 2. File Layout (Sequential Sections)

Like BLV files, ODM data is read sequentially. The exact section ordering from the
7,195-byte loader:

```cpp
Section   Contents                       Size/Record     Notes
--------  -----------------------------  --------------  -------------------------
 1        Header block                   variable        Map name, format, version
 2        Height map                     32,768 bytes    128x128 grid of uint16
 3        Tile map                       65,536 bytes    128x128 x 4 bytes per tile
 4        Attribute map                  variable        Per-tile attributes
 5        BSP model count (u32)          4 bytes
          BSP model data                 variable        Building geometry
 6        Decoration count (u32)         4 bytes
          Decoration data                variable        Outdoor props
 7        Spawn point data               variable        "Spawn" markers
 8        Additional map data            variable        Monsters, objects, etc.

```

---

## 3. Height Map (32,768 bytes)

The outdoor terrain is a 128x128 grid. Each cell stores a height value:

```cpp
Total size: 0x8000 = 32,768 bytes
Grid:       128 x 128 cells
Cell size:  2 bytes (uint16_t height value)

Height = heightMap[y * 128 + x]

```

The height map is allocated and loaded in a single read of 0x8000 bytes. Height
values represent terrain elevation at each grid intersection point. Between
intersections, height is bilinearly interpolated for smooth terrain rendering.

---

## 4. Tile Map (65,536 bytes)

The tile map defines the terrain texture for each grid cell:

```text
Total size: 0x10000 = 65,536 bytes
Grid:       128 x 128 cells
Cell size:  4 bytes per tile entry

TileEntry = tileMap[y * 128 + x]

```

Each tile entry encodes the texture index and tile attributes (water, road, etc.).
The tile table (`dtile.bin` or `tile.def`) defines the mapping from tile IDs to
texture names and terrain type attributes.

### Tile Attributes

Tiles are loaded from `dtile.bin` (binary) or `tile.def` (text). The tile table
parser at `0x0048802d` recognizes these attribute strings:

| Attribute String     | Meaning                      |
|----------------------|------------------------------|
| `TTattr_Transition`  | Terrain transition tile      |
| `TTsect_Start`       | Sector start marker          |
| `TTtype_Start`       | Type classification start    |
| `Friendly Terrain`   | Non-hostile terrain type     |

---

## 5. BSP Models (Buildings)

Outdoor buildings and structures are represented as BSP (Binary Space Partition)
models. Each model contains vertices, faces, and a BSP tree for rendering.

### Outdoor Face Structure (308 bytes)

Outdoor faces are significantly larger than indoor faces because they store per-vertex
arrays inline (up to 20 vertices), rather than referencing a separate flat data pool:

```text
Size: 0x134 = 308 bytes per face

Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    4      normalX             Plane normal X (fixed-point, x65536)
0x04    4      normalY             Plane normal Y (fixed-point, x65536)
0x08    4      normalZ             Plane normal Z (fixed-point, x65536)
0x0C    4      normalDist          Plane distance (fixed-point, x65536)
0x10    4      zCalc1              Z-calculation coefficient 1
0x14    4      zCalc2              Z-calculation coefficient 2
0x18    4      zCalc3              Z-calculation coefficient 3
0x1C    4      attributes          Face attribute flags (bitfield)
0x20    40     vertexIds[20]       Vertex indices (int16 × 20)
0x48    40     textureUs[20]       Texture U coordinates (int16 × 20)
0x70    40     textureVs[20]       Texture V coordinates (int16 × 20)
0x98    40     xInterceptDisp[20]  X intercept displacements (int16 × 20)
0xC0    40     yInterceptDisp[20]  Y intercept displacements (int16 × 20)
0xE8    40     zInterceptDisp[20]  Z intercept displacements (int16 × 20)
0x110   2      bitmapId            Texture reference index
0x112   2      textureDeltaU       Texture offset U
0x114   2      textureDeltaV       Texture offset V
0x116   12     bbox[6]             Bounding box (minX, maxX, minY, maxY, minZ, maxZ)
0x122   2      cogNumber           COG interaction number
0x124   2      eventId             Event trigger ID
0x126   2      eventTriggerType    Event trigger type
0x128   2      reserved            Reserved
0x12A   4      gradientVertices    Gradient vertex indices (4 × uint8)
0x12E   1      numVertices         Actual vertex count (3-20)
0x12F   1      polygonType         Polygon type / orientation
0x130   1      shadeType           Shade type
0x131   1      visible             Visibility flag
0x132   2      padding             Alignment padding

```

**Key difference from indoor faces (96 bytes):** ODM faces embed all 6 per-vertex
arrays (20 entries each = 240 bytes) directly in the struct, making them self-contained.
Indoor (BLV) faces instead store pointers into a shared flat data pool.

### Model Data

Each BSP model in the outdoor map contains:

- Vertex list (3D coordinates)
- Face list (0x134 bytes each)
- BSP tree nodes for the model
- Bounding box
- Position and orientation in world space

---

## 6. Outdoor Decorations

Decorations in outdoor maps (trees, rocks, signs, etc.) follow a similar pattern to
indoor decorations but with world-space coordinates. The decoration list references
entries from `ddeclist.bin` / `declist.txt`.

---

## 7. Spawn Points

Outdoor maps use the same spawn point system as indoor maps:

| Name            | Usage                                      |
|-----------------|--------------------------------------------|
| `"Party Start"` | Default entry when loading this map        |
| `"North Start"` | Entry from the map to the north            |
| `"South Start"` | Entry from the map to the south            |
| `"East Start"`  | Entry from the map to the east             |
| `"West Start"`  | Entry from the map to the west             |

The `"Spawn"` string (at `004ec270`) is used during ODM loading for processing
monster and NPC spawn markers.

---

## 8. Terrain Rendering Parameters

Outdoor rendering uses configurable LOD (Level of Detail) band distances read from
the INI file:

| Parameter          | Default | Description                       |
|--------------------|---------|-----------------------------------|
| `gridband1`        | 10      | Near terrain detail distance      |
| `gridband2`        | 15      | Medium terrain detail distance    |
| `gridband3`        | 25      | Far terrain detail distance       |
| `ter_gamma`        | 0       | Terrain gamma correction          |
| `bld_gamma`        | 0       | Building gamma correction         |
| `terrain_subdivpow2` | (var) | Terrain subdivision power of 2    |
| `terrain_subdivsize` | (var) | Terrain subdivision cell size     |

### Sky and Atmosphere

Day/night sky colors are stored as RGB triplets:

| Parameter          | Default RGB   | Description              |
|--------------------|---------------|--------------------------|
| `RGBDayTop`        | 81, 121, 236  | Daytime sky top color    |
| `RGBDayBottom`     | 153, 193, 237 | Daytime sky bottom color |
| `RGBNightTop`      | 0, 0, 0       | Nighttime sky top color  |
| `RGBNightBottom`   | 11, 41, 129   | Nighttime sky bottom color |

---

## 9. Known ODM Maps

Maps referenced in the binary:

| Filename      | Context / Notes                            |
|---------------|--------------------------------------------|
| `out01.odm`   | Emerald Island (default start map)         |
| `out02.odm`   | Referenced in gameplay loop                |
| `out09.odm`   | Special map handler                        |
| `out14.odm`   | Map generation reference                   |
| `out15.odm`   | Multiple special-case checks in code       |
| `i6.odm`      | Compatibility format identifier            |
| `default.odm` | Reset/fallback map name                    |

The full range is `out01.odm` through `out15.odm`, with the format string
`"out%02d.odm"` used for programmatic map generation/reference.

---

## 10. Map Globals

| Address          | Type   | Purpose                            |
|------------------|--------|------------------------------------|
| `DAT_006be1c4`   | char[] | Current level filename             |
| `DAT_006bdf94`   | char[] | Sky texture name (outdoor)         |
| `DAT_006bdfa4`   | char[] | Ground texture name (outdoor)      |
| `DAT_006bdf44`   | i32    | No sky flag                        |
| `DAT_006bdf48`   | i32    | No decorations flag                |
| `DAT_006bdf54`   | i32    | No wavy water flag                 |
| `DAT_006bdf60`   | i32    | Terrain gamma                      |
| `DAT_006bdf5c`   | i32    | Building gamma                     |
| `DAT_006bdf7c-84`| int[3] | Grid band distances (10, 15, 25)   |
| `DAT_006bdf88-93`| u8[] | RGB day/night sky colors           |
| `DAT_006bdfbc`   | i32    | Current map index in MapStats      |
| `DAT_00576ea8`   | i32    | Outdoor map texture handle         |

---

## 11. MapStats.txt Reference

Map metadata is loaded from `MapStats.txt` (77 maps, 0x44 = 68 bytes per entry):

| Column | Offset  | Type       | Purpose                    |
|--------|---------|------------|----------------------------|
| 1      | +0x00   | string ptr | Map display name           |
| 2      | +0x04   | string ptr | Map filename (.blv / .odm) |
| 3      | +0x14   | i32        | Map attribute 1            |
| 4      | +0x18   | i32        | Map attribute 2            |
| 5      | +0x28   | i32        | Map attribute 3            |
| 11     | +0x2F   | u8       | Reverb type (for audio)    |

---

## 12. Error Messages

| Error String                            | Condition                      |
|-----------------------------------------|--------------------------------|
| `"Unable to find %s in Games.LOD"`      | Map not found in archive       |
| `"Can't load file!"`                    | Invalid file header            |
| `"MM6 Outdoor v1.11"`                   | Alternate version string       |

---

## Integration notes

- The format string `"MM6 Outdoor v1.00"` confirms cross-compatibility with MM6
- Height map is a flat 128x128 grid of 16-bit values (32KB total)
- Tile map is a flat 128x128 grid of 32-bit entries (64KB total)
- Outdoor faces at 308 bytes (0x134) are over triple the size of indoor faces (96 bytes)
  because they embed per-vertex arrays inline (6 arrays × 20 entries × 2 bytes = 240 bytes)
- Terrain LOD bands control detail distance for rendering optimization
- The `"nosky"`, `"nowavywater"`, and `"noMist"` INI flags disable specific effects
- Day/night cycle interpolates between the two sky color sets
- Maps are identified by extension: `.odm` = outdoor, `.blv` = indoor
