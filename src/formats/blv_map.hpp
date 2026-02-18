// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

/**
 * BLV (Building Level Volume) Indoor Map Format
 *
 * BLV files contain indoor/dungeon map geometry for MM6/7/8.
 * File layout (MM7):
 *   1. BLVHeader (136 bytes)
 *   2. Vertices (count-prefixed, 6 bytes each)
 *   3. Faces (count-prefixed, 96 bytes each)
 *   4. Face data (flat int16 array, header.faceDataSize bytes)
 *   5. Face textures (faceCount * 10 chars)
 *   6. Face extras (count-prefixed, 36 bytes each)
 *   7. Face extra textures (faceExtraCount * 10 chars)
 *   8. Sectors (count-prefixed, 116 bytes each)
 *   9. Sector data (flat uint16 array, header.sectorRDataSize bytes)
 *  10. Sector light data (flat uint16 array, header.sectorLRDataSize bytes)
 *  11. Door count (uint32)
 *  12. Decorations (count-prefixed, 32 bytes each) + names
 *  14. Lights (count-prefixed, 16 bytes each)
 *  15. BSP nodes, spawn points, map outlines
 */

#pragma pack(push, 1)

// BLV header (136 bytes = 0x88)
struct BLVHeader
{
    uint32_t version;          // 0x00: Format version (usually 1)
    char levelName[76];        // 0x04: Null-terminated level name
    char skyTexture[16];       // 0x50: Null-terminated sky texture name
    uint8_t reserved_60[8];    // 0x60: Reserved
    uint32_t faceDataSize;     // 0x68: Size of face vertex data section (bytes)
    uint32_t sectorRDataSize;  // 0x6C: Size of sector regular data (bytes)
    uint32_t sectorLRDataSize; // 0x70: Size of sector light/render data (bytes)
    uint32_t doorsDataSize;    // 0x74: Size of door data section (bytes)
    uint8_t reserved_78[16];   // 0x78: Reserved
};
static_assert(sizeof(BLVHeader) == 136, "BLVHeader must be 136 bytes");

// Vertex position (6 bytes)
struct BLVVertex
{
    int16_t x;
    int16_t y;
    int16_t z;
};
static_assert(sizeof(BLVVertex) == 6);

// On-disk face structure (96 bytes = 0x60)
// Field offsets verified against original binary and OpenEnroth.
struct BLVFaceOnDisk
{
    // Float plane (Planef_MM7) - IEEE 754 floats
    float facePlaneNX;   // 0x00: Normal X
    float facePlaneNY;   // 0x04: Normal Y
    float facePlaneNZ;   // 0x08: Normal Z
    float facePlaneDist; // 0x0C: Distance from origin
    // Fixed-point integer plane (Planei_MM7, legacy)
    int32_t facePlaneOldNX;   // 0x10: Normal X (x 65536)
    int32_t facePlaneOldNY;   // 0x14: Normal Y (x 65536)
    int32_t facePlaneOldNZ;   // 0x18: Normal Z (x 65536)
    int32_t facePlaneOldDist; // 0x1C: Distance (fixed-point)
    // Z calculation coefficients
    int32_t zCalc1; // 0x20
    int32_t zCalc2; // 0x24
    int32_t zCalc3; // 0x28
    // Attributes
    uint32_t attributes; // 0x2C: Face attribute flags
    // Runtime pointers (garbage on disk, filled at load time)
    int32_t pVertexIDs;      // 0x30
    int32_t pXInterceptDisp; // 0x34
    int32_t pYInterceptDisp; // 0x38
    int32_t pZInterceptDisp; // 0x3C
    int32_t pVertexUIDs;     // 0x40
    int32_t pVertexVIDs;     // 0x44
    // Face references
    uint16_t faceExtraID; // 0x48: Index into face extras
    uint16_t bitmapID;    // 0x4A: Texture bitmap index
    uint16_t sectorID;    // 0x4C: Sector this face belongs to
    int16_t backSectorID; // 0x4E: Adjacent sector (portals)
    // Bounding box
    int16_t bboxMinX; // 0x50
    int16_t bboxMaxX; // 0x52
    int16_t bboxMinY; // 0x54
    int16_t bboxMaxY; // 0x56
    int16_t bboxMinZ; // 0x58
    int16_t bboxMaxZ; // 0x5A
    // Type and vertex count
    uint8_t polygonType; // 0x5C: Polygon type
    uint8_t numVertices; // 0x5D: Number of vertices (3-20)
    int16_t pad_5E;      // 0x5E: Padding
};
static_assert(sizeof(BLVFaceOnDisk) == 96, "BLVFaceOnDisk must be 96 bytes");

// Face extra data on disk (36 bytes = 0x24)
struct BLVFaceExtraOnDisk
{
    int16_t field_0;             // 0x00
    int16_t field_2;             // 0x02
    int16_t field_4;             // 0x04
    int16_t field_6;             // 0x06
    int16_t field_8;             // 0x08
    int16_t field_A;             // 0x0A
    int16_t faceId;              // 0x0C: Face ID reference
    uint16_t additionalBitmapID; // 0x0E: Secondary texture
    int16_t field_10;            // 0x10
    int16_t field_12;            // 0x12
    int16_t textureDeltaU;       // 0x14: Texture U offset
    int16_t textureDeltaV;       // 0x16: Texture V offset
    int16_t cogNumber;           // 0x18: Mechanism number
    uint16_t eventID;            // 0x1A: Event trigger ID
    int16_t field_1C;            // 0x1C
    int16_t field_1E;            // 0x1E
    int16_t field_20;            // 0x20
    int16_t field_22;            // 0x22
};
static_assert(sizeof(BLVFaceExtraOnDisk) == 36, "BLVFaceExtraOnDisk must be 36 bytes");

// Sector on disk (116 bytes = 0x74)
struct BLVSectorOnDisk
{
    int32_t field_0;           // 0x00: Unknown
    uint16_t numFloors;        // 0x04
    int16_t pad0;              // 0x06
    uint32_t pFloors;          // 0x08: Runtime pointer
    uint16_t numWalls;         // 0x0C
    int16_t pad1;              // 0x0E
    uint32_t pWalls;           // 0x10: Runtime pointer
    uint16_t numCeilings;      // 0x14
    int16_t pad2;              // 0x16
    uint32_t pCeilings;        // 0x18: Runtime pointer
    uint16_t numFluids;        // 0x1C
    int16_t pad3;              // 0x1E
    uint32_t pFluids;          // 0x20: Runtime pointer
    int16_t numPortals;        // 0x24
    int16_t pad4;              // 0x26
    uint32_t pPortals;         // 0x28: Runtime pointer
    uint16_t numFaces;         // 0x2C
    uint16_t numNonBSPFaces;   // 0x2E
    uint32_t pFaceIDs;         // 0x30: Runtime pointer
    uint16_t numCylinderFaces; // 0x34
    int16_t pad5;              // 0x36
    int32_t pCylinderFaces;    // 0x38: Runtime pointer
    uint16_t numCogs;          // 0x3C
    int16_t pad6;              // 0x3E
    uint32_t pCogs;            // 0x40: Runtime pointer
    uint16_t numDecorations;   // 0x44
    int16_t pad7;              // 0x46
    uint32_t pDecorationIDs;   // 0x48: Runtime pointer
    uint16_t numMarkers;       // 0x4C
    int16_t pad8;              // 0x4E
    uint32_t pMarkers;         // 0x50: Runtime pointer
    uint16_t numLights;        // 0x54
    int16_t pad9;              // 0x56
    uint32_t pLights;          // 0x58: Runtime pointer
    int16_t waterLevel;        // 0x5C
    int16_t mistLevel;         // 0x5E
    int16_t lightDistMult;     // 0x60
    int16_t minAmbientLight;   // 0x62
    int16_t firstBSPNode;      // 0x64
    int16_t exitTag;           // 0x66
    int16_t bboxMinX;          // 0x68
    int16_t bboxMaxX;          // 0x6A
    int16_t bboxMinY;          // 0x6C
    int16_t bboxMaxY;          // 0x6E
    int16_t bboxMinZ;          // 0x70
    int16_t bboxMaxZ;          // 0x72
};
static_assert(sizeof(BLVSectorOnDisk) == 116, "BLVSectorOnDisk must be 116 bytes");

// Light source (16 bytes)
struct BLVLight
{
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t radius;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t type;
    uint16_t attributes;
    int16_t brightness;
};
static_assert(sizeof(BLVLight) == 16);

// Door definition (40 bytes)
struct BLVDoor
{
    uint32_t attributes;
    uint32_t doorId;
    uint32_t timeSinceTriggered;
    int32_t boundingX1;
    int32_t boundingX2;
    int32_t boundingY1;
    int32_t boundingY2;
    int16_t numVertices;
    int16_t numFaces;
    int16_t numSectors;
    int16_t numOffsets;
    int16_t state;
    int16_t padding;
};

// Spawn point (22 bytes)
struct BLVSpawnPoint
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

// Face attribute flags
enum class FaceAttribute : uint32_t
{
    Portal = 0x0001,
    CanSaturate = 0x0002,
    Floor = 0x0004,
    Water = 0x0008,
    Lava = 0x0010,
    Specular = 0x0020,
    CannotPickup = 0x0040,
    SecretDoor = 0x0080,
    Invisible = 0x0100,
    Animated = 0x0200,
    Event = 0x0400,
    TriggerEvent = 0x0800,
    Outdoor = 0x1000,
    HasPulsingLight = 0x2000,
    IsBitmap = 0x4000,
    Indoor = 0x8000,
    Ceiling = 0x10000,
    Clickable = 0x20000,
    Pressure = 0x40000,
    Ethereal = 0x80000
};

// Parsed face with resolved vertex indices and UV coordinates
struct ParsedFace
{
    std::vector<uint16_t> vertexIndices; // Indices into vertex array
    // Fixed-point plane (from facePlaneOld, used for collision)
    int32_t normalX = 0;        // Face normal X (fixed-point, /65536)
    int32_t normalY = 0;        // Face normal Y (fixed-point, /65536)
    int32_t normalZ = 0;        // Face normal Z (fixed-point, /65536)
    int32_t normalDistance = 0; // Distance from origin along normal
    // Float plane (from facePlane, used for rendering)
    float normalFX = 0.0f;    // Float normal X
    float normalFY = 0.0f;    // Float normal Y
    float normalFZ = 0.0f;    // Float normal Z
    float normalFDist = 0.0f; // Float distance
    // Z calculation coefficients
    int32_t zCalc1 = 0;
    int32_t zCalc2 = 0;
    int32_t zCalc3 = 0;
    // Face metadata
    uint32_t attributes = 0;    // Face attribute flags
    uint16_t sectorId = 0;      // Parent sector
    uint16_t otherSectorId = 0; // Adjacent sector (for portals)
    uint8_t numVertices = 0;    // Number of vertices in this face
    uint8_t polygonType = 0;    // Polygon type
    int16_t textureId = -1;     // Texture bitmap index
    int16_t faceExtraId = -1;   // Face extra index
    std::string textureName;    // Texture name from face texture table

    // Per-vertex data (from faceData flat array)
    std::vector<int16_t> xIntercepts; // X intercept displacements
    std::vector<int16_t> yIntercepts; // Y intercept displacements
    std::vector<int16_t> zIntercepts; // Z intercept displacements
    std::vector<int16_t> uCoords;     // Texture U coordinates
    std::vector<int16_t> vCoords;     // Texture V coordinates

    // Bounding box
    int16_t minX = 0, maxX = 0;
    int16_t minY = 0, maxY = 0;
    int16_t minZ = 0, maxZ = 0;

    // Helper methods
    bool isFloor() const { return (attributes & static_cast<uint32_t>(FaceAttribute::Floor)) != 0; }
    bool isWall() const { return !isFloor() && !isCeiling(); }
    bool isCeiling() const
    {
        return (attributes & static_cast<uint32_t>(FaceAttribute::Ceiling)) != 0;
    }
    bool isPortal() const
    {
        return (attributes & static_cast<uint32_t>(FaceAttribute::Portal)) != 0;
    }
    bool isWater() const { return (attributes & static_cast<uint32_t>(FaceAttribute::Water)) != 0; }
    bool isLava() const { return (attributes & static_cast<uint32_t>(FaceAttribute::Lava)) != 0; }
    bool isInvisible() const
    {
        return (attributes & static_cast<uint32_t>(FaceAttribute::Invisible)) != 0;
    }
};

// Parsed face extra with texture offsets and event data
struct ParsedFaceExtra
{
    int16_t faceId = 0;
    uint16_t additionalBitmapID = 0;
    int16_t textureDeltaU = 0;
    int16_t textureDeltaV = 0;
    int16_t cogNumber = 0;
    uint16_t eventID = 0;
    std::string textureName; // From face extra texture table
};

// Parsed door with variable-length arrays resolved
struct ParsedDoor
{
    BLVDoor header{};
    std::vector<uint16_t> vertexIds;
    std::vector<uint16_t> faceIds;
    std::vector<uint16_t> sectorIds;
    std::vector<uint16_t> offsetIds;
};

// Parsed sector (room) with resolved face lists
struct ParsedSector
{
    std::vector<uint16_t> floorFaceIds;
    std::vector<uint16_t> wallFaceIds;
    std::vector<uint16_t> ceilingFaceIds;
    std::vector<uint16_t> liquidFaceIds;
    std::vector<uint16_t> portalFaceIds;
    std::vector<uint16_t> decorIds;
    std::vector<uint16_t> lightIds;
    std::vector<uint16_t> bspLeafIds;
    // Additional sector metadata
    int16_t waterLevel = 0;
    int16_t minAmbientLight = 0;
};

// Parsed map data (variable-length data resolved)
struct BLVMapData
{
    // Header info
    uint32_t version = 0;
    std::string levelName;
    std::string skyTexture;

    // Header data sizes
    uint32_t faceDataSize = 0;
    uint32_t sectorRDataSize = 0;
    uint32_t sectorLRDataSize = 0;
    uint32_t doorsDataSize = 0;

    // Counts (from count-prefixed arrays)
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t sectorCount = 0;
    uint32_t lightCount = 0;

    // Geometry data
    std::vector<BLVVertex> vertices;
    std::vector<ParsedFace> faces;
    std::vector<ParsedFaceExtra> faceExtras;
    std::vector<ParsedSector> sectors;
    std::vector<BLVLight> lights;
    std::vector<ParsedDoor> doors;
    std::vector<BLVSpawnPoint> spawns;
};

class BLVMap
{
  public:
    using ProgressCallback = std::function<void(float)>;

    explicit BLVMap(util::ILogger& logger);

    // Parse from raw decompressed BLV data
    bool parse(const std::vector<uint8_t>& data, ProgressCallback progress = {});

    // Access parsed data
    const BLVMapData& getData() const { return mapData; }

    // Quick accessors
    const std::string& getLevelName() const { return mapData.levelName; }
    const std::string& getSkyTexture() const { return mapData.skyTexture; }
    uint32_t getVertexCount() const { return mapData.vertexCount; }
    uint32_t getFaceCount() const { return mapData.faceCount; }
    uint32_t getSectorCount() const { return mapData.sectorCount; }
    uint32_t getLightCount() const { return mapData.lightCount; }
    const std::vector<BLVVertex>& getVertices() const { return mapData.vertices; }
    const std::vector<ParsedFace>& getFaces() const { return mapData.faces; }
    const std::vector<ParsedSector>& getSectors() const { return mapData.sectors; }
    const std::vector<BLVLight>& getLights() const { return mapData.lights; }

  private:
    bool parseHeader(const std::vector<uint8_t>& data);
    bool parseVertices(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaces(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaceData(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaceTextures(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaceExtras(const std::vector<uint8_t>& data, size_t& offset);
    bool parseSectors(const std::vector<uint8_t>& data, size_t& offset);
    bool parseLights(const std::vector<uint8_t>& data, size_t& offset);
    bool parseTrailingData(const std::vector<uint8_t>& data, size_t& offset);

    std::string extractString(const char* data, size_t maxLen) const;
    bool skipCountPrefixed(const std::vector<uint8_t>& data, size_t& offset, size_t structSize,
                           uint32_t& outCount);

    util::ILogger& logger;
    BLVMapData mapData;
    ProgressCallback progressCallback;

    void reportProgress(float value);
};

} // namespace runeharbor::formats
