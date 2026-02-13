#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "odm_map.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include <cstring>

namespace runeharbor::formats
{

ODMMap::ODMMap(util::ILogger& logger) : logger(logger) {}

bool ODMMap::parse(const std::vector<uint8_t>& data, ProgressCallback progress)
{
    progressCallback = std::move(progress);
    reportProgress(0.02f);

    if (data.size() < sizeof(ODMHeader) + 128 * 128 * 2)
    {
        logger.error("ODM data too small");
        return false;
    }

    if (!parseHeader(data))
    {
        return false;
    }

    reportProgress(0.1f);

    // ODM file layout (MM7):
    // 0x00-0xB7: Header (184 bytes)
    // 0xB8+: Tile texture list (variable)
    // After tiles: Heightmap (128*128*2 bytes = 32KB)
    // After heightmap: Tilemap (128*128 bytes = 16KB)
    // After tilemap: Attribute map (128*128 bytes = 16KB)
    // After attributes: Building data

    size_t offset = sizeof(ODMHeader);

    if (!parseTileTextures(data, offset))
    {
        logger.warning("Failed to parse tile textures, continuing...");
    }

    reportProgress(0.2f);

    if (!parseHeightmap(data, offset))
    {
        logger.warning("Failed to parse heightmap, continuing...");
    }

    reportProgress(0.6f);

    if (!parseBuildings(data, offset))
    {
        logger.warning("Failed to parse buildings, continuing...");
    }

    reportProgress(0.85f);

    if (!parseSpawns(data, offset))
    {
        logger.warning("Failed to parse spawns, continuing...");
    }

    reportProgress(0.95f);

    logger.info(std::format("Parsed ODM map: '{}' with {} buildings, {}x{} terrain",
                            mapData.levelName.empty() ? "(unnamed)" : mapData.levelName,
                            mapData.buildings.size(), ODMMapData::TERRAIN_SIZE,
                            ODMMapData::TERRAIN_SIZE));

    reportProgress(1.0f);
    return true;
}

bool ODMMap::parseHeader(const std::vector<uint8_t>& data)
{
    // Read header
    const ODMHeader* header = reinterpret_cast<const ODMHeader*>(data.data());

    mapData.levelName = extractString(header->levelName, sizeof(header->levelName));
    mapData.skyTexture = extractString(header->skyTexture, sizeof(header->skyTexture));
    mapData.groundTexture = extractString(header->groundTexture, sizeof(header->groundTexture));

    mapData.fogMinDist = header->fogMinDist;
    mapData.fogMaxDist = header->fogMaxDist;
    mapData.fogR = header->fogR;
    mapData.fogG = header->fogG;
    mapData.fogB = header->fogB;

    logger.debug(std::format("ODM header: name='{}', sky='{}', ground='{}'",
                             mapData.levelName.empty() ? "(none)" : mapData.levelName,
                             mapData.skyTexture.empty() ? "(none)" : mapData.skyTexture,
                             mapData.groundTexture.empty() ? "(none)" : mapData.groundTexture));

    return true;
}

bool ODMMap::parseTileTextures(const std::vector<uint8_t>& data, size_t& offset)
{
    // Tile textures are stored as a count followed by tile headers
    // The exact format varies - for now we'll skip detailed parsing

    // Read tile count (if available)
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t tileCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);

    // Sanity check
    if (tileCount > 256)
    {
        // Probably not a tile count - try alternative parsing
        tileCount = 0;
    }

    if (tileCount > 0)
    {
        offset += 4;

        for (uint32_t i = 0; i < tileCount && offset + sizeof(ODMTileHeader) <= data.size(); i++)
        {
            const ODMTileHeader* tile =
                reinterpret_cast<const ODMTileHeader*>(data.data() + offset);
            mapData.tileTextures.push_back(extractString(tile->tileName, sizeof(tile->tileName)));
            offset += sizeof(ODMTileHeader);
        }

        logger.debug(std::format("Parsed {} tile textures", mapData.tileTextures.size()));
    }

    return true;
}

bool ODMMap::parseHeightmap(const std::vector<uint8_t>& data, size_t& offset)
{
    // Heightmap is 128x128 int16_t values
    constexpr size_t heightmapSize = ODMMapData::TERRAIN_SIZE * ODMMapData::TERRAIN_SIZE;
    constexpr size_t heightmapBytes = heightmapSize * sizeof(int16_t);

    // Search for heightmap data - it should start with reasonable height values
    // Heights in MM7 are typically in range -1000 to 5000

    // Try current offset first
    size_t searchStart = offset;
    size_t foundOffset = 0;
    bool found = false;

    // Simple heuristic: look for a block of reasonable height values
    for (size_t testOffset = searchStart; testOffset + heightmapBytes <= data.size();
         testOffset += 4)
    {
        // Sample a few heights to check validity
        const int16_t* heights = reinterpret_cast<const int16_t*>(data.data() + testOffset);

        int validCount = 0;
        int sampleCount = 16;

        for (int i = 0; i < sampleCount; i++)
        {
            int idx = (i * heightmapSize) / sampleCount;
            if (heights[idx] >= -2000 && heights[idx] <= 10000)
            {
                validCount++;
            }
        }

        if (validCount >= sampleCount - 2)
        {
            foundOffset = testOffset;
            found = true;
            break;
        }

        // Only search first 64KB
        if (testOffset - searchStart > 65536)
        {
            break;
        }
    }

    if (!found)
    {
        logger.warning("Could not locate heightmap data");
        // Initialize with flat terrain
        mapData.heightmap.resize(heightmapSize);
        for (auto& cell : mapData.heightmap)
        {
            cell.height = 0;
            cell.tileIndex = 0;
            cell.attributes = 0;
        }
        return false;
    }

    offset = foundOffset;

    // Read heightmap
    mapData.heightmap.resize(heightmapSize);
    const int16_t* heights = reinterpret_cast<const int16_t*>(data.data() + offset);

    for (size_t i = 0; i < heightmapSize; i++)
    {
        mapData.heightmap[i].height = heights[i];
    }

    offset += heightmapBytes;

    // Read tilemap (128x128 bytes)
    if (offset + heightmapSize <= data.size())
    {
        const uint8_t* tiles = data.data() + offset;
        for (size_t i = 0; i < heightmapSize; i++)
        {
            mapData.heightmap[i].tileIndex = tiles[i];
        }
        offset += heightmapSize;
    }

    // Read attribute map (128x128 bytes)
    if (offset + heightmapSize <= data.size())
    {
        const uint8_t* attrs = data.data() + offset;
        for (size_t i = 0; i < heightmapSize; i++)
        {
            mapData.heightmap[i].attributes = attrs[i];
        }
        offset += heightmapSize;
    }

    // Calculate height statistics
    int16_t minH = mapData.heightmap[0].height;
    int16_t maxH = minH;
    for (const auto& cell : mapData.heightmap)
    {
        minH = std::min(minH, cell.height);
        maxH = std::max(maxH, cell.height);
    }

    logger.debug(std::format("Parsed heightmap: {} cells, height range [{}, {}]", heightmapSize,
                             minH, maxH));

    return true;
}

bool ODMMap::parseBuildings(const std::vector<uint8_t>& data, size_t& offset)
{
    // Building data starts with a count
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t buildingCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);

    // Sanity check
    if (buildingCount > 500)
    {
        logger.debug(std::format("Building count {} seems too high, skipping building parsing",
                                 buildingCount));
        return false;
    }

    if (buildingCount == 0)
    {
        logger.debug("No buildings in ODM");
        return true;
    }

    offset += 4;

    mapData.buildings.reserve(buildingCount);
    mapData.buildingCount = buildingCount;

    // Read building headers first
    std::vector<ODMBuildingHeader> headers;
    headers.reserve(buildingCount);

    for (uint32_t i = 0; i < buildingCount && offset + sizeof(ODMBuildingHeader) <= data.size();
         i++)
    {
        ODMBuildingHeader hdr;
        std::memcpy(&hdr, data.data() + offset, sizeof(ODMBuildingHeader));
        headers.push_back(hdr);
        offset += sizeof(ODMBuildingHeader);
    }

    // Parse each building's geometry
    for (const auto& hdr : headers)
    {
        ParsedBuilding building;
        building.name = extractString(hdr.name, sizeof(hdr.name));
        building.worldX = hdr.worldX;
        building.worldY = hdr.worldY;
        building.worldZ = hdr.worldZ;
        building.minX = hdr.boundingBox[0];
        building.maxX = hdr.boundingBox[1];
        building.minY = hdr.boundingBox[2];
        building.maxY = hdr.boundingBox[3];
        building.minZ = hdr.boundingBox[4];
        building.maxZ = hdr.boundingBox[5];

        // Read vertices
        if (hdr.vertexCount > 0 && hdr.vertexCount < 10000 &&
            offset + hdr.vertexCount * sizeof(BLVVertex) <= data.size())
        {
            building.vertices.resize(hdr.vertexCount);
            std::memcpy(building.vertices.data(), data.data() + offset,
                        hdr.vertexCount * sizeof(BLVVertex));
            offset += hdr.vertexCount * sizeof(BLVVertex);
            mapData.vertexCount += hdr.vertexCount;
        }

        // Read faces (simplified - similar to BLV faces)
        // ODM face format may differ slightly from BLV
        // For now, just skip face data and note the count
        mapData.faceCount += hdr.faceCount;

        mapData.buildings.push_back(std::move(building));
    }

    logger.debug(std::format("Parsed {} buildings with {} total vertices", mapData.buildings.size(),
                             mapData.vertexCount));

    return !mapData.buildings.empty();
}

bool ODMMap::parseSpawns(const std::vector<uint8_t>& data, size_t& offset)
{
    // Spawns follow buildings
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t spawnCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);

    if (spawnCount > 1000)
    {
        logger.debug("Spawn count seems invalid, skipping");
        return false;
    }

    if (spawnCount == 0)
    {
        return true;
    }

    offset += 4;

    mapData.spawns.reserve(spawnCount);

    for (uint32_t i = 0; i < spawnCount && offset + sizeof(ODMSpawnPoint) <= data.size(); i++)
    {
        ODMSpawnPoint spawn;
        std::memcpy(&spawn, data.data() + offset, sizeof(ODMSpawnPoint));
        mapData.spawns.push_back(spawn);
        offset += sizeof(ODMSpawnPoint);
    }

    logger.debug(std::format("Parsed {} spawns", mapData.spawns.size()));

    return true;
}

int16_t ODMMap::getHeightAt(int x, int y) const
{
    if (x < 0 || x >= ODMMapData::TERRAIN_SIZE || y < 0 || y >= ODMMapData::TERRAIN_SIZE)
    {
        return 0;
    }

    size_t index = static_cast<size_t>(y * ODMMapData::TERRAIN_SIZE + x);
    if (index >= mapData.heightmap.size())
    {
        return 0;
    }

    return mapData.heightmap[index].height;
}

float ODMMap::getHeightAtWorld(float worldX, float worldY) const
{
    // Convert world coordinates to grid coordinates
    // In MM7, terrain cells are typically 512 units
    constexpr float CELL_SIZE = 512.0f;
    constexpr float HALF_TERRAIN = ODMMapData::TERRAIN_SIZE / 2.0f;

    float gridX = (worldX / CELL_SIZE) + HALF_TERRAIN;
    float gridY = (worldY / CELL_SIZE) + HALF_TERRAIN;

    // Bilinear interpolation
    int x0 = static_cast<int>(gridX);
    int y0 = static_cast<int>(gridY);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = gridX - x0;
    float fy = gridY - y0;

    float h00 = static_cast<float>(getHeightAt(x0, y0));
    float h10 = static_cast<float>(getHeightAt(x1, y0));
    float h01 = static_cast<float>(getHeightAt(x0, y1));
    float h11 = static_cast<float>(getHeightAt(x1, y1));

    // Bilinear interpolation
    float h0 = h00 + fx * (h10 - h00);
    float h1 = h01 + fx * (h11 - h01);
    return h0 + fy * (h1 - h0);
}

std::string ODMMap::extractString(const char* data, size_t maxLen) const
{
    std::string result;
    for (size_t i = 0; i < maxLen && data[i] != '\0'; i++)
    {
        result += data[i];
    }
    return result;
}

void ODMMap::reportProgress(float value)
{
    if (!progressCallback)
    {
        return;
    }

    value = std::clamp(value, 0.0f, 1.0f);
    progressCallback(value);
}

} // namespace runeharbor::formats
