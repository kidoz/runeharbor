// SPDX-License-Identifier: MIT
#pragma once

#include <array>
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
 * Structure:
 *   - Header (version, name, sky texture)
 *   - Geometry counts (vertices, faces, sectors, lights)
 *   - Vertex data
 *   - Face data (BSP-organized)
 *   - Sector data (rooms)
 *   - Light data
 *   - Additional data (doors, spawns, items, etc.)
 */

#pragma pack(push, 1)

// BLV header at file start
struct BLVHeader
{
    uint32_t version;    // Format version (usually 1)
    char levelName[60];  // Null-terminated level name
    char skyTexture[12]; // Null-terminated sky texture name
};

// Vertex position (3D point)
struct BLVVertex
{
    int16_t x;
    int16_t y;
    int16_t z;
};

// Face attributes
struct BLVFaceAttributes
{
    uint16_t portalRoom : 1; // Is portal to another room
    uint16_t unknown1 : 1;
    uint16_t floorFace : 1; // Is a floor face
    uint16_t waterFace : 1; // Is water surface
    uint16_t lavaFace : 1;  // Is lava surface
    uint16_t unknown2 : 3;
    uint16_t invisible : 1; // Face is invisible
    uint16_t animated : 1;  // Animated texture
    uint16_t unknown3 : 6;
};

// Face structure (polygon in 3D space)
struct BLVFace
{
    int32_t normalX;        // Normal vector X (fixed point)
    int32_t normalY;        // Normal vector Y (fixed point)
    int32_t normalZ;        // Normal vector Z (fixed point)
    int32_t normalDistance; // Distance from origin along normal
    int32_t zCalc1;         // Z calculation coefficient 1
    int32_t zCalc2;         // Z calculation coefficient 2
    int32_t zCalc3;         // Z calculation coefficient 3
    uint32_t attributes;    // Face attributes/flags
    uint16_t vertexIds[20]; // Vertex indices (max 20 vertices per face)
    int16_t xOffsets[20];   // Texture X offsets
    int16_t yOffsets[20];   // Texture Y offsets
    int16_t uCoords[20];    // Texture U coordinates
    int16_t vCoords[20];    // Texture V coordinates
    uint16_t sectorId;      // Sector this face belongs to
    uint16_t otherSectorId; // Adjacent sector (for portals)
    int16_t boundingBox[6]; // minX, maxX, minY, maxY, minZ, maxZ
    uint16_t polygonType;   // Type of polygon
    uint8_t numVertices;    // Number of vertices in this face
    uint8_t padding;
    int16_t textureBitmapId; // Texture index
    int16_t unknown1;
    int16_t faceExtraId; // Index into face extra data
    int16_t unknown2;
};

// Simplified face for reading (variable size in file)
struct BLVFaceBasic
{
    int32_t normalX;
    int32_t normalY;
    int32_t normalZ;
    int32_t normalDistance;
    int32_t zCalc1;
    int32_t zCalc2;
    int32_t zCalc3;
    uint32_t attributes;
};

// Sector (room) structure
struct BLVSector
{
    int16_t floorFaceCount;
    int16_t wallFaceCount;
    int16_t ceilingFaceCount;
    int16_t floorFaceCount2; // Duplicates?
    int16_t liquidFaceCount;
    int16_t portalFaceCount;
    int16_t decorCount;
    int16_t lightCount;
    int16_t bspLeafCount;
    int16_t unknown1;
    int16_t unknown2;
    int16_t unknown3;
    // Variable-length arrays follow:
    // - floorFaceIds[]
    // - wallFaceIds[]
    // - ceilingFaceIds[]
    // - liquidFaceIds[]
    // - portalFaceIds[]
    // - decorIds[]
    // - lightIds[]
    // - bspLeafIds[]
};

// Light source
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

// Door definition
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
    // Variable-length arrays follow
};

// Spawn point
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

// Face extra data (raw, heuristic size)
struct BLVFaceExtraRaw
{
    std::array<uint8_t, 0x24> data;
};

// Item data (raw, heuristic size)
struct BLVItemRaw
{
    std::array<uint8_t, 0x24> data;
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

// Parsed face with resolved vertex indices
struct ParsedFace
{
    std::vector<uint16_t> vertexIndices; // Indices into vertex array
    int32_t normalX = 0;                 // Face normal X (fixed-point, /65536)
    int32_t normalY = 0;                 // Face normal Y (fixed-point, /65536)
    int32_t normalZ = 0;                 // Face normal Z (fixed-point, /65536)
    int32_t normalDistance = 0;          // Distance from origin along normal
    int32_t zCalc1 = 0;                  // Z calculation coefficient 1
    int32_t zCalc2 = 0;                  // Z calculation coefficient 2
    int32_t zCalc3 = 0;                  // Z calculation coefficient 3
    uint32_t attributes = 0;             // Face attribute flags
    uint16_t sectorId = 0;               // Parent sector
    uint16_t otherSectorId = 0;          // Adjacent sector (for portals)
    uint8_t numVertices = 0;             // Number of vertices in this face
    int16_t textureId = -1;              // Texture index
    int16_t faceExtraId = -1;            // Face extra index (if present)

    // Texture UV coordinates per vertex
    std::vector<int16_t> uCoords;
    std::vector<int16_t> vCoords;

    // Bounding box
    int16_t minX = 0, maxX = 0;
    int16_t minY = 0, maxY = 0;
    int16_t minZ = 0, maxZ = 0;

    // Raw embedded bytes from the 96-byte face structure (bytes 0x3C-0x5F)
    std::array<uint8_t, 36> embeddedData = {};

    // Texture offsets per vertex (best-effort parsing from embedded data)
    std::vector<int16_t> xOffsets;
    std::vector<int16_t> yOffsets;

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
};

// Parsed map data (variable-length data resolved)
struct BLVMapData
{
    // Header info
    uint32_t version = 0;
    std::string levelName;
    std::string skyTexture;

    // Geometry counts
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t sectorCount = 0;
    uint32_t lightCount = 0;

    // Geometry data
    std::vector<BLVVertex> vertices;
    std::vector<ParsedFace> faces;
    std::vector<ParsedSector> sectors;
    std::vector<BLVLight> lights;
    std::vector<BLVFaceExtraRaw> faceExtras;
    std::vector<ParsedDoor> doors;
    std::vector<BLVSpawnPoint> spawns;
    std::vector<BLVItemRaw> items;

    // Basic face data (without variable arrays for now)
    std::vector<BLVFaceBasic> faceBasics;
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
    bool parseGeometryCounts(const std::vector<uint8_t>& data);
    bool parseVertices(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaces(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaceVertexData(const std::vector<uint8_t>& data, size_t& offset);
    bool parseSectors(const std::vector<uint8_t>& data, size_t& offset);
    bool parseFaceExtras(const std::vector<uint8_t>& data, size_t& offset);
    bool parseDoors(const std::vector<uint8_t>& data, size_t& offset);
    bool parseSpawns(const std::vector<uint8_t>& data, size_t& offset);
    bool parseItems(const std::vector<uint8_t>& data, size_t& offset);
    bool parseLights(const std::vector<uint8_t>& data, size_t& offset);

    std::string extractString(const char* data, size_t maxLen) const;

    util::ILogger& logger;
    BLVMapData mapData;
    ProgressCallback progressCallback;

    void reportProgress(float value);
};

} // namespace runeharbor::formats
