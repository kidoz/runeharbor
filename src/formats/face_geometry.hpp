// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "blv_map.hpp"

namespace runeharbor::formats
{

// Result of face triangulation
struct TriangulatedFace
{
    // Triangle indices (local to face vertices, 0-indexed)
    // Every 3 consecutive values form a triangle
    std::vector<uint8_t> triangleIndices;

    // Number of triangles
    size_t triangleCount() const { return triangleIndices.size() / 3; }
};

// Render vertex with position, normal, and UV
struct RenderVertex
{
    float x = 0.0f, y = 0.0f, z = 0.0f;    // Position
    float nx = 0.0f, ny = 0.0f, nz = 0.0f; // Normal
    float u = 0.0f, v = 0.0f;              // Texture coordinates
};

// 3D vector for geometry calculations
struct Vec3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}

    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }

    Vec3 cross(const Vec3& other) const
    {
        return Vec3(y * other.z - z * other.y, z * other.x - x * other.z,
                    x * other.y - y * other.x);
    }

    float length() const;
    Vec3 normalized() const;
};

/**
 * Utility class for face geometry operations.
 *
 * Provides triangulation, coordinate conversion, and geometry analysis
 * for BLV map faces.
 */
class FaceGeometry
{
  public:
    // Fixed-point scale factor for BLV normals
    static constexpr float FixedPointScale = 65536.0f;

    /**
     * Triangulate a convex N-gon using fan method.
     *
     * For a polygon with vertices 0..n-1:
     * - Creates (n-2) triangles
     * - Triangle i = {0, i+1, i+2} for i in 0..(n-3)
     *
     * @param vertexCount Number of vertices in the polygon (must be >= 3)
     * @return Triangulated face with local indices
     */
    static TriangulatedFace triangulateConvex(uint8_t vertexCount);

    /**
     * Convert BLV fixed-point normal to float Vec3.
     *
     * BLV normals are stored as fixed-point values where 65536 = 1.0
     *
     * @param nx Normal X component (fixed-point)
     * @param ny Normal Y component (fixed-point)
     * @param nz Normal Z component (fixed-point)
     * @return Floating-point normal vector
     */
    static Vec3 normalToVec3(int32_t nx, int32_t ny, int32_t nz);

    /**
     * Convert BLV vertex to world-space Vec3.
     *
     * @param v BLV vertex (int16 coordinates)
     * @return Floating-point position
     */
    static Vec3 vertexToWorld(const BLVVertex& v);

    /**
     * Build render vertices for a face.
     *
     * Creates vertices with position, normal, and UV coordinates
     * suitable for rendering.
     *
     * @param face Parsed face with vertex indices and UVs
     * @param vertices All map vertices
     * @return Vector of render vertices (one per face vertex)
     */
    static std::vector<RenderVertex> buildRenderVertices(const ParsedFace& face,
                                                         std::span<const BLVVertex> vertices);

    /**
     * Compute approximate face area for LOD calculations.
     *
     * Uses triangulation and sums triangle areas.
     *
     * @param face Parsed face
     * @param vertices All map vertices
     * @return Face area in world units squared
     */
    static float computeFaceArea(const ParsedFace& face, std::span<const BLVVertex> vertices);

    /**
     * Check if a face polygon is convex.
     *
     * Tests if all cross products of consecutive edges have the same sign.
     *
     * @param face Parsed face
     * @param vertices All map vertices
     * @return true if convex, false otherwise
     */
    static bool isConvex(const ParsedFace& face, std::span<const BLVVertex> vertices);

    /**
     * Compute the centroid of a face.
     *
     * @param face Parsed face
     * @param vertices All map vertices
     * @return Centroid position
     */
    static Vec3 computeCentroid(const ParsedFace& face, std::span<const BLVVertex> vertices);

    /**
     * Get the face normal as Vec3.
     *
     * @param face Parsed face
     * @return Normal vector (unit length)
     */
    static Vec3 getFaceNormal(const ParsedFace& face);
};

} // namespace runeharbor::formats
