// SPDX-License-Identifier: MIT
#include "physics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "outdoor_terrain.hpp"

namespace runeharbor::engine
{

namespace
{

constexpr float MIN_MOVE_DISTANCE = 0.5f;
constexpr float CLOSEST_DIST = 0.5f;
constexpr float ALLOWED_OVERSHOOT = -100.0f;
constexpr float DAMPING_FACTOR = 0.89263916f;

struct Vector3
{
    float x, y, z;
    Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vector3& operator+=(const Vector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
    float lengthSq() const { return dot(*this); }
    float length() const { return std::sqrt(lengthSq()); }
    void normalize()
    {
        float len = length();
        if (len > 0.0f) { x /= len; y /= len; z /= len; }
    }
};

Vector3 operator*(float s, const Vector3& v) { return {v.x * s, v.y * s, v.z * s}; }

bool hasShorterSolution(float a, float b, float c, float currentSolution, float& outNewSolution)
{
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return false;

    float sqrtD = std::sqrt(discriminant);
    float alpha1 = (-b - sqrtD) / (2.0f * a);
    float alpha2 = (-b + sqrtD) / (2.0f * a);

    if (alpha1 > alpha2)
        std::swap(alpha1, alpha2);

    if (alpha1 > 0.0f && alpha1 < currentSolution)
    {
        outNewSolution = alpha1;
        return true;
    }

    if (alpha2 > 0.0f && alpha2 < currentSolution)
    {
        outNewSolution = alpha2;
        return true;
    }

    return false;
}

bool collideWithLine(const Vector3& p1, const Vector3& p2, const Vector3& spherePos, const Vector3& dir, float radius, float currentDist, float& newDist, Vector3& intersectionPoint)
{
    Vector3 edge = p2 - p1;
    Vector3 sphereToVertex = p1 - spherePos;
    float edgeLengthSq = edge.lengthSq();
    if (edgeLengthSq < 0.0001f) return false;
    
    float edgeDotDir = edge.dot(dir);
    float edgeDotSphereToVertex = edge.dot(sphereToVertex);
    float sphereToVertexLengthSq = sphereToVertex.lengthSq();

    float a = edgeLengthSq * -dir.lengthSq() + (edgeDotDir * edgeDotDir);
    float b = edgeLengthSq * (2.0f * dir.dot(sphereToVertex)) - (2.0f * edgeDotDir * edgeDotSphereToVertex);
    float c = edgeLengthSq * (radius * radius - sphereToVertexLengthSq) + (edgeDotSphereToVertex * edgeDotSphereToVertex);

    if (hasShorterSolution(a, b, c, currentDist, newDist))
    {
        float f = (edgeDotDir * newDist - edgeDotSphereToVertex) / edgeLengthSq;
        if (f >= 0.0f && f <= 1.0f)
        {
            intersectionPoint = p1 + edge * f;
            return true;
        }
    }
    return false;
}

template<typename VertexList>
bool containsPoint(const formats::ParsedFace& face, const VertexList& vertices, const Vector3& point)
{
    float nx = std::abs(face.normalFX);
    float ny = std::abs(face.normalFY);
    float nz = std::abs(face.normalFZ);
    
    int axis = (nx > ny && nx > nz) ? 0 : (ny > nz ? 1 : 2);
    
    bool inside = false;
    const size_t numVerts = face.vertexIndices.size();
    if (numVerts < 3) return false;

    for (size_t i = 0, j = numVerts - 1; i < numVerts; j = i++)
    {
        const auto& vi = vertices[face.vertexIndices[i]];
        const auto& vj = vertices[face.vertexIndices[j]];
        
        float vxi, vyi, vxj, vyj, px, py;
        if (axis == 0) { 
            vxi = static_cast<float>(vi.y); vyi = static_cast<float>(vi.z); 
            vxj = static_cast<float>(vj.y); vyj = static_cast<float>(vj.z); 
            px = point.y; py = point.z;
        } else if (axis == 1) { 
            vxi = static_cast<float>(vi.x); vyi = static_cast<float>(vi.z); 
            vxj = static_cast<float>(vj.x); vyj = static_cast<float>(vj.z); 
            px = point.x; py = point.z;
        } else { 
            vxi = static_cast<float>(vi.x); vyi = static_cast<float>(vi.y); 
            vxj = static_cast<float>(vj.x); vyj = static_cast<float>(vj.y); 
            px = point.x; py = point.y;
        }
        
        if (((vyi > py) != (vyj > py)) && (px < (vxj - vxi) * (py - vyi) / (vyj - vyi) + vxi))
        {
            inside = !inside;
        }
    }
    return inside;
}

template<typename VertexList>
bool collideSphereWithFace(const formats::ParsedFace& face, const VertexList& vertices,
                           const Vector3& pos, float radius, const Vector3& dir,
                           float& moveDistance, Vector3& collisionPoint)
{
    float dirNormalDot = dir.dot({face.normalFX, face.normalFY, face.normalFZ});
    if (dirNormalDot > -0.001f)
        return false;

    float centerFaceDist = pos.dot({face.normalFX, face.normalFY, face.normalFZ}) - face.normalFDist;
    float distToPlane = (centerFaceDist - radius) / -dirNormalDot;
    
    if (distToPlane < -radius)
        return false; 

    if (distToPlane > 65536.0f) return false;

    Vector3 projectedPos = pos + dir * distToPlane - Vector3{face.normalFX, face.normalFY, face.normalFZ} * radius;

    if (containsPoint(face, vertices, projectedPos))
    {
        if (distToPlane < moveDistance)
        {
            moveDistance = distToPlane;
            collisionPoint = projectedPos;
            return true;
        }
    }

    bool colliding = false;
    float currentBestDist = moveDistance;
    Vector3 bestColPos;

    float a = dir.lengthSq();
    for (uint16_t idx : face.vertexIndices)
    {
        const auto& v = vertices[idx];
        Vector3 vertPos{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
        float b = 2.0f * dir.dot(pos - vertPos);
        float c = (vertPos - pos).lengthSq() - radius * radius;
        float newDist;
        if (hasShorterSolution(a, b, c, currentBestDist, newDist))
        {
            currentBestDist = newDist;
            colliding = true;
            bestColPos = vertPos;
        }
    }

    const size_t numVerts = face.vertexIndices.size();
    for (size_t i = 0; i < numVerts; ++i)
    {
        size_t j = (i + 1) % numVerts;
        const auto& v1 = vertices[face.vertexIndices[i]];
        const auto& v2 = vertices[face.vertexIndices[j]];
        Vector3 vert1{static_cast<float>(v1.x), static_cast<float>(v1.y), static_cast<float>(v1.z)};
        Vector3 vert2{static_cast<float>(v2.x), static_cast<float>(v2.y), static_cast<float>(v2.z)};
        
        float newDist;
        Vector3 intersect;
        if (collideWithLine(vert1, vert2, pos, dir, radius, currentBestDist, newDist, intersect))
        {
            currentBestDist = newDist;
            colliding = true;
            bestColPos = intersect;
        }
    }

    if (colliding)
    {
        moveDistance = currentBestDist;
        collisionPoint = bestColPos;
        return true;
    }
    return false;
}

enum class HitType { None, Face, Terrain };
struct HitInfo
{
    HitType type = HitType::None;
    Vector3 normal = {0,0,1};
};

struct CollisionState
{
    Vector3 positionLo;
    Vector3 positionHi;
    float radiusLo;
    float radiusHi;
    Vector3 velocity;
    Vector3 direction;
    float speed;
    float moveDistance;
    float adjustedMoveDistance;
    Vector3 collisionPos;
    HitInfo hit;
    float heightOffset;
};

template<typename VertexList>
void processFaces(CollisionState& state, const std::vector<formats::ParsedFace>& faces, const VertexList& vertices)
{
    for (const auto& face : faces)
    {
        if (face.isPortal() || face.isInvisible() || face.isWater() || face.isLava())
            continue;

        float minZ = std::min(state.positionLo.z - state.radiusLo, state.positionHi.z - state.radiusHi);
        float maxZ = std::max(state.positionLo.z + state.radiusLo, state.positionHi.z + state.radiusHi);
        float minX = state.positionLo.x - state.radiusLo;
        float maxX = state.positionLo.x + state.radiusLo;
        float minY = state.positionLo.y - state.radiusLo;
        float maxY = state.positionLo.y + state.radiusLo;
        
        Vector3 newPosLo = state.positionLo + state.direction * state.moveDistance;
        minX = std::min(minX, newPosLo.x - state.radiusLo);
        maxX = std::max(maxX, newPosLo.x + state.radiusLo);
        minY = std::min(minY, newPosLo.y - state.radiusLo);
        maxY = std::max(maxY, newPosLo.y + state.radiusLo);

        if (maxX < face.minX || minX > face.maxX || maxY < face.minY || minY > face.maxY || maxZ < face.minZ || minZ > face.maxZ)
            continue;

        float moveDist = state.adjustedMoveDistance;
        Vector3 colPos;
        if (collideSphereWithFace(face, vertices, state.positionLo, state.radiusLo, state.direction, moveDist, colPos))
        {
            if (moveDist > ALLOWED_OVERSHOOT)
            {
                state.adjustedMoveDistance = moveDist;
                state.collisionPos = colPos;
                state.hit.type = HitType::Face;
                state.hit.normal = {face.normalFX, face.normalFY, face.normalFZ};
                state.heightOffset = 0.0f;
            }
        }
        
        moveDist = state.adjustedMoveDistance;
        if (collideSphereWithFace(face, vertices, state.positionHi, state.radiusHi, state.direction, moveDist, colPos))
        {
            if (moveDist > ALLOWED_OVERSHOOT)
            {
                state.adjustedMoveDistance = moveDist;
                state.collisionPos = colPos;
                state.hit.type = HitType::Face;
                state.hit.normal = {face.normalFX, face.normalFY, face.normalFZ};
                state.heightOffset = state.positionHi.z - state.positionLo.z;
            }
        }
    }
}

} // namespace

void updatePartyPhysics(game::Party& party, const formats::BLVMapData* blv,
                        const formats::ODMMapData* odm, float deltaMs, const PhysicsConfig& config)
{
    float dt = deltaMs / 1000.0f;
    if (dt <= 0.0f) return;

    Vector3 pos = {party.worldX(), party.worldY(), party.worldZ()};
    Vector3 vel = {party.velocityX(), party.velocityY(), party.velocityZ()};

    // Apply gravity
    vel.z += config.gravity * dt;
    if (vel.z < config.maxFallSpeed)
        vel.z = config.maxFallSpeed;

    CollisionState state;
    state.radiusLo = config.playerRadius;
    state.radiusHi = config.playerRadius;

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        state.positionLo = pos + Vector3{0, 0, state.radiusLo};
        state.positionHi = pos + Vector3{0, 0, config.playerHeight - state.radiusLo};
        state.velocity = vel;
        state.speed = vel.length();
        
        if (state.speed < 0.01f) break;
        
        state.direction = vel * (1.0f / state.speed);
        state.moveDistance = state.speed * dt;
        if (state.moveDistance <= MIN_MOVE_DISTANCE) break;

        state.adjustedMoveDistance = state.moveDistance;
        state.hit.type = HitType::None;

        if (blv)
        {
            processFaces(state, blv->faces, blv->vertices);
        }
        else if (odm)
        {
            for (const auto& bldg : odm->buildings)
            {
                processFaces(state, bldg.faces, bldg.vertices);
            }
            
            // Check outdoor terrain
            Vector3 newPosLo = state.positionLo + state.direction * state.adjustedMoveDistance;
            float terrainZ = sampleOutdoorTerrainHeight(*odm, newPosLo.x, newPosLo.y);
            if (newPosLo.z - state.radiusLo <= terrainZ)
            {
                float distToFloor = (terrainZ - (state.positionLo.z - state.radiusLo)) / state.direction.z;
                if (distToFloor >= 0.0f && distToFloor < state.adjustedMoveDistance)
                {
                    state.adjustedMoveDistance = distToFloor;
                    state.hit.type = HitType::Terrain;
                    state.hit.normal = {0.0f, 0.0f, 1.0f}; // Simplified terrain normal
                    state.collisionPos = {newPosLo.x, newPosLo.y, terrainZ};
                }
            }
        }

        if (state.adjustedMoveDistance > state.moveDistance)
            state.adjustedMoveDistance = state.moveDistance;

        Vector3 adjustedPos = pos + state.direction * std::max(0.0f, state.adjustedMoveDistance - CLOSEST_DIST);
        pos = adjustedPos;

        if (state.adjustedMoveDistance >= state.moveDistance)
            break;

        if (state.hit.type == HitType::Terrain)
        {
            if (vel.z < 0) vel.z = 0;
            pos.z = state.collisionPos.z;
        }
        else if (state.hit.type == HitType::Face)
        {
            bool slopeTooSteep = state.hit.normal.z > 0.0f && state.hit.normal.z < 0.70767211914f;

            Vector3 newPosLo = state.positionLo + state.direction * state.moveDistance;
            Vector3 slidePlaneOrigin = state.collisionPos - Vector3{0, 0, state.heightOffset};
            Vector3 slidePlaneNormal = adjustedPos + Vector3{0, 0, state.radiusLo} - slidePlaneOrigin;
            slidePlaneNormal.normalize();

            float destPlaneDist = (newPosLo - slidePlaneOrigin).dot(slidePlaneNormal);
            Vector3 newDestination = newPosLo - slidePlaneNormal * destPlaneDist;
            Vector3 newDirection = newDestination - slidePlaneOrigin;

            if (slopeTooSteep && newDirection.z > 0)
                newDirection.z = 0;

            newDirection.normalize();

            if (slopeTooSteep)
                vel += Vector3{state.hit.normal.x, state.hit.normal.y, -2.0f} * 10.0f;

            vel = newDirection * newDirection.dot(vel);

            if (state.hit.normal.z >= 0.70767211914f) // Walkable floor
            {
                if (vel.z < 0) vel.z = 0;
            }
        }
        
        vel *= DAMPING_FACTOR;
        dt -= (state.adjustedMoveDistance / state.speed);
    }

    party.setWorldPosition(pos.x, pos.y, pos.z);
    party.setVelocityX(vel.x);
    party.setVelocityY(vel.y);
    party.setVelocityZ(vel.z);
}

} // namespace runeharbor::engine
