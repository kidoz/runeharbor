// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"
#include "blv_map.hpp" // For BLVVertex, ParsedFace (shared structures)

namespace runeharbor::formats
{

/**
 * ODM (Outdoor Map) Format
 *
 * ODM files contain outdoor world map geometry for MM6/7/8.
 * Structure:
 *   - Header (level name, sky texture, fog, etc.)
 *   - Heightmap (128x128 terrain heights)
 *   - Tilemap (terrain texture indices)
 *   - Building geometry (vertices and faces)
 *   - Spawns, decorations, lights
 */

#pragma pack(push, 1)

// ODM header structure
struct ODMHeader
{
    char levelName[32];       // Null-terminated level name
    char locationFile[32];    // Associated location file
    char locationName[32];    // Location display name
    char skyTexture[32];      // Sky texture name
    char groundTexture[32];   // Default ground texture
    uint32_t attributes;      // Level attributes/flags
    int32_t fogMinDist;       // Fog near distance
    int32_t fogMaxDist;       // Fog far distance
    int32_t fogDensity;       // Fog density
    uint8_t fogR, fogG, fogB; // Fog color
    uint8_t padding;
};

// ODM tile information
struct ODMTileHeader
{
    char tileName[20]; // Texture name
    uint16_t tileId;   // Tile ID
};

// Building (model) header
struct ODMBuildingHeader
{
    char name[32]; // Building name
    uint16_t vertexCount;
    uint16_t faceCount;
    uint16_t vertexIndexOffset;
    uint16_t faceDataOffset;
    int16_t boundingBox[6]; // minX, maxX, minY, maxY, minZ, maxZ
    int32_t worldX;         // Position in world
    int32_t worldY;
    int32_t worldZ;
};

// Spawn point (similar to BLV)
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
    std::vector<BLVVertex> vertices; // Local vertices
    std::vector<ParsedFace> faces;   // Faces referencing local vertices
    int32_t worldX = 0;              // World position
    int32_t worldY = 0;
    int32_t worldZ = 0;

    // Bounding box
    int16_t minX = 0, maxX = 0;
    int16_t minY = 0, maxY = 0;
    int16_t minZ = 0, maxZ = 0;
};

// Heightmap cell data
struct HeightmapCell
{
    int16_t height = 0;     // Terrain height
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
    bool parseTileTextures(const std::vector<uint8_t>& data, size_t& offset);
    bool parseBuildings(const std::vector<uint8_t>& data, size_t& offset);
    bool parseSpawns(const std::vector<uint8_t>& data, size_t& offset);

    std::string extractString(const char* data, size_t maxLen) const;

    util::ILogger& logger;
    ODMMapData mapData;
    ProgressCallback progressCallback;

    void reportProgress(float value);
};

} // namespace runeharbor::formats
