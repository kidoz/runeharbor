// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"
#include "blv_map.hpp" // For ParsedFace (shared structure)

namespace runeharbor::formats
{

/**
 * ODM (Outdoor Map) Format
 *
 * ODM files contain outdoor world map geometry for MM6/7/8.
 * Structure (MM7):
 *   1. Header (176 bytes)
 *   2. Heightmap (128x128 uint8, height = value * 32)
 *   3. Tilemap (128x128 uint8, tile texture indices)
 *   4. Attribute map (128x128 uint8, walkability flags)
 *   5. Terrain normals (variable, uint32 count + CMAP data + normals)
 *   6. BSP models (buildings) - count + 188-byte headers + per-model data
 *   7. Sprites, IDList, OMAP, Spawns
 */

#pragma pack(push, 1)

// ODM header (176 bytes)
// Layout: Name(32) + FileName(32) + VersionStr(32) + Unused1(32) + Unused2(32) + Tilesets(16)
struct ODMHeader
{
    char levelName[32];   // Null-terminated level name
    char fileName[32];    // ODM file name
    char versionStr[32];  // Version string (31 chars + 1 byte tileset file flag)
    char unused1[32];     // Reserved
    char unused2[32];     // Reserved
    uint8_t tilesets[16]; // 4 tilesets, 4 bytes each (index + setId pairs)
};
static_assert(sizeof(ODMHeader) == 176, "ODMHeader must be 176 bytes");

// BSP model (building) header - 188 bytes (0xBC)
struct BSPModelHeader
{
    char name[32];          // 0x00: Model name
    char name2[32];         // 0x20: Secondary name
    int32_t flags;          // 0x40: Flags (ShowOnMap bit)
    uint32_t numVertices;   // 0x44: Vertex count
    int32_t pVertices;      // 0x48: Runtime pointer (ignored on disk)
    uint32_t numFaces;      // 0x4C: Face count
    int16_t numConvexFaces; // 0x50: Convex face subset count
    int16_t pad_52;         // 0x52: Padding
    int32_t pFaces;         // 0x54: Runtime pointer (ignored)
    int32_t pOrdering;      // 0x58: Runtime pointer (ignored)
    int32_t numNodes;       // 0x5C: BSP node count
    int32_t pNodes;         // 0x60: Runtime pointer (ignored)
    int32_t numDecorations; // 0x64: Decoration count
    int32_t gridX;          // 0x68: Grid position X
    int32_t gridY;          // 0x6C: Grid position Y
    int32_t posX;           // 0x70: World position X
    int32_t posY;           // 0x74: World position Y
    int32_t posZ;           // 0x78: World position Z
    int32_t minX;           // 0x7C: Bounding box min X
    int32_t minY;           // 0x80: Bounding box min Y
    int32_t minZ;           // 0x84: Bounding box min Z
    int32_t maxX;           // 0x88: Bounding box max X
    int32_t maxY;           // 0x8C: Bounding box max Y
    int32_t maxZ;           // 0x90: Bounding box max Z
    int32_t bfMinX;         // 0x94: Alt bounds min X
    int32_t bfMinY;         // 0x98: Alt bounds min Y
    int32_t bfMinZ;         // 0x9C: Alt bounds min Z
    int32_t bfMaxX;         // 0xA0: Alt bounds max X
    int32_t bfMaxY;         // 0xA4: Alt bounds max Y
    int32_t bfMaxZ;         // 0xA8: Alt bounds max Z
    int32_t centerX;        // 0xAC: Bounding sphere center X
    int32_t centerY;        // 0xB0: Bounding sphere center Y
    int32_t centerZ;        // 0xB4: Bounding sphere center Z
    int32_t boundingRadius; // 0xB8: Bounding sphere radius
};
static_assert(sizeof(BSPModelHeader) == 188, "BSPModelHeader must be 188 bytes");

// ODM face on disk - 308 bytes (0x134)
// Unlike BLV faces which store per-vertex data externally,
// ODM faces store all per-vertex arrays inline (up to 20 vertices).
struct ODMFaceOnDisk
{
    // Fixed-point plane equation (normal / 65536)
    int32_t normalX;    // 0x00
    int32_t normalY;    // 0x04
    int32_t normalZ;    // 0x08
    int32_t normalDist; // 0x0C

    // Z calculation cache
    int32_t zCalc1; // 0x10
    int32_t zCalc2; // 0x14
    int32_t zCalc3; // 0x18

    // Face attributes
    uint32_t attributes; // 0x1C

    // Per-vertex arrays (6 arrays x 20 entries x 2 bytes = 240 bytes)
    int16_t vertexIds[20];      // 0x20: Indices into model vertex array
    int16_t textureUs[20];      // 0x48: Texture U coordinates
    int16_t textureVs[20];      // 0x70: Texture V coordinates
    int16_t xInterceptDisp[20]; // 0x98: X intercept displacements
    int16_t yInterceptDisp[20]; // 0xC0: Y intercept displacements
    int16_t zInterceptDisp[20]; // 0xE8: Z intercept displacements

    // Texture reference
    int16_t bitmapId;      // 0x110
    int16_t textureDeltaU; // 0x112
    int16_t textureDeltaV; // 0x114

    // Bounding box
    int16_t bboxMinX; // 0x116
    int16_t bboxMaxX; // 0x118
    int16_t bboxMinY; // 0x11A
    int16_t bboxMaxY; // 0x11C
    int16_t bboxMinZ; // 0x11E
    int16_t bboxMaxZ; // 0x120

    // Events and triggers
    int16_t cogNumber;        // 0x122
    int16_t eventId;          // 0x124
    int16_t eventTriggerType; // 0x126
    int16_t reserved;         // 0x128

    // Gradient and vertex metadata
    uint8_t gradientVertex1; // 0x12A
    uint8_t gradientVertex2; // 0x12B
    uint8_t gradientVertex3; // 0x12C
    uint8_t gradientVertex4; // 0x12D
    uint8_t numVertices;     // 0x12E: Actual vertex count (3-20)
    uint8_t polygonType;     // 0x12F: PolygonType enum

    // Shade/visibility
    uint8_t shadeType; // 0x130
    uint8_t visible;   // 0x131
    uint8_t pad[2];    // 0x132-0x133
};
static_assert(sizeof(ODMFaceOnDisk) == 0x134, "ODMFaceOnDisk must be 308 bytes");

// Outdoor vertex - 12 bytes (3 x int32, unlike BLV's 3 x int16)
struct ODMVertex3D
{
    int32_t x;
    int32_t y;
    int32_t z;
};
static_assert(sizeof(ODMVertex3D) == 12, "ODMVertex3D must be 12 bytes");

// Spawn point
struct ODMSpawnPoint
{
    int32_t x;
    int32_t y;
    int32_t z;
    uint16_t radius;
    uint16_t objectType;
    uint16_t objectIndex;
    uint16_t attributes;
    int16_t group;
};

#pragma pack(pop)

// Parsed building with resolved geometry
struct ParsedBuilding
{
    std::string name;
    std::vector<ODMVertex3D> vertices; // Model-local vertices (int32)
    std::vector<ParsedFace> faces;     // Faces referencing local vertices
    int32_t worldX = 0;                // World position
    int32_t worldY = 0;
    int32_t worldZ = 0;

    // Bounding box (int32 for outdoor coordinates)
    int32_t minX = 0, maxX = 0;
    int32_t minY = 0, maxY = 0;
    int32_t minZ = 0, maxZ = 0;
};

// Heightmap cell data
struct HeightmapCell
{
    int16_t height = 0;     // Terrain height (world units)
    uint8_t tileIndex = 0;  // Index into tile texture array
    uint8_t attributes = 0; // Water, walkable, etc.
};

// Parsed outdoor map data
struct ODMMapData
{
    // Header info
    std::string levelName;
    std::string skyTexture;
    std::string groundTexture;

    // Fog settings
    int32_t fogMinDist = 0;
    int32_t fogMaxDist = 0;
    uint8_t fogR = 0, fogG = 0, fogB = 0;

    // Terrain data (128x128 grid)
    static constexpr int TERRAIN_SIZE = 128;
    std::vector<HeightmapCell> heightmap; // [y * TERRAIN_SIZE + x]

    // Tile textures
    std::vector<std::string> tileTextures;

    // Building geometry
    std::vector<ParsedBuilding> buildings;

    // Spawns
    std::vector<ODMSpawnPoint> spawns;

    // Statistics
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t buildingCount = 0;
};

class ODMMap
{
  public:
    using ProgressCallback = std::function<void(float)>;

    explicit ODMMap(util::ILogger& logger);

    // Parse from raw decompressed ODM data
    bool parse(const std::vector<uint8_t>& data, ProgressCallback progress = {});

    // Access parsed data
    const ODMMapData& getData() const { return mapData; }

    // Quick accessors
    const std::string& getLevelName() const { return mapData.levelName; }
    const std::string& getSkyTexture() const { return mapData.skyTexture; }
    const std::vector<HeightmapCell>& getHeightmap() const { return mapData.heightmap; }
    const std::vector<ParsedBuilding>& getBuildings() const { return mapData.buildings; }

    // Get terrain height at grid position (0-127, 0-127)
    int16_t getHeightAt(int x, int y) const;

    // Get terrain height interpolated at world position
    float getHeightAtWorld(float worldX, float worldY) const;

  private:
    bool parseHeader(const std::vector<uint8_t>& data);
    bool parseHeightmap(const std::vector<uint8_t>& data, size_t& offset);
    bool skipTerrainNormals(const std::vector<uint8_t>& data, size_t& offset);
    bool parseBuildings(const std::vector<uint8_t>& data, size_t& offset);
    bool parseSpawns(const std::vector<uint8_t>& data, size_t& offset);

    ParsedFace convertFace(const ODMFaceOnDisk& diskFace, const std::string& texName) const;
    std::string extractString(const char* data, size_t maxLen) const;

    util::ILogger& logger;
    ODMMapData mapData;
    ProgressCallback progressCallback;

    void reportProgress(float value);
};

} // namespace runeharbor::formats
