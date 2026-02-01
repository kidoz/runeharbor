// SPDX-License-Identifier: MIT
#include "blv_map.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <utility>

#include <cstring>

namespace runeharbor::formats
{

BLVMap::BLVMap(util::ILogger& logger) : logger(logger) {}

bool BLVMap::parse(const std::vector<uint8_t>& data, ProgressCallback progress)
{
    progressCallback = std::move(progress);
    reportProgress(0.02f);

    if (data.size() < sizeof(BLVHeader) + 16)
    {
        logger.error("BLV data too small for header");
        return false;
    }

    if (!parseHeader(data))
    {
        return false;
    }

    if (!parseGeometryCounts(data))
    {
        return false;
    }

    reportProgress(0.1f);

    // BLV file layout (MM7):
    // 0x00-0x4B: Header (76 bytes) - version, name, sky
    // 0x4C-0x87: Reserved + geometry info
    // 0x88-0x8B: Vertex count
    // 0x8C+: Vertex data (6 bytes per vertex)
    // After vertices: 96-byte face structures
    // After faces: Sector data, lights, etc.

    size_t offset = 0x8C; // Vertices start here

    if (!parseVertices(data, offset))
    {
        logger.warning("Failed to parse vertices, continuing with partial data");
    }

    reportProgress(0.35f);

    // Parse faces right after vertices (96-byte structures)
    if (!parseFaces(data, offset))
    {
        logger.warning("Failed to parse faces, continuing with partial data");
    }

    reportProgress(0.6f);

    // Try to parse sectors (may be after face data)
    size_t cursor = offset;
    if (!parseSectors(data, cursor))
    {
        logger.debug("Could not parse sectors at expected offset");
        cursor = offset;
    }

    // Try to parse face extra table
    if (!parseFaceExtras(data, cursor))
    {
        logger.debug("Could not parse face extras");
    }

    // Try to parse doors
    if (!parseDoors(data, cursor))
    {
        logger.debug("Could not parse doors");
    }

    // Try to parse spawns
    if (!parseSpawns(data, cursor))
    {
        logger.debug("Could not parse spawns");
    }

    // Try to parse items
    if (!parseItems(data, cursor))
    {
        logger.debug("Could not parse items");
    }

    reportProgress(0.8f);

    // Try to parse lights (prefer header count; otherwise heuristic)
    if (!parseLights(data, cursor))
    {
        logger.debug("Could not parse lights");
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
    // Version is first 4 bytes
    mapData.version = *reinterpret_cast<const uint32_t*>(data.data());

    // Level name follows version (60 bytes max)
    mapData.levelName = extractString(reinterpret_cast<const char*>(data.data() + 4), 60);

    // Sky texture follows level name (12 bytes)
    mapData.skyTexture = extractString(reinterpret_cast<const char*>(data.data() + 64), 12);

    logger.debug(std::format("BLV version: {}, name: '{}', sky: '{}'", mapData.version,
                             mapData.levelName.empty() ? "(none)" : mapData.levelName,
                             mapData.skyTexture.empty() ? "(none)" : mapData.skyTexture));

    return true;
}

bool BLVMap::parseGeometryCounts(const std::vector<uint8_t>& data)
{
    // BLV file format for MM7:
    // 0x68-0x6B: Unknown (not reliable for face offset)
    // 0x6C-0x6F: Face count from header (may not match actual parsed faces)
    // 0x70-0x73: Sector count
    // 0x74-0x77: Light count
    // 0x88-0x8B: Vertex count
    // 0x8C+: Vertex data (6 bytes each)
    // After vertices: 96-byte face structures

    constexpr size_t countsOffset = 0x68;

    if (data.size() < countsOffset + 32)
    {
        logger.error("BLV data too small for geometry counts");
        return false;
    }

    const uint32_t* headerVals = reinterpret_cast<const uint32_t*>(data.data() + countsOffset);

    // These header values may not be accurate for face parsing
    mapData.faceCount = headerVals[1];
    mapData.sectorCount = headerVals[2];
    mapData.lightCount = headerVals[3];

    logger.debug(std::format("BLV header counts: faces={}, sectors={}, lights={}", mapData.faceCount,
                             mapData.sectorCount, mapData.lightCount));

    // Vertex count is stored at 0x88
    if (data.size() >= 0x8C)
    {
        mapData.vertexCount = *reinterpret_cast<const uint32_t*>(data.data() + 0x88);
        if (mapData.vertexCount > 50000)
        {
            mapData.vertexCount = 0;
        }
        logger.debug(std::format("Vertex count from header (0x88): {}", mapData.vertexCount));
    }

    // Sanity check on counts
    if (mapData.faceCount > 100000)
    {
        mapData.faceCount = 0;
    }
    if (mapData.sectorCount > 10000)
    {
        mapData.sectorCount = 0;
    }
    if (mapData.lightCount > 10000)
    {
        mapData.lightCount = 0;
    }

    return true;
}

bool BLVMap::parseVertices(const std::vector<uint8_t>& data, size_t& offset)
{
    const size_t vertexSize = sizeof(BLVVertex);

    // Vertices start at 0x8C, count is at 0x88
    offset = 0x8C;

    if (mapData.vertexCount == 0)
    {
        logger.warning("No vertices to parse");
        return false;
    }

    size_t requiredSize = offset + mapData.vertexCount * vertexSize;
    if (data.size() < requiredSize)
    {
        logger.error(std::format("BLV data too small for vertices: need {} bytes, have {}",
                                 requiredSize, data.size()));
        return false;
    }

    mapData.vertices.reserve(mapData.vertexCount);

    for (uint32_t i = 0; i < mapData.vertexCount; i++)
    {
        BLVVertex vertex;
        std::memcpy(&vertex, data.data() + offset, vertexSize);
        mapData.vertices.push_back(vertex);
        offset += vertexSize;
    }

    logger.debug(std::format("Parsed {} vertices from 0x8C to 0x{:X}", mapData.vertices.size(),
                             offset));

    // Calculate and log vertex bounds
    if (!mapData.vertices.empty())
    {
        int16_t minX = mapData.vertices[0].x, maxX = minX;
        int16_t minY = mapData.vertices[0].y, maxY = minY;
        int16_t minZ = mapData.vertices[0].z, maxZ = minZ;
        int64_t sumX = 0;
        int64_t sumY = 0;
        int64_t sumZ = 0;

        for (const auto& v : mapData.vertices)
        {
            minX = std::min(minX, v.x);
            maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y);
            maxY = std::max(maxY, v.y);
            minZ = std::min(minZ, v.z);
            maxZ = std::max(maxZ, v.z);
            sumX += v.x;
            sumY += v.y;
            sumZ += v.z;
        }

        const double invCount = 1.0 / static_cast<double>(mapData.vertices.size());
        const double avgX = static_cast<double>(sumX) * invCount;
        const double avgY = static_cast<double>(sumY) * invCount;
        const double avgZ = static_cast<double>(sumZ) * invCount;

        logger.debug(std::format("  Bounds: X[{}, {}] Y[{}, {}] Z[{}, {}]", minX, maxX, minY, maxY,
                                 minZ, maxZ));
        logger.debug(std::format("  Average: X[{:.2f}] Y[{:.2f}] Z[{:.2f}]", avgX, avgY, avgZ));
    }

    return !mapData.vertices.empty();
}

bool BLVMap::parseFaces(const std::vector<uint8_t>& data, size_t& offset)
{
    // BLV face structure (96 bytes per face in MM7):
    // Bytes 0-59: Face header
    //   0x00-0x03: normalX (int32, fixed-point /65536)
    //   0x04-0x07: normalY (int32, fixed-point /65536)
    //   0x08-0x0B: normalZ (int32, fixed-point /65536)
    //   0x0C-0x0F: normalDistance (int32)
    //   0x10-0x1B: zCalc values (12 bytes)
    //   0x1C-0x1F: attributes (uint32)
    //   0x37: numVertices (uint8)
    //   0x38-0x39: textureBitmapId (int16)
    //   0x3A-0x3B: faceExtraId (int16)
    // Bytes 60-95: Embedded vertex data (indices, UV coords)

    constexpr size_t faceStructSize = 96;

    auto isValidFaceAt = [&](size_t off) -> bool {
        if (off + faceStructSize > data.size())
        {
            return false;
        }
        const uint8_t* candidate = data.data() + off;
        int32_t nx = *reinterpret_cast<const int32_t*>(candidate);
        int32_t ny = *reinterpret_cast<const int32_t*>(candidate + 4);
        int32_t nz = *reinterpret_cast<const int32_t*>(candidate + 8);
        uint8_t nv = candidate[55];

        // Normal components should be in fixed-point range for unit vectors
        int32_t absNx = nx < 0 ? -nx : nx;
        int32_t absNy = ny < 0 ? -ny : ny;
        int32_t absNz = nz < 0 ? -nz : nz;
        int32_t maxComponent = std::max({absNx, absNy, absNz});

        // Valid unit normal has max component between ~20000 and 70000
        bool validNormal = (maxComponent >= 20000 && maxComponent <= 70000 && absNx <= 70000 &&
                            absNy <= 70000 && absNz <= 70000);
        bool validVerts = (nv >= 3 && nv <= 30);

        return validNormal && validVerts;
    };

    // Prefer deterministic parsing based on header counts; fall back to heuristic scan.
    if (mapData.faceCount > 0)
    {
        size_t expectedEnd = offset + static_cast<size_t>(mapData.faceCount) * faceStructSize;
        if (expectedEnd > data.size() || !isValidFaceAt(offset))
        {
            // There may be padding between vertices and faces; scan forward a bit.
            size_t searchStart = offset;
            size_t searchEnd = std::min(offset + 128, data.size() - faceStructSize);

            for (size_t searchOff = searchStart; searchOff < searchEnd; searchOff += 2)
            {
                if (isValidFaceAt(searchOff))
                {
                    offset = searchOff;
                    logger.debug(std::format("Found face data at 0x{:X} ({} bytes after vertices)",
                                             offset, offset - searchStart));
                    break;
                }
            }
        }
    }
    else
    {
        // No reliable face count; scan for first valid face
        size_t searchStart = offset;
        size_t searchEnd = std::min(offset + 128, data.size() - faceStructSize);
        for (size_t searchOff = searchStart; searchOff < searchEnd; searchOff += 2)
        {
            if (isValidFaceAt(searchOff))
            {
                offset = searchOff;
                logger.debug(std::format("Found face data at 0x{:X} ({} bytes after vertices)", offset,
                                         offset - searchStart));
                break;
            }
        }
    }

    size_t faceStartOffset = offset;
    uint32_t parsedFaces = 0;
    constexpr uint32_t maxFaces = 50000;
    uint32_t faceLimit = (mapData.faceCount > 0) ? mapData.faceCount : maxFaces;

    mapData.faces.reserve(std::min(faceLimit, maxFaces));

    uint64_t totalRawIndices = 0;
    uint64_t outOfRangeIndices = 0;
    uint64_t facesWithOutOfRange = 0;
    uint16_t minRawIndex = std::numeric_limits<uint16_t>::max();
    uint16_t maxRawIndex = 0;
    bool haveInRangeRaw = false;

    while (offset + faceStructSize <= data.size() && parsedFaces < faceLimit)
    {
        if (!isValidFaceAt(offset))
        {
            break;
        }
        const uint8_t* faceData = data.data() + offset;

        // Read face header
        int32_t normalX = *reinterpret_cast<const int32_t*>(faceData + 0x00);
        int32_t normalY = *reinterpret_cast<const int32_t*>(faceData + 0x04);
        int32_t normalZ = *reinterpret_cast<const int32_t*>(faceData + 0x08);
        uint8_t numVertices = faceData[55]; // numVertices at byte 55 (0x37)

        ParsedFace face;
        face.normalX = normalX;
        face.normalY = normalY;
        face.normalZ = normalZ;
        face.normalDistance = *reinterpret_cast<const int32_t*>(faceData + 0x0C);
        face.zCalc1 = *reinterpret_cast<const int32_t*>(faceData + 0x10);
        face.zCalc2 = *reinterpret_cast<const int32_t*>(faceData + 0x14);
        face.zCalc3 = *reinterpret_cast<const int32_t*>(faceData + 0x18);
        face.attributes = *reinterpret_cast<const uint32_t*>(faceData + 0x1C);
        face.numVertices = numVertices;
        face.textureId = *reinterpret_cast<const int16_t*>(faceData + 56); // textureBitmapId
        face.faceExtraId = *reinterpret_cast<const int16_t*>(faceData + 58); // faceExtraId

        // Store embedded raw bytes (0x3C-0x5F)
        std::memcpy(face.embeddedData.data(), faceData + 0x3C, face.embeddedData.size());

        // Extract vertex indices from embedded data (bytes 0x3C+)
        face.vertexIndices.resize(numVertices);
        const uint16_t* embeddedIndices = reinterpret_cast<const uint16_t*>(faceData + 0x3C);
        const size_t embeddedCount = face.embeddedData.size() / sizeof(uint16_t);

        // Quick sanity: inspect the first numVertices raw indices for ranges
        bool faceOutOfRange = false;
        const size_t rawCount = std::min<size_t>(numVertices, embeddedCount);
        for (size_t i = 0; i < rawCount; i++)
        {
            uint16_t idx = embeddedIndices[i];
            totalRawIndices++;
            if (idx >= mapData.vertices.size())
            {
                outOfRangeIndices++;
                faceOutOfRange = true;
            }
            else
            {
                haveInRangeRaw = true;
                minRawIndex = std::min(minRawIndex, idx);
                maxRawIndex = std::max(maxRawIndex, idx);
            }
        }
        if (faceOutOfRange)
        {
            facesWithOutOfRange++;
        }

        // Heuristic: take the first numVertices indices that are within range
        uint8_t foundIndices = 0;
        for (size_t i = 0; i < embeddedCount && foundIndices < numVertices; i++)
        {
            uint16_t idx = embeddedIndices[i];
            if (idx < mapData.vertices.size())
            {
                face.vertexIndices[foundIndices++] = idx;
            }
        }

        // Fill remaining indices with 0 to avoid out-of-range access
        for (uint8_t i = foundIndices; i < numVertices; i++)
        {
            face.vertexIndices[i] = 0;
        }

        // Best-effort UV/offset parsing from embedded words
        if (numVertices > 0 && embeddedCount >= numVertices)
        {
            size_t cursor = numVertices;
            size_t remaining = (embeddedCount > cursor) ? (embeddedCount - cursor) : 0;

            if (remaining >= static_cast<size_t>(numVertices) * 4)
            {
                face.xOffsets.resize(numVertices);
                face.yOffsets.resize(numVertices);
                face.uCoords.resize(numVertices);
                face.vCoords.resize(numVertices);

                for (uint8_t i = 0; i < numVertices; i++)
                {
                    face.xOffsets[i] = static_cast<int16_t>(embeddedIndices[cursor++]);
                }
                for (uint8_t i = 0; i < numVertices; i++)
                {
                    face.yOffsets[i] = static_cast<int16_t>(embeddedIndices[cursor++]);
                }
                for (uint8_t i = 0; i < numVertices; i++)
                {
                    face.uCoords[i] = static_cast<int16_t>(embeddedIndices[cursor++]);
                }
                for (uint8_t i = 0; i < numVertices; i++)
                {
                    face.vCoords[i] = static_cast<int16_t>(embeddedIndices[cursor++]);
                }
            }
            else if (remaining >= static_cast<size_t>(numVertices) * 2)
            {
                face.uCoords.resize(numVertices);
                face.vCoords.resize(numVertices);
                for (uint8_t i = 0; i < numVertices; i++)
                {
                    face.uCoords[i] = static_cast<int16_t>(embeddedIndices[cursor++]);
                }
                for (uint8_t i = 0; i < numVertices; i++)
                {
                    face.vCoords[i] = static_cast<int16_t>(embeddedIndices[cursor++]);
                }
            }
        }

        mapData.faces.push_back(std::move(face));
        offset += faceStructSize;
        parsedFaces++;
    }

    mapData.faceCount = parsedFaces;
    logger.debug(std::format("Parsed {} faces (96-byte each) from 0x{:X} to 0x{:X}",
                             mapData.faces.size(), faceStartOffset, offset));
    if (totalRawIndices > 0)
    {
        double outPct = (static_cast<double>(outOfRangeIndices) * 100.0) /
                        static_cast<double>(totalRawIndices);
        if (!haveInRangeRaw)
        {
            logger.debug(std::format("Face raw indices: total={}, out-of-range={} ({:.2f}%)",
                                     totalRawIndices, outOfRangeIndices, outPct));
        }
        else
        {
            logger.debug(std::format(
                "Face raw indices: total={}, out-of-range={} ({:.2f}%), in-range min={}, max={}, faces with OOR={}",
                totalRawIndices, outOfRangeIndices, outPct, minRawIndex, maxRawIndex,
                facesWithOutOfRange));
        }
    }

    return !mapData.faces.empty();
}

bool BLVMap::parseFaceVertexData(const std::vector<uint8_t>& data, size_t& offset)
{
    // Vertex indices are now embedded in 96-byte face structures
    // This function is kept for compatibility but does nothing
    (void)data;
    (void)offset;
    return true;
}

bool BLVMap::parseSectors(const std::vector<uint8_t>& data, size_t& offset)
{
    // Sector header structure (24 bytes):
    // 0x00-0x01: floorFaceCount (int16)
    // 0x02-0x03: wallFaceCount (int16)
    // 0x04-0x05: ceilingFaceCount (int16)
    // 0x06-0x07: floorFaceCount2 (int16, duplicate)
    // 0x08-0x09: liquidFaceCount (int16)
    // 0x0A-0x0B: portalFaceCount (int16)
    // 0x0C-0x0D: decorCount (int16)
    // 0x0E-0x0F: lightCount (int16)
    // 0x10-0x11: bspLeafCount (int16)
    // 0x12-0x17: unknown (6 bytes)

    constexpr size_t sectorHeaderSize = 24;

    if (mapData.sectorCount == 0 || mapData.sectorCount > 1000)
    {
        logger.debug(
            std::format("Skipping sector parsing: count {} seems invalid", mapData.sectorCount));
        return false;
    }

    if (data.size() < offset + mapData.sectorCount * sectorHeaderSize)
    {
        logger.warning("BLV data too small for sector headers");
        return false;
    }

    mapData.sectors.reserve(mapData.sectorCount);

    // First pass: read sector headers to know array sizes
    struct SectorHeader
    {
        int16_t floorFaceCount;
        int16_t wallFaceCount;
        int16_t ceilingFaceCount;
        int16_t floorFaceCount2;
        int16_t liquidFaceCount;
        int16_t portalFaceCount;
        int16_t decorCount;
        int16_t lightCount;
        int16_t bspLeafCount;
        int16_t unknown[3];
    };

    std::vector<SectorHeader> headers;
    headers.reserve(mapData.sectorCount);

    size_t headerOffset = offset;
    for (uint32_t i = 0; i < mapData.sectorCount; i++)
    {
        SectorHeader hdr;
        std::memcpy(&hdr, data.data() + headerOffset, sizeof(SectorHeader));
        headers.push_back(hdr);
        headerOffset += sectorHeaderSize;
    }

    offset = headerOffset;

    // Second pass: read variable-length arrays for each sector
    for (uint32_t i = 0; i < mapData.sectorCount; i++)
    {
        const auto& hdr = headers[i];
        ParsedSector sector;

        // Read floor face IDs
        if (hdr.floorFaceCount > 0 && hdr.floorFaceCount < 10000)
        {
            sector.floorFaceIds.resize(hdr.floorFaceCount);
            for (int16_t j = 0; j < hdr.floorFaceCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.floorFaceIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read wall face IDs
        if (hdr.wallFaceCount > 0 && hdr.wallFaceCount < 10000)
        {
            sector.wallFaceIds.resize(hdr.wallFaceCount);
            for (int16_t j = 0; j < hdr.wallFaceCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.wallFaceIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read ceiling face IDs
        if (hdr.ceilingFaceCount > 0 && hdr.ceilingFaceCount < 10000)
        {
            sector.ceilingFaceIds.resize(hdr.ceilingFaceCount);
            for (int16_t j = 0; j < hdr.ceilingFaceCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.ceilingFaceIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read liquid face IDs
        if (hdr.liquidFaceCount > 0 && hdr.liquidFaceCount < 10000)
        {
            sector.liquidFaceIds.resize(hdr.liquidFaceCount);
            for (int16_t j = 0; j < hdr.liquidFaceCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.liquidFaceIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read portal face IDs
        if (hdr.portalFaceCount > 0 && hdr.portalFaceCount < 10000)
        {
            sector.portalFaceIds.resize(hdr.portalFaceCount);
            for (int16_t j = 0; j < hdr.portalFaceCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.portalFaceIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read decor IDs
        if (hdr.decorCount > 0 && hdr.decorCount < 10000)
        {
            sector.decorIds.resize(hdr.decorCount);
            for (int16_t j = 0; j < hdr.decorCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.decorIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read light IDs
        if (hdr.lightCount > 0 && hdr.lightCount < 1000)
        {
            sector.lightIds.resize(hdr.lightCount);
            for (int16_t j = 0; j < hdr.lightCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.lightIds[j] = *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        // Read BSP leaf IDs
        if (hdr.bspLeafCount > 0 && hdr.bspLeafCount < 10000)
        {
            sector.bspLeafIds.resize(hdr.bspLeafCount);
            for (int16_t j = 0; j < hdr.bspLeafCount; j++)
            {
                if (offset + 2 <= data.size())
                {
                    sector.bspLeafIds[j] =
                        *reinterpret_cast<const uint16_t*>(data.data() + offset);
                    offset += 2;
                }
            }
        }

        mapData.sectors.push_back(std::move(sector));
    }

    logger.debug(std::format("Parsed {} sectors", mapData.sectors.size()));
    return !mapData.sectors.empty();
}

bool BLVMap::parseFaceExtras(const std::vector<uint8_t>& data, size_t& offset)
{
    int32_t maxExtraId = -1;
    for (const auto& face : mapData.faces)
    {
        if (face.faceExtraId > maxExtraId)
        {
            maxExtraId = face.faceExtraId;
        }
    }

    if (maxExtraId < 0)
    {
        return false;
    }

    constexpr size_t extraSize = sizeof(BLVFaceExtraRaw);
    static_assert(extraSize == 0x24, "BLVFaceExtraRaw expected size 0x24");

    const uint32_t extraCount = static_cast<uint32_t>(maxExtraId) + 1;

    size_t cursor = offset;
    size_t required = cursor + static_cast<size_t>(extraCount) * extraSize;
    if (required > data.size())
    {
        return false;
    }

    std::vector<BLVFaceExtraRaw> extras;
    extras.reserve(extraCount);

    for (uint32_t i = 0; i < extraCount; i++)
    {
        BLVFaceExtraRaw extra;
        std::memcpy(&extra, data.data() + cursor, extraSize);
        extras.push_back(extra);
        cursor += extraSize;
    }

    mapData.faceExtras = std::move(extras);
    offset = cursor;

    logger.debug(std::format("Parsed {} face extras (0x24 bytes each)", mapData.faceExtras.size()));
    return !mapData.faceExtras.empty();
}

bool BLVMap::parseDoors(const std::vector<uint8_t>& data, size_t& offset)
{
    size_t cursor = offset;
    if (cursor + 4 > data.size())
    {
        return false;
    }

    uint32_t doorCount = *reinterpret_cast<const uint32_t*>(data.data() + cursor);
    if (doorCount > 2000)
    {
        return false;
    }

    cursor += 4;

    std::vector<ParsedDoor> doors;
    doors.reserve(doorCount);

    for (uint32_t i = 0; i < doorCount; i++)
    {
        if (cursor + sizeof(BLVDoor) > data.size())
        {
            return false;
        }

        BLVDoor header;
        std::memcpy(&header, data.data() + cursor, sizeof(BLVDoor));
        cursor += sizeof(BLVDoor);

        ParsedDoor door;
        door.header = header;

        auto readIds = [&](int16_t count, std::vector<uint16_t>& out) -> bool {
            if (count <= 0)
            {
                return true;
            }
            if (count > 10000)
            {
                return false;
            }
            if (cursor + static_cast<size_t>(count) * 2 > data.size())
            {
                return false;
            }
            out.resize(count);
            for (int16_t j = 0; j < count; j++)
            {
                out[j] = *reinterpret_cast<const uint16_t*>(data.data() + cursor);
                cursor += 2;
            }
            return true;
        };

        if (!readIds(header.numVertices, door.vertexIds) ||
            !readIds(header.numFaces, door.faceIds) || !readIds(header.numSectors, door.sectorIds) ||
            !readIds(header.numOffsets, door.offsetIds))
        {
            return false;
        }

        doors.push_back(std::move(door));
    }

    mapData.doors = std::move(doors);
    offset = cursor;

    logger.debug(std::format("Parsed {} doors", mapData.doors.size()));
    return !mapData.doors.empty() || doorCount == 0;
}

bool BLVMap::parseSpawns(const std::vector<uint8_t>& data, size_t& offset)
{
    size_t cursor = offset;
    if (cursor + 4 > data.size())
    {
        return false;
    }

    uint32_t spawnCount = *reinterpret_cast<const uint32_t*>(data.data() + cursor);
    if (spawnCount > 5000)
    {
        return false;
    }

    cursor += 4;

    constexpr size_t spawnSize = sizeof(BLVSpawnPoint);
    static_assert(spawnSize == 22, "BLVSpawnPoint expected size 22");

    if (cursor + spawnCount * spawnSize > data.size())
    {
        return false;
    }

    std::vector<BLVSpawnPoint> spawns;
    spawns.reserve(spawnCount);

    for (uint32_t i = 0; i < spawnCount; i++)
    {
        BLVSpawnPoint spawn;
        std::memcpy(&spawn, data.data() + cursor, spawnSize);
        spawns.push_back(spawn);
        cursor += spawnSize;
    }

    mapData.spawns = std::move(spawns);
    offset = cursor;

    logger.debug(std::format("Parsed {} spawns", mapData.spawns.size()));
    return !mapData.spawns.empty() || spawnCount == 0;
}

bool BLVMap::parseItems(const std::vector<uint8_t>& data, size_t& offset)
{
    size_t cursor = offset;
    if (cursor + 4 > data.size())
    {
        return false;
    }

    uint32_t itemCount = *reinterpret_cast<const uint32_t*>(data.data() + cursor);
    if (itemCount > 5000)
    {
        return false;
    }

    cursor += 4;

    constexpr size_t itemSize = sizeof(BLVItemRaw);
    static_assert(itemSize == 0x24, "BLVItemRaw expected size 0x24");

    if (cursor + itemCount * itemSize > data.size())
    {
        return false;
    }

    std::vector<BLVItemRaw> items;
    items.reserve(itemCount);

    for (uint32_t i = 0; i < itemCount; i++)
    {
        BLVItemRaw item;
        std::memcpy(&item, data.data() + cursor, itemSize);
        items.push_back(item);
        cursor += itemSize;
    }

    mapData.items = std::move(items);
    offset = cursor;

    logger.debug(std::format("Parsed {} items (raw)", mapData.items.size()));
    return !mapData.items.empty() || itemCount == 0;
}

bool BLVMap::parseLights(const std::vector<uint8_t>& data, size_t& offset)
{
    // BLV light structure (16 bytes):
    // 0x00-0x01: x (int16)
    // 0x02-0x03: y (int16)
    // 0x04-0x05: z (int16)
    // 0x06-0x07: radius (int16)
    // 0x08: red (uint8)
    // 0x09: green (uint8)
    // 0x0A: blue (uint8)
    // 0x0B: type (uint8)
    // 0x0C-0x0D: attributes (uint16)
    // 0x0E-0x0F: brightness (int16)

    constexpr size_t lightSize = sizeof(BLVLight);
    static_assert(lightSize == 16, "BLVLight should be 16 bytes");

    // Prefer header count when it looks valid and fits in the remaining buffer
    if (mapData.lightCount > 0 && mapData.lightCount < 10000 &&
        offset + mapData.lightCount * lightSize <= data.size())
    {
        mapData.lights.reserve(mapData.lightCount);
        for (uint32_t i = 0; i < mapData.lightCount; i++)
        {
            BLVLight light;
            std::memcpy(&light, data.data() + offset, lightSize);
            mapData.lights.push_back(light);
            offset += lightSize;
        }
        logger.debug(std::format("Parsed {} lights (header count)", mapData.lights.size()));
        return !mapData.lights.empty();
    }

    // Fall back to heuristic scan if header count is missing or invalid

    size_t maxPossibleLights = (data.size() - offset) / lightSize;
    if (maxPossibleLights == 0)
    {
        logger.debug("No space for light data");
        return false;
    }

    // Read lights until we hit invalid data or reasonable limit
    mapData.lights.reserve(std::min(maxPossibleLights, size_t(1000)));

    for (size_t i = 0; i < maxPossibleLights && i < 1000; i++)
    {
        if (offset + lightSize > data.size())
        {
            break;
        }

        BLVLight light;
        std::memcpy(&light, data.data() + offset, lightSize);

        // Validate light data - check if coordinates are reasonable
        if (light.x < -30000 || light.x > 30000 || light.y < -30000 || light.y > 30000 ||
            light.z < -30000 || light.z > 30000 || light.radius < 0 || light.radius > 10000)
        {
            // Probably hit end of light data
            break;
        }

        mapData.lights.push_back(light);
        offset += lightSize;
    }

    mapData.lightCount = static_cast<uint32_t>(mapData.lights.size());
    logger.debug(std::format("Parsed {} lights", mapData.lights.size()));

    return !mapData.lights.empty();
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
