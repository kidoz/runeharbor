// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <unordered_set>

#include "math3d.hpp"

namespace runeharbor::formats
{
struct BLVMapData;
struct ODMMapData;
} // namespace runeharbor::formats

namespace runeharbor::graphics
{

/// Coarse line-of-sight test used to keep billboards (monsters, decorations,
/// dropped items) from drawing through opaque world geometry.
///
/// The test casts a single segment from the camera to the billboard's base
/// position. It intentionally ignores partial occlusion: a sprite whose base is
/// visible still draws in full. This is a first-pass replacement for MM7's
/// per-facet sprite tests and works for both the GPU and 2D render paths.
///
/// All positions are gameplay-space (X/Y ground plane, Z up).
bool isOccludedByIndoorGeometry(const formats::BLVMapData& data, const Vec3& cameraPos,
                                const Vec3& targetPos,
                                const std::unordered_set<uint16_t>* visibleSectors = nullptr);

/// Same line-of-sight test for outdoor maps: terrain height sampling plus
/// opaque building faces.
bool isOccludedByOutdoorGeometry(const formats::ODMMapData& data, const Vec3& cameraPos,
                                 const Vec3& targetPos);

} // namespace runeharbor::graphics
