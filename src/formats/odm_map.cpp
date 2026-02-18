// SPDX-License-Identifier: MIT
#include "odm_map.hpp"

#include <algorithm>
#include <format>

#include <cstring>

namespace runeharbor::formats
{

ODMMap::ODMMap(util::ILogger& logger) : logger(logger) {}

bool ODMMap::parse(const std::vector<uint8_t>& data, ProgressCallback progress)
{
    progressCallback = std::move(progress);
    reportProgress(0.02f);

    if (data.size() < sizeof(ODMHeader) + 128 * 128 * 3)
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
    //   Header (176 bytes)
    //   HeightMap (128*128 uint8 = 16KB)
    //   TileMap (128*128 uint8 = 16KB)
    //   AttributeMap (128*128 uint8 = 16KB)
    //   Terrain normals section (variable)
    //   Model count + headers + data
    //   Sprites, IDList, OMAP, Spawns

    size_t offset = sizeof(ODMHeader);

    if (!parseHeightmap(data, offset))
    {
        logger.warning("Failed to parse heightmap, continuing...");
    }

    reportProgress(0.3f);

    if (!skipTerrainNormals(data, offset))
    {
        logger.warning("Failed to skip terrain normals section, continuing...");
    }

    reportProgress(0.4f);

    if (!parseBuildings(data, offset))
    {
        logger.warning("Failed to parse buildings, continuing...");
    }

    reportProgress(0.85f);

    // Skip sprites, IDList, OMAP sections to get to spawns
    // These sections have variable sizes and are not yet parsed
    // Spawn parsing is best-effort from current offset
    if (!parseSpawns(data, offset))
    {
        logger.debug("No spawns found (sections may be interleaved)");
    }

    reportProgress(0.95f);

    logger.info(std::format("Parsed ODM map: '{}' with {} buildings ({} faces), {}x{} terrain",
                            mapData.levelName.empty() ? "(unnamed)" : mapData.levelName,
                            mapData.buildings.size(), mapData.faceCount, ODMMapData::TERRAIN_SIZE,
                            ODMMapData::TERRAIN_SIZE));

    reportProgress(1.0f);
    return true;
}

bool ODMMap::parseHeader(const std::vector<uint8_t>& data)
{
    const ODMHeader* header = reinterpret_cast<const ODMHeader*>(data.data());

    mapData.levelName = extractString(header->levelName, sizeof(header->levelName));

    logger.debug(std::format("ODM header: name='{}'",
                             mapData.levelName.empty() ? "(none)" : mapData.levelName));

    return true;
}

bool ODMMap::parseHeightmap(const std::vector<uint8_t>& data, size_t& offset)
{
    constexpr size_t gridSize = ODMMapData::TERRAIN_SIZE * ODMMapData::TERRAIN_SIZE; // 16384

    // Heightmap: 128x128 uint8 values, world height = value * 32
    if (offset + gridSize * 3 > data.size())
    {
        logger.warning("Not enough data for heightmap + tilemap + attrmap");
        mapData.heightmap.resize(gridSize);
        return false;
    }

    mapData.heightmap.resize(gridSize);

    // Read heights (uint8, multiply by 32 for world units)
    const uint8_t* heights = data.data() + offset;
    for (size_t i = 0; i < gridSize; i++)
    {
        mapData.heightmap[i].height = static_cast<int16_t>(heights[i]) * 32;
    }
    offset += gridSize;

    // Read tilemap (uint8 tile indices)
    const uint8_t* tiles = data.data() + offset;
    for (size_t i = 0; i < gridSize; i++)
    {
        mapData.heightmap[i].tileIndex = tiles[i];
    }
    offset += gridSize;

    // Read attribute map (uint8)
    const uint8_t* attrs = data.data() + offset;
    for (size_t i = 0; i < gridSize; i++)
    {
        mapData.heightmap[i].attributes = attrs[i];
    }
    offset += gridSize;

    // Calculate height statistics
    int16_t minH = mapData.heightmap[0].height;
    int16_t maxH = minH;
    for (const auto& cell : mapData.heightmap)
    {
        minH = std::min(minH, cell.height);
        maxH = std::max(maxH, cell.height);
    }

    logger.debug(
        std::format("Parsed heightmap: {} cells, height range [{}, {}]", gridSize, minH, maxH));

    return true;
}

bool ODMMap::skipTerrainNormals(const std::vector<uint8_t>& data, size_t& offset)
{
    // Terrain normals section (MM7):
    //   uint32 normalCount
    //   uint8 CMAP1[0x20000] (131072 bytes)
    //   uint8 CMAP2[0x10000] (65536 bytes)
    //   Vec3i normals[normalCount] (normalCount * 12 bytes)

    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t normalCount = 0;
    std::memcpy(&normalCount, data.data() + offset, 4);

    // Sanity check: for a 128x128 grid with 2 normals per cell, expect ~32768
    if (normalCount > 100000)
    {
        logger.debug(
            std::format("Terrain normal count {} seems invalid, skipping section", normalCount));
        return false;
    }

    offset += 4;

    // CMAP1 + CMAP2
    constexpr size_t cmapSize = 0x20000 + 0x10000; // 196608 bytes
    if (offset + cmapSize > data.size())
    {
        logger.debug("Not enough data for terrain CMAPs");
        return false;
    }
    offset += cmapSize;

    // Normal vectors (normalCount * 12 bytes)
    size_t normalsSize = static_cast<size_t>(normalCount) * 12;
    if (offset + normalsSize > data.size())
    {
        logger.debug("Not enough data for terrain normals");
        return false;
    }
    offset += normalsSize;

    logger.debug(std::format("Skipped terrain normals: {} normals, {} bytes total", normalCount,
                             4 + cmapSize + normalsSize));

    return true;
}

bool ODMMap::parseBuildings(const std::vector<uint8_t>& data, size_t& offset)
{
    // Model count
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t modelCount = 0;
    std::memcpy(&modelCount, data.data() + offset, 4);

    if (modelCount > 500)
    {
        logger.debug(std::format("Model count {} seems too high, skipping", modelCount));
        return false;
    }

    if (modelCount == 0)
    {
        logger.debug("No models in ODM");
        return true;
    }

    offset += 4;
    mapData.buildingCount = modelCount;

    // Read all model headers first (count x 188 bytes)
    size_t headersSize = modelCount * sizeof(BSPModelHeader);
    if (offset + headersSize > data.size())
    {
        logger.debug("Not enough data for model headers");
        return false;
    }

    std::vector<BSPModelHeader> headers(modelCount);
    std::memcpy(headers.data(), data.data() + offset, headersSize);
    offset += headersSize;

    mapData.buildings.reserve(modelCount);

    // After all headers, model data is serialized sequentially per model:
    //   1. Vertices: numVertices * 12 bytes (ODMVertex3D)
    //   2. Faces: numFaces * 308 bytes (ODMFaceOnDisk)
    //   3. Face ordering: numFaces * 2 bytes (int16)
    //   4. Face texture names: numFaces * 10 bytes (null-padded ASCII)
    //   5. BSP nodes: numNodes * 8 bytes

    for (uint32_t m = 0; m < modelCount; m++)
    {
        const auto& hdr = headers[m];
        ParsedBuilding building;
        building.name = extractString(hdr.name, sizeof(hdr.name));
        building.worldX = hdr.posX;
        building.worldY = hdr.posY;
        building.worldZ = hdr.posZ;
        building.minX = hdr.minX;
        building.maxX = hdr.maxX;
        building.minY = hdr.minY;
        building.maxY = hdr.maxY;
        building.minZ = hdr.minZ;
        building.maxZ = hdr.maxZ;

        // 1. Vertices (numVertices * 12 bytes)
        size_t vertSize = static_cast<size_t>(hdr.numVertices) * sizeof(ODMVertex3D);
        if (hdr.numVertices > 0 && hdr.numVertices < 50000 && offset + vertSize <= data.size())
        {
            building.vertices.resize(hdr.numVertices);
            std::memcpy(building.vertices.data(), data.data() + offset, vertSize);
            offset += vertSize;
            mapData.vertexCount += hdr.numVertices;
        }
        else if (hdr.numVertices > 0)
        {
            logger.warning(std::format("Model '{}': cannot read {} vertices at offset {}",
                                       building.name, hdr.numVertices, offset));
            mapData.buildings.push_back(std::move(building));
            return false;
        }

        // 2. Faces (numFaces * 308 bytes)
        size_t facesSize = static_cast<size_t>(hdr.numFaces) * sizeof(ODMFaceOnDisk);
        std::vector<ODMFaceOnDisk> diskFaces;
        if (hdr.numFaces > 0 && hdr.numFaces < 50000 && offset + facesSize <= data.size())
        {
            diskFaces.resize(hdr.numFaces);
            std::memcpy(diskFaces.data(), data.data() + offset, facesSize);
            offset += facesSize;
        }
        else if (hdr.numFaces > 0)
        {
            logger.warning(std::format("Model '{}': cannot read {} faces at offset {}",
                                       building.name, hdr.numFaces, offset));
            mapData.buildings.push_back(std::move(building));
            return false;
        }

        // 3. Face ordering (numFaces * 2 bytes) - skip
        size_t orderingSize = static_cast<size_t>(hdr.numFaces) * 2;
        if (offset + orderingSize <= data.size())
        {
            offset += orderingSize;
        }

        // 4. Face texture names (numFaces * 10 bytes)
        std::vector<std::string> faceTexNames;
        size_t texNamesSize = static_cast<size_t>(hdr.numFaces) * 10;
        if (hdr.numFaces > 0 && offset + texNamesSize <= data.size())
        {
            faceTexNames.reserve(hdr.numFaces);
            for (uint32_t f = 0; f < hdr.numFaces; f++)
            {
                faceTexNames.push_back(extractString(
                    reinterpret_cast<const char*>(data.data() + offset + f * 10), 10));
            }
            offset += texNamesSize;
        }

        // 5. BSP nodes (numNodes * 8 bytes) - skip
        int32_t nodeCount = hdr.numNodes;
        if (nodeCount > 0 && nodeCount < 50000)
        {
            size_t nodesSize = static_cast<size_t>(nodeCount) * 8;
            if (offset + nodesSize <= data.size())
            {
                offset += nodesSize;
            }
        }

        // Convert disk faces to ParsedFace
        building.faces.reserve(diskFaces.size());
        for (size_t f = 0; f < diskFaces.size(); f++)
        {
            std::string texName;
            if (f < faceTexNames.size())
            {
                texName = faceTexNames[f];
            }
            building.faces.push_back(convertFace(diskFaces[f], texName));
        }

        mapData.faceCount += hdr.numFaces;
        mapData.buildings.push_back(std::move(building));
    }

    logger.debug(std::format("Parsed {} models with {} total vertices, {} total faces",
                             mapData.buildings.size(), mapData.vertexCount, mapData.faceCount));

    return !mapData.buildings.empty();
}

ParsedFace ODMMap::convertFace(const ODMFaceOnDisk& df, const std::string& texName) const
{
    ParsedFace face;

    // Plane (fixed-point integer, /65536 for float)
    face.normalX = df.normalX;
    face.normalY = df.normalY;
    face.normalZ = df.normalZ;
    face.normalDistance = df.normalDist;

    // Compute float normal from fixed-point
    constexpr float kFixedPointScale = 1.0f / 65536.0f;
    face.normalFX = static_cast<float>(df.normalX) * kFixedPointScale;
    face.normalFY = static_cast<float>(df.normalY) * kFixedPointScale;
    face.normalFZ = static_cast<float>(df.normalZ) * kFixedPointScale;
    face.normalFDist = static_cast<float>(df.normalDist) * kFixedPointScale;

    // Z calculation
    face.zCalc1 = df.zCalc1;
    face.zCalc2 = df.zCalc2;
    face.zCalc3 = df.zCalc3;

    // Attributes and metadata
    face.attributes = df.attributes;
    face.numVertices = df.numVertices;
    face.polygonType = df.polygonType;
    face.textureId = df.bitmapId;
    face.textureName = texName;
    face.sectorId = 0; // No sectors in outdoor
    face.otherSectorId = 0;

    // Per-vertex data (copy only the used vertices)
    uint8_t nv = df.numVertices;
    if (nv > 20)
    {
        nv = 20;
    }

    face.vertexIndices.resize(nv);
    face.uCoords.resize(nv);
    face.vCoords.resize(nv);
    face.xIntercepts.resize(nv);
    face.yIntercepts.resize(nv);
    face.zIntercepts.resize(nv);

    for (uint8_t i = 0; i < nv; i++)
    {
        face.vertexIndices[i] = static_cast<uint16_t>(df.vertexIds[i]);
        face.uCoords[i] = df.textureUs[i];
        face.vCoords[i] = df.textureVs[i];
        face.xIntercepts[i] = df.xInterceptDisp[i];
        face.yIntercepts[i] = df.yInterceptDisp[i];
        face.zIntercepts[i] = df.zInterceptDisp[i];
    }

    // Bounding box
    face.minX = df.bboxMinX;
    face.maxX = df.bboxMaxX;
    face.minY = df.bboxMinY;
    face.maxY = df.bboxMaxY;
    face.minZ = df.bboxMinZ;
    face.maxZ = df.bboxMaxZ;

    return face;
}

bool ODMMap::parseSpawns(const std::vector<uint8_t>& data, size_t& offset)
{
    // Spawns follow models (after sprites, IDList, OMAP sections)
    // Try to find spawn data by scanning forward
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t spawnCount = 0;
    std::memcpy(&spawnCount, data.data() + offset, 4);

    if (spawnCount > 1000)
    {
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
    // In MM7, terrain cells are 512 units, origin at map center
    constexpr float CELL_SIZE = 512.0f;
    constexpr float HALF_TERRAIN = ODMMapData::TERRAIN_SIZE / 2.0f;

    float gridX = (worldX / CELL_SIZE) + HALF_TERRAIN;
    float gridY = (worldY / CELL_SIZE) + HALF_TERRAIN;

    // Bilinear interpolation
    int x0 = static_cast<int>(gridX);
    int y0 = static_cast<int>(gridY);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = gridX - static_cast<float>(x0);
    float fy = gridY - static_cast<float>(y0);

    float h00 = static_cast<float>(getHeightAt(x0, y0));
    float h10 = static_cast<float>(getHeightAt(x1, y0));
    float h01 = static_cast<float>(getHeightAt(x0, y1));
    float h11 = static_cast<float>(getHeightAt(x1, y1));

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
