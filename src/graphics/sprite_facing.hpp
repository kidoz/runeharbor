// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>

#include "math3d.hpp"

namespace runeharbor::graphics
{

/// Result of resolving a camera-relative facing into a drawable frame direction.
///
/// MM7 stores only 5 of the 8 directional frames for most creatures; the three
/// remaining octants are produced by horizontally mirroring frames 1..3. This
/// captures both the frame to draw (1-based, matching the `<base><NN>` naming the
/// engine uses) and whether it must be flipped.
struct SpriteFacing
{
    int frameDirection = 1; // 1..5 (1-based frame suffix)
    bool flipU = false;     // horizontally mirror the sprite for this octant
};

/// MM7 uses a 0..2047 fixed-point angle ("turn units"). The full circle is 2048.
inline constexpr int kMM7FullTurn = 2048;

/// Compute the camera-relative octant (0..7) for a sprite.
///
/// @param spriteFacingMM7 The sprite's own heading in MM7 turn units (0..2047).
/// @param cameraPos        Camera world position.
/// @param spritePos        Sprite world position.
/// The horizontal plane is X/Z (Y is up), matching the render coordinate system.
inline int cameraRelativeOctant(int spriteFacingMM7, const Vec3& cameraPos, const Vec3& spritePos)
{
    const float dx = cameraPos.x - spritePos.x;
    const float dz = cameraPos.z - spritePos.z;
    const float camAngle = std::atan2(dx, dz);

    // Map (-pi, pi] to 0..2047 turn units.
    const float halfTurn = static_cast<float>(kMM7FullTurn) / 2.0f;
    int camFacing = static_cast<int>((camAngle + static_cast<float>(M_PI)) * halfTurn /
                                     static_cast<float>(M_PI)) %
                    kMM7FullTurn;
    if (camFacing < 0)
    {
        camFacing += kMM7FullTurn;
    }

    // Difference, rounded to the nearest of 8 octants (each octant = 256 units;
    // +128 biases the truncation to round-to-nearest).
    return ((spriteFacingMM7 - camFacing + kMM7FullTurn + 128) >> 8) & 7;
}

/// Map an octant (0..7) to a drawable frame direction, applying the 5-to-8 mirror
/// scheme: octants 0..4 use frames 1..5 directly; octants 5,6,7 reuse frames
/// 4,3,2 mirrored (i.e. 8 - octant) so a creature seen from its left looks like
/// the flipped right-side frame.
inline SpriteFacing resolveSpriteFacing(int octant)
{
    octant &= 7;
    SpriteFacing facing;
    if (octant <= 4)
    {
        facing.frameDirection = octant + 1; // 1..5
        facing.flipU = false;
    }
    else
    {
        facing.frameDirection = (8 - octant) + 1; // 5->4, 6->3, 7->2
        facing.flipU = true;
    }
    return facing;
}

} // namespace runeharbor::graphics
