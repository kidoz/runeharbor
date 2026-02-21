// SPDX-License-Identifier: MIT
#include "blv_map.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include <cstring>

namespace runeharbor::formats
{

BLVMap::BLVMap(util::ILogger& logger) : logger(logger) {}

bool BLVMap::parse(const std::vector<uint8_t>& data, ProgressCallback progress)
{
    progressCallback = std::move(progress);
    reportProgress(0.02f);

    if (data.size() < sizeof(BLVHeader))
    {
        logger.error("BLV data too small for header");
        return false;
    }

    if (!parseHeader(data))
    {
        return false;
    }

    reportProgress(0.05f);

    // Deserialization follows the exact MM7 BLV file layout.
    // Each section is read sequentially; offset tracks current position.
    size_t offset = sizeof(BLVHeader); // 0x88

    // 2. Vertices (count-prefixed, 6 bytes each)
    if (!parseVertices(data, offset))
    {
        logger.warning("Failed to parse vertices");
        return false;
    }

    reportProgress(0.15f);

    // 3. Faces (count-prefixed, 96 bytes each)
    if (!parseFaces(data, offset))
    {
        logger.warning("Failed to parse faces");
        return false;
    }

    reportProgress(0.30f);

    // 4. Face data (flat int16 array, faceDataSize bytes from header)
    if (!parseFaceData(data, offset))
    {
        logger.warning("Failed to parse face data, continuing without UV coords");
    }

    reportProgress(0.45f);

    // 5. Face textures (faceCount x 10 chars)
    if (!parseFaceTextures(data, offset))
    {
        logger.warning("Failed to parse face textures");
    }

    reportProgress(0.50f);

    // 6-7. Face extras (count-prefixed, 36 bytes each) + extra textures
    if (!parseFaceExtras(data, offset))
    {
        logger.debug("Could not parse face extras");
    }

    reportProgress(0.55f);

    // 8-10. Sectors (count-prefixed, 116 bytes each) + sector data + sector light data
    if (!parseSectors(data, offset))
    {
        logger.debug("Could not parse sectors");
    }

    reportProgress(0.75f);

    // 11+. Door count, decorations, lights, BSP, spawns, map outlines
    if (!parseTrailingData(data, offset))
    {
        logger.debug("Could not parse all trailing data sections");
    }

    reportProgress(0.95f);

    logger.info(std::format(
        "Parsed BLV map: '{}' with {} vertices, {} faces, {} sectors, {} lights",
        mapData.levelName.empty() ? "(unnamed)" : mapData.levelName, mapData.vertices.size(),
        mapData.faces.size(), mapData.sectors.size(), mapData.lights.size()));

    reportProgress(1.0f);
    return true;
}

bool BLVMap::parseHeader(const std::vector<uint8_t>& data)
{
    const auto* hdr = reinterpret_cast<const BLVHeader*>(data.data());

    mapData.version = hdr->version;
    mapData.levelName = extractString(hdr->levelName, sizeof(hdr->levelName));
    mapData.skyTexture = extractString(hdr->skyTexture, sizeof(hdr->skyTexture));
    mapData.faceDataSize = hdr->faceDataSize;
    mapData.sectorRDataSize = hdr->sectorRDataSize;
    mapData.sectorLRDataSize = hdr->sectorLRDataSize;
    mapData.doorsDataSize = hdr->doorsDataSize;

    logger.debug(std::format(
        "BLV header: version={}, name='{}', sky='{}', "
        "faceDataSize={}, sectorRData={}, sectorLRData={}, doorsData={}",
        mapData.version, mapData.levelName.empty() ? "(none)" : mapData.levelName,
        mapData.skyTexture.empty() ? "(none)" : mapData.skyTexture, mapData.faceDataSize,
        mapData.sectorRDataSize, mapData.sectorLRDataSize, mapData.doorsDataSize));

    return true;
}

bool BLVMap::parseVertices(const std::vector<uint8_t>& data, size_t& offset)
{
    // Count-prefixed: uint32 count + count x BLVVertex (6 bytes each)
    if (offset + 4 > data.size())
    {
        logger.error("No space for vertex count");
        return false;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;

    if (count == 0 || count > 100000)
    {
        logger.error(std::format("Invalid vertex count: {}", count));
        return false;
    }

    constexpr size_t vertexSize = sizeof(BLVVertex);

    if (offset + count * vertexSize > data.size())
    {
        logger.error(std::format("Vertex data exceeds file: need {} bytes at 0x{:X}",
                                 count * vertexSize, offset));
        return false;
    }

    mapData.vertexCount = count;
    mapData.vertices.resize(count);
    std::memcpy(mapData.vertices.data(), data.data() + offset, count * vertexSize);

    size_t startOffset = offset;
    offset += count * vertexSize;

    // Log vertex bounds
    if (!mapData.vertices.empty())
    {
        int16_t minX = mapData.vertices[0].x, maxX = minX;
        int16_t minY = mapData.vertices[0].y, maxY = minY;
        int16_t minZ = mapData.vertices[0].z, maxZ = minZ;

        for (const auto& v : mapData.vertices)
        {
            minX = std::min(minX, v.x);
            maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y);
            maxY = std::max(maxY, v.y);
            minZ = std::min(minZ, v.z);
            maxZ = std::max(maxZ, v.z);
        }

        logger.debug(std::format("Parsed {} vertices (0x{:X} to 0x{:X}), bounds X[{},{}] Y[{},{}] "
                                 "Z[{},{}]",
                                 count, startOffset, offset, minX, maxX, minY, maxY, minZ, maxZ));
    }

    return true;
}

bool BLVMap::parseFaces(const std::vector<uint8_t>& data, size_t& offset)
{
    // Count-prefixed: uint32 count + count x BLVFaceOnDisk (96 bytes each)
    if (offset + 4 > data.size())
    {
        logger.error("No space for face count");
        return false;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;

    if (count == 0 || count > 100000)
    {
        logger.error(std::format("Invalid face count: {}", count));
        return false;
    }

    constexpr size_t faceSize = sizeof(BLVFaceOnDisk);

    if (offset + count * faceSize > data.size())
    {
        logger.error(std::format("Face data exceeds file: need {} bytes at 0x{:X}",
                                 count * faceSize, offset));
        return false;
    }

    mapData.faceCount = count;
    mapData.faces.resize(count);

    size_t faceStart = offset;

    for (uint32_t i = 0; i < count; i++)
    {
        const auto* disk = reinterpret_cast<const BLVFaceOnDisk*>(data.data() + offset);
        ParsedFace& face = mapData.faces[i];

        // Float plane (for rendering)
        face.normalFX = disk->facePlaneNX;
        face.normalFY = disk->facePlaneNY;
        face.normalFZ = disk->facePlaneNZ;
        face.normalFDist = disk->facePlaneDist;

        // Fixed-point plane (for collision, legacy calculations)
        face.normalX = disk->facePlaneOldNX;
        face.normalY = disk->facePlaneOldNY;
        face.normalZ = disk->facePlaneOldNZ;
        face.normalDistance = disk->facePlaneOldDist;

        face.zCalc1 = disk->zCalc1;
        face.zCalc2 = disk->zCalc2;
        face.zCalc3 = disk->zCalc3;
        face.attributes = disk->attributes;
        face.faceExtraId = static_cast<int16_t>(disk->faceExtraID);
        face.textureId = static_cast<int16_t>(disk->bitmapID);
        face.sectorId = disk->sectorID;
        face.otherSectorId = static_cast<uint16_t>(disk->backSectorID);
        face.minX = disk->bboxMinX;
        face.maxX = disk->bboxMaxX;
        face.minY = disk->bboxMinY;
        face.maxY = disk->bboxMaxY;
        face.minZ = disk->bboxMinZ;
        face.maxZ = disk->bboxMaxZ;
        face.polygonType = disk->polygonType;
        face.numVertices = disk->numVertices;

        offset += faceSize;
    }

    logger.debug(
        std::format("Parsed {} face structs (0x{:X} to 0x{:X})", count, faceStart, offset));

    return true;
}

bool BLVMap::parseFaceData(const std::vector<uint8_t>& data, size_t& offset)
{
    // Flat int16 array, total size = header.faceDataSize bytes.
    // For each face sequentially: 6 arrays of (numVertices+1) x int16:
    //   vertexIDs, xIntercepts, yIntercepts, zIntercepts, uCoords, vCoords
    // The +1 entry wraps (last = first) for edge calculations.

    if (mapData.faceDataSize == 0)
    {
        logger.warning("Face data size is 0, no UV coordinates available");
        return false;
    }

    if (offset + mapData.faceDataSize > data.size())
    {
        logger.error(std::format("Face data exceeds file: need {} bytes at 0x{:X}",
                                 mapData.faceDataSize, offset));
        return false;
    }

    const int16_t* faceDataBase = reinterpret_cast<const int16_t*>(data.data() + offset);
    size_t totalEntries = mapData.faceDataSize / sizeof(int16_t);
    size_t idx = 0;

    for (uint32_t i = 0; i < mapData.faceCount; i++)
    {
        ParsedFace& face = mapData.faces[i];
        uint32_t nv = face.numVertices;
        uint32_t entryCount = nv + 1;

        // Need 6 arrays of entryCount entries
        if (idx + 6 * entryCount > totalEntries)
        {
            logger.warning(std::format("Face data truncated at face {}/{} (index {}/{})", i,
                                       mapData.faceCount, idx, totalEntries));
            break;
        }

        // Vertex IDs (stored as int16 but interpreted as uint16 indices)
        face.vertexIndices.resize(nv);
        for (uint32_t j = 0; j < nv; j++)
        {
            face.vertexIndices[j] = static_cast<uint16_t>(faceDataBase[idx + j]);
        }
        idx += entryCount;

        // X intercept displacements
        face.xIntercepts.assign(&faceDataBase[idx], &faceDataBase[idx + nv]);
        idx += entryCount;

        // Y intercept displacements
        face.yIntercepts.assign(&faceDataBase[idx], &faceDataBase[idx + nv]);
        idx += entryCount;

        // Z intercept displacements
        face.zIntercepts.assign(&faceDataBase[idx], &faceDataBase[idx + nv]);
        idx += entryCount;

        // Texture U coordinates
        face.uCoords.assign(&faceDataBase[idx], &faceDataBase[idx + nv]);
        idx += entryCount;

        // Texture V coordinates
        face.vCoords.assign(&faceDataBase[idx], &faceDataBase[idx + nv]);
        idx += entryCount;
    }

    size_t faceDataEnd = offset + mapData.faceDataSize;
    logger.debug(
        std::format("Distributed face data: {} entries used of {} total (0x{:X} to 0x{:X})", idx,
                    totalEntries, offset, faceDataEnd));

    // Validate vertex indices
    uint64_t totalIndices = 0;
    uint64_t outOfRange = 0;
    for (const auto& face : mapData.faces)
    {
        for (uint16_t vidx : face.vertexIndices)
        {
            totalIndices++;
            if (vidx >= mapData.vertices.size())
            {
                outOfRange++;
            }
        }
    }
    if (totalIndices > 0)
    {
        logger.debug(std::format(
            "Face vertex indices: total={}, out-of-range={} ({:.1f}%)", totalIndices, outOfRange,
            static_cast<double>(outOfRange) * 100.0 / static_cast<double>(totalIndices)));
    }

    offset = faceDataEnd;
    return true;
}

bool BLVMap::parseFaceTextures(const std::vector<uint8_t>& data, size_t& offset)
{
    // faceCount x 10-char texture names (not count-prefixed, uses face count)
    constexpr size_t nameLen = 10;
    size_t required = static_cast<size_t>(mapData.faceCount) * nameLen;

    if (offset + required > data.size())
    {
        logger.error(
            std::format("Face textures exceed file: need {} bytes at 0x{:X}", required, offset));
        return false;
    }

    uint32_t namedCount = 0;
    for (uint32_t i = 0; i < mapData.faceCount; i++)
    {
        mapData.faces[i].textureName = extractString(
            reinterpret_cast<const char*>(data.data() + offset + i * nameLen), nameLen);
        if (!mapData.faces[i].textureName.empty())
        {
            namedCount++;
        }
    }

    offset += required;

    logger.debug(
        std::format("Loaded {} face texture names ({} non-empty)", mapData.faceCount, namedCount));

    return true;
}

bool BLVMap::parseFaceExtras(const std::vector<uint8_t>& data, size_t& offset)
{
    // Count-prefixed: uint32 count + BLVFaceExtraOnDisk[count] (36 bytes each)
    // Then: count x 10-char extra texture names
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;

    if (count > 100000)
    {
        logger.warning(std::format("Invalid face extra count: {}", count));
        return false;
    }

    constexpr size_t extraSize = sizeof(BLVFaceExtraOnDisk);

    if (offset + count * extraSize > data.size())
    {
        logger.warning("Face extras exceed file");
        return false;
    }

    mapData.faceExtras.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        const auto* disk = reinterpret_cast<const BLVFaceExtraOnDisk*>(data.data() + offset);
        ParsedFaceExtra& extra = mapData.faceExtras[i];
        extra.faceId = disk->faceId;
        extra.additionalBitmapID = disk->additionalBitmapID;
        extra.textureDeltaU = disk->textureDeltaU;
        extra.textureDeltaV = disk->textureDeltaV;
        extra.cogNumber = disk->cogNumber;
        extra.eventID = disk->eventID;
        if (extra.faceId >= 0 && static_cast<size_t>(extra.faceId) < mapData.faces.size())
        {
            mapData.faces[static_cast<size_t>(extra.faceId)].eventId =
                static_cast<int>(extra.eventID);
        }
        offset += extraSize;
    }

    // Face extra texture names (count x 10 chars)
    constexpr size_t nameLen = 10;
    size_t namesRequired = static_cast<size_t>(count) * nameLen;

    if (offset + namesRequired <= data.size())
    {
        for (uint32_t i = 0; i < count; i++)
        {
            mapData.faceExtras[i].textureName = extractString(
                reinterpret_cast<const char*>(data.data() + offset + i * nameLen), nameLen);
        }
        offset += namesRequired;
    }

    logger.debug(std::format("Parsed {} face extras", count));
    return true;
}

bool BLVMap::parseSectors(const std::vector<uint8_t>& data, size_t& offset)
{
    // Count-prefixed: uint32 count + BLVSectorOnDisk[count] (116 bytes each)
    // Then: sectorData flat array (sectorRDataSize bytes)
    // Then: sectorLightData flat array (sectorLRDataSize bytes)

    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;

    if (count > 10000)
    {
        logger.warning(std::format("Invalid sector count: {}", count));
        return false;
    }

    constexpr size_t sectorSize = sizeof(BLVSectorOnDisk);

    if (offset + count * sectorSize > data.size())
    {
        logger.warning("Sector structs exceed file");
        return false;
    }

    mapData.sectorCount = count;

    // Read sector structs and store counts for distribution
    struct SectorInfo
    {
        uint16_t floors, walls, ceilings, fluids;
        uint16_t portals, faces, cylinderFaces, cogs;
        uint16_t decorations, markers, lights;
        int16_t waterLevel, minAmbientLight;
    };

    std::vector<SectorInfo> infos(count);

    for (uint32_t i = 0; i < count; i++)
    {
        const auto* disk = reinterpret_cast<const BLVSectorOnDisk*>(data.data() + offset);
        auto& si = infos[i];
        si.floors = disk->numFloors;
        si.walls = disk->numWalls;
        si.ceilings = disk->numCeilings;
        si.fluids = disk->numFluids;
        si.portals = static_cast<uint16_t>(disk->numPortals);
        si.faces = disk->numFaces;
        si.cylinderFaces = disk->numCylinderFaces;
        si.cogs = disk->numCogs;
        si.decorations = disk->numDecorations;
        si.markers = disk->numMarkers;
        si.lights = disk->numLights;
        si.waterLevel = disk->waterLevel;
        si.minAmbientLight = disk->minAmbientLight;
        offset += sectorSize;
    }

    logger.debug(std::format("Read {} sector structs (116 bytes each)", count));

    // Read sectorData flat array (sectorRDataSize bytes)
    if (offset + mapData.sectorRDataSize > data.size())
    {
        logger.warning("Sector data exceeds file");
        // Still create empty sectors with metadata
        mapData.sectors.resize(count);
        for (uint32_t i = 0; i < count; i++)
        {
            mapData.sectors[i].waterLevel = infos[i].waterLevel;
            mapData.sectors[i].minAmbientLight = infos[i].minAmbientLight;
        }
        return false;
    }

    const uint16_t* sectorData = reinterpret_cast<const uint16_t*>(data.data() + offset);
    size_t sectorDataEntries = mapData.sectorRDataSize / sizeof(uint16_t);
    size_t sdIdx = 0;

    // Read sectorLightData flat array (sectorLRDataSize bytes)
    const uint16_t* sectorLightData =
        reinterpret_cast<const uint16_t*>(data.data() + offset + mapData.sectorRDataSize);
    size_t sectorLightEntries = mapData.sectorLRDataSize / sizeof(uint16_t);
    size_t slIdx = 0;

    if (offset + mapData.sectorRDataSize + mapData.sectorLRDataSize > data.size())
    {
        logger.warning("Sector light data exceeds file");
    }

    // Distribute sector data to ParsedSectors
    mapData.sectors.resize(count);

    auto readIds = [&](size_t n, std::vector<uint16_t>& out)
    {
        if (sdIdx + n > sectorDataEntries)
        {
            return;
        }
        out.assign(&sectorData[sdIdx], &sectorData[sdIdx + n]);
        sdIdx += n;
    };

    auto readLightIds = [&](size_t n, std::vector<uint16_t>& out)
    {
        if (slIdx + n > sectorLightEntries)
        {
            return;
        }
        out.assign(&sectorLightData[slIdx], &sectorLightData[slIdx + n]);
        slIdx += n;
    };

    for (uint32_t i = 0; i < count; i++)
    {
        ParsedSector& sector = mapData.sectors[i];
        const auto& si = infos[i];

        sector.waterLevel = si.waterLevel;
        sector.minAmbientLight = si.minAmbientLight;

        // Read from sectorData (regular data) sequentially
        readIds(si.floors, sector.floorFaceIds);
        readIds(si.walls, sector.wallFaceIds);
        readIds(si.ceilings, sector.ceilingFaceIds);
        readIds(si.fluids, sector.liquidFaceIds);
        readIds(si.portals, sector.portalFaceIds);
        // Skip: faces (numFaces), cylinderFaces, cogs, decorations, markers
        sdIdx += si.faces + si.cylinderFaces + si.cogs + si.decorations + si.markers;

        // Read from sectorLightData sequentially
        readLightIds(si.lights, sector.lightIds);
    }

    offset += mapData.sectorRDataSize + mapData.sectorLRDataSize;

    logger.debug(std::format("Distributed sector data: {}/{} regular entries, {}/{} light entries",
                             sdIdx, sectorDataEntries, slIdx, sectorLightEntries));

    return !mapData.sectors.empty();
}

bool BLVMap::parseLights(const std::vector<uint8_t>& data, size_t& offset)
{
    // Count-prefixed: uint32 count + BLVLight[count] (16 bytes each)
    if (offset + 4 > data.size())
    {
        return false;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;

    if (count > 10000)
    {
        logger.debug(std::format("Light count {} seems too large, skipping", count));
        return false;
    }

    constexpr size_t lightSize = sizeof(BLVLight);

    if (offset + count * lightSize > data.size())
    {
        logger.warning("Light data exceeds file");
        return false;
    }

    mapData.lightCount = count;
    mapData.lights.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        std::memcpy(&mapData.lights[i], data.data() + offset, lightSize);
        offset += lightSize;
    }

    logger.debug(std::format("Parsed {} lights", count));
    return true;
}

bool BLVMap::parseTrailingData(const std::vector<uint8_t>& data, size_t& offset)
{
    // After sectors + sector data, the remaining layout is:
    //   11. Door count (uint32, just the count)
    //   12. Decorations (count-prefixed, 32 bytes each)
    //   13. Decoration names (decorCount x 32 chars)
    //   14. Lights (count-prefixed, 16 bytes each)
    //   15. BSP nodes (count-prefixed, 8 bytes each)
    //   16. Spawn points (count-prefixed, variable size)
    //   17. Map outlines (count-prefixed, 12 bytes each)

    // 11. Door count
    if (offset + 4 > data.size())
    {
        return false;
    }
    uint32_t doorCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;
    logger.debug(std::format("Door count: {}", doorCount));

    // 12. Decorations (count-prefixed, 32 bytes each)
    if (offset + 4 > data.size())
    {
        logger.debug("No space for decoration count");
        return false;
    }
    const uint32_t decorCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;
    constexpr size_t decorSize = sizeof(BLVDecorationOnDisk);
    if (offset + static_cast<size_t>(decorCount) * decorSize > data.size())
    {
        logger.debug("Decoration data exceeds file");
        return false;
    }

    mapData.decorations.resize(decorCount);
    for (uint32_t i = 0; i < decorCount; i++)
    {
        const auto* decor = reinterpret_cast<const BLVDecorationOnDisk*>(data.data() + offset);
        auto& out = mapData.decorations[i];
        out.x = decor->x;
        out.y = decor->y;
        out.z = decor->z;
        out.eventId = decor->eventId;
        offset += decorSize;
    }
    logger.debug(std::format("Parsed {} decorations", decorCount));

    // 13. Decoration names (decorCount x 32 chars)
    constexpr size_t nameSize = 32;
    size_t decorNamesSize = static_cast<size_t>(decorCount) * nameSize;
    if (offset + decorNamesSize > data.size())
    {
        logger.debug("Decoration names exceed file");
        return false;
    }
    for (uint32_t i = 0; i < decorCount; i++)
    {
        mapData.decorations[i].name = extractString(
            reinterpret_cast<const char*>(data.data() + offset + static_cast<size_t>(i) * nameSize),
            nameSize);
    }
    offset += decorNamesSize;

    // 14. Lights (count-prefixed, 16 bytes each)
    if (!parseLights(data, offset))
    {
        logger.debug("Could not parse lights in trailing data");
    }

    // 15. BSP nodes (count-prefixed, 8 bytes each)
    uint32_t bspCount = 0;
    if (!skipCountPrefixed(data, offset, 8, bspCount))
    {
        logger.debug("Could not skip BSP nodes");
        return false;
    }
    logger.debug(std::format("Skipped {} BSP nodes", bspCount));

    // 16. Spawn points (count-prefixed)
    if (offset + 4 <= data.size())
    {
        uint32_t spawnCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);
        offset += 4;

        if (spawnCount <= 5000)
        {
            constexpr size_t spawnSize = sizeof(BLVSpawnPoint);

            if (offset + spawnCount * spawnSize <= data.size())
            {
                mapData.spawns.resize(spawnCount);
                for (uint32_t i = 0; i < spawnCount; i++)
                {
                    std::memcpy(&mapData.spawns[i], data.data() + offset, spawnSize);
                    offset += spawnSize;
                }
                logger.debug(std::format("Parsed {} spawn points", spawnCount));
            }
        }
    }

    // 17. Map outlines (count-prefixed, 12 bytes each)
    if (offset + 4 > data.size())
    {
        logger.debug("No space for outline count");
        return true;
    }

    const uint32_t outlineCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;
    constexpr size_t outlineSize = sizeof(BLVOutlinePoint);
    const size_t outlinesSize = static_cast<size_t>(outlineCount) * outlineSize;

    if (offset + outlinesSize > data.size())
    {
        logger.warning(std::format("Outline data exceeds file: count={}, need={} bytes at 0x{:X}",
                                   outlineCount, outlinesSize, offset));
        return true;
    }

    mapData.outlines.resize(outlineCount);
    if (outlineCount > 0)
    {
        std::memcpy(mapData.outlines.data(), data.data() + offset, outlinesSize);
        offset += outlinesSize;
    }
    logger.debug(std::format("Parsed {} outline points", outlineCount));

    return true;
}

bool BLVMap::skipCountPrefixed(const std::vector<uint8_t>& data, size_t& offset, size_t structSize,
                               uint32_t& outCount)
{
    if (offset + 4 > data.size())
    {
        outCount = 0;
        return false;
    }

    outCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += 4;

    size_t totalSize = static_cast<size_t>(outCount) * structSize;
    if (offset + totalSize > data.size())
    {
        return false;
    }

    offset += totalSize;
    return true;
}

std::string BLVMap::extractString(const char* data, size_t maxLen) const
{
    std::string result;
    for (size_t i = 0; i < maxLen && data[i] != '\0'; i++)
    {
        result += data[i];
    }
    return result;
}

void BLVMap::reportProgress(float value)
{
    if (!progressCallback)
    {
        return;
    }

    value = std::clamp(value, 0.0f, 1.0f);
    progressCallback(value);
}

} // namespace runeharbor::formats
