#include "../util/string_utils.hpp"
// SPDX-License-Identifier: MIT
#include "face_geometry.hpp"

#include <cmath>

namespace runeharbor::formats
{

float Vec3::length() const
{
    return std::sqrt(x * x + y * y + z * z);
}

Vec3 Vec3::normalized() const
{
    float len = length();
    if (len < 1e-6f)
    {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return Vec3(x / len, y / len, z / len);
}

TriangulatedFace FaceGeometry::triangulateConvex(uint8_t vertexCount)
{
    TriangulatedFace result;

    if (vertexCount < 3)
    {
        return result;
    }

    // Fan triangulation: create (n-2) triangles
    // Triangle i uses vertices {0, i+1, i+2}
    size_t triangleCount = static_cast<size_t>(vertexCount) - 2;
    result.triangleIndices.reserve(triangleCount * 3);

    for (uint8_t i = 1; i < vertexCount - 1; i++)
    {
        result.triangleIndices.push_back(0);
        result.triangleIndices.push_back(i);
        result.triangleIndices.push_back(static_cast<uint8_t>(i + 1));
    }

    return result;
}

Vec3 FaceGeometry::normalToVec3(int32_t nx, int32_t ny, int32_t nz)
{
    return Vec3(static_cast<float>(nx) / FixedPointScale, static_cast<float>(ny) / FixedPointScale,
                static_cast<float>(nz) / FixedPointScale);
}

Vec3 FaceGeometry::vertexToWorld(const BLVVertex& v)
{
    return Vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

std::vector<RenderVertex> FaceGeometry::buildRenderVertices(const ParsedFace& face,
                                                            std::span<const BLVVertex> vertices)
{
    std::vector<RenderVertex> result;
    result.reserve(face.numVertices);

    Vec3 normal = normalToVec3(face.normalX, face.normalY, face.normalZ);

    for (uint8_t i = 0; i < face.numVertices; i++)
    {
        RenderVertex rv;

        // Position from vertex index
        if (i < face.vertexIndices.size() && face.vertexIndices[i] < vertices.size())
        {
            const auto& v = vertices[face.vertexIndices[i]];
            rv.x = static_cast<float>(v.x);
            rv.y = static_cast<float>(v.y);
            rv.z = static_cast<float>(v.z);
        }

        // Normal (shared across all vertices of the face)
        rv.nx = normal.x;
        rv.ny = normal.y;
        rv.nz = normal.z;

        // UV coordinates
        if (i < face.uCoords.size())
        {
            // BLV UVs are typically in a 0-255 or similar range; normalize to 0-1
            rv.u = static_cast<float>(face.uCoords[i]) / 256.0f;
        }
        if (i < face.vCoords.size())
        {
            rv.v = static_cast<float>(face.vCoords[i]) / 256.0f;
        }

        result.push_back(rv);
    }

    return result;
}

float FaceGeometry::computeFaceArea(const ParsedFace& face, std::span<const BLVVertex> vertices)
{
    if (face.numVertices < 3)
    {
        return 0.0f;
    }

    // Get triangulation
    auto triangles = triangulateConvex(face.numVertices);

    float totalArea = 0.0f;

    // Sum up triangle areas
    for (size_t t = 0; t < triangles.triangleCount(); t++)
    {
        uint8_t i0 = triangles.triangleIndices[t * 3];
        uint8_t i1 = triangles.triangleIndices[t * 3 + 1];
        uint8_t i2 = triangles.triangleIndices[t * 3 + 2];

        // Get vertex positions
        if (i0 >= face.vertexIndices.size() || i1 >= face.vertexIndices.size() ||
            i2 >= face.vertexIndices.size())
        {
            continue;
        }

        uint16_t vi0 = face.vertexIndices[i0];
        uint16_t vi1 = face.vertexIndices[i1];
        uint16_t vi2 = face.vertexIndices[i2];

        if (vi0 >= vertices.size() || vi1 >= vertices.size() || vi2 >= vertices.size())
        {
            continue;
        }

        Vec3 p0 = vertexToWorld(vertices[vi0]);
        Vec3 p1 = vertexToWorld(vertices[vi1]);
        Vec3 p2 = vertexToWorld(vertices[vi2]);

        // Triangle area = 0.5 * |cross(p1-p0, p2-p0)|
        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p0;
        Vec3 crossProduct = e1.cross(e2);
        totalArea += crossProduct.length() * 0.5f;
    }

    return totalArea;
}

bool FaceGeometry::isConvex(const ParsedFace& face, std::span<const BLVVertex> vertices)
{
    if (face.numVertices < 3)
    {
        return false;
    }
    if (face.numVertices == 3)
    {
        return true; // Triangles are always convex
    }

    // Get face normal to determine winding direction
    Vec3 faceNormal = normalToVec3(face.normalX, face.normalY, face.normalZ);

    // Check that all cross products of consecutive edges have same sign
    // relative to face normal
    int positiveCount = 0;
    int negativeCount = 0;

    size_t n = face.numVertices;
    for (size_t i = 0; i < n; i++)
    {
        size_t i0 = i;
        size_t i1 = (i + 1) % n;
        size_t i2 = (i + 2) % n;

        if (i0 >= face.vertexIndices.size() || i1 >= face.vertexIndices.size() ||
            i2 >= face.vertexIndices.size())
        {
            return false;
        }

        uint16_t vi0 = face.vertexIndices[i0];
        uint16_t vi1 = face.vertexIndices[i1];
        uint16_t vi2 = face.vertexIndices[i2];

        if (vi0 >= vertices.size() || vi1 >= vertices.size() || vi2 >= vertices.size())
        {
            return false;
        }

        Vec3 p0 = vertexToWorld(vertices[vi0]);
        Vec3 p1 = vertexToWorld(vertices[vi1]);
        Vec3 p2 = vertexToWorld(vertices[vi2]);

        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p1;
        Vec3 cross = e1.cross(e2);

        float dot = cross.dot(faceNormal);
        if (dot > 1e-6f)
        {
            positiveCount++;
        }
        else if (dot < -1e-6f)
        {
            negativeCount++;
        }
    }

    // Convex if all same sign (or zero)
    return (positiveCount == 0) || (negativeCount == 0);
}

Vec3 FaceGeometry::computeCentroid(const ParsedFace& face, std::span<const BLVVertex> vertices)
{
    Vec3 sum(0.0f, 0.0f, 0.0f);

    if (face.numVertices == 0)
    {
        return sum;
    }

    uint8_t validCount = 0;
    for (uint8_t i = 0; i < face.numVertices; i++)
    {
        if (i < face.vertexIndices.size() && face.vertexIndices[i] < vertices.size())
        {
            Vec3 pos = vertexToWorld(vertices[face.vertexIndices[i]]);
            sum = sum + pos;
            validCount++;
        }
    }

    if (validCount > 0)
    {
        return sum * (1.0f / static_cast<float>(validCount));
    }
    return sum;
}

Vec3 FaceGeometry::getFaceNormal(const ParsedFace& face)
{
    return normalToVec3(face.normalX, face.normalY, face.normalZ);
}

} // namespace runeharbor::formats
