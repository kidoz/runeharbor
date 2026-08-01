// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace runeharbor::formats
{

// ---- Sprite Frame Table (dsft.bin) ----
//
// File layout: uint32 frameCount, uint32 groupCount, frameCount × 60-byte frames,
// groupCount × uint16 group lookup entries. The trailing lookup table maps sprite
// group ids to their first frame and is not needed for name-based lookups.

// Raw on-disk layout: 60 bytes per entry
struct SpriteFrameEntryRaw
{
    char iconName[12];        // 0x00 — only set on the first frame of a sequence
    char textureName[12];     // 0x0C — sprite name in SPRITES.LOD, without octant suffix
    int16_t octantSprites[8]; // 0x18 — per-octant sprite ids, resolved at load time
    int32_t scale;            // 0x28 — 16.16 fixed-point billboard scale
    uint32_t attributes;      // 0x2C — SpriteFrameAttribute bits
    int16_t glowRadius;       // 0x30
    int16_t paletteId;        // 0x32
    int16_t paletteIndex;     // 0x34
    int16_t animDuration;     // 0x36 — frame duration, 1/16th of a second
    int16_t animLength;       // 0x38 — total sequence duration, first frame only
    int16_t reserved;         // 0x3A
};
static_assert(sizeof(SpriteFrameEntryRaw) == 60, "SpriteFrameEntryRaw must be 60 bytes");

// Sprite frame attribute bits.
enum SpriteFrameAttribute : uint32_t
{
    kSpriteFrameHasMore = 0x00001,     // animation continues with further frames
    kSpriteFrameLit = 0x00002,         // self-lit; not dimmed by scene lighting
    kSpriteFrameFirst = 0x00004,       // first frame of a sequence
    kSpriteFrameSingleImage = 0x00010, // one image shared by all eight octants
    kSpriteFrameCenter = 0x00020,      // anchor at the sprite's centre, not its base
    kSpriteFrameFidget = 0x00040,
    kSpriteFrameLoaded = 0x00080,
    kSpriteFrameMirror0 = 0x00100, // octant N is mirrored: 0x100 << N
    kSpriteFrameThreeImages = 0x10000,
    kSpriteFrameGlowing = 0x20000,
    kSpriteFrameTransparent = 0x40000,
};

/// Whether the frame's sprite must be mirrored when seen from `octant` (0..7).
inline bool spriteFrameMirrorsOctant(uint32_t attributes, int octant)
{
    return (attributes & (kSpriteFrameMirror0 << (octant & 7))) != 0;
}

// Clean parsed representation
struct SpriteFrameEntry
{
    std::string iconName;
    std::string textureName;
    int16_t paletteId = 0;
    int16_t paletteIndex = 0;
    uint32_t attributes = 0;
    float scale = 1.0f;
    int16_t animDuration = 0;
    int16_t animLength = 0;
    int16_t animOffset = 0;
    int16_t lightRadius = 0;
    uint8_t lightR = 0;
    uint8_t lightG = 0;
    uint8_t lightB = 0;
};

class SpriteFrameTable
{
  public:
    bool parse(const std::vector<uint8_t>& data);
    bool parseText(std::string_view text);
    const std::vector<SpriteFrameEntry>& entries() const { return entries_; }
    const SpriteFrameEntry* findEntryByIcon(std::string_view iconName) const;

  private:
    std::vector<SpriteFrameEntry> entries_;
};

// Resolves the animated texture name for a billboard. MM7 animates sprites by
// cycling a numeric frame suffix on the texture name using the time base
// (tick >> 3) % groupLength (RE: FUN_0044e1c6). When the frame table entry has
// a positive animLength, this returns the base name with the current frame
// suffix; otherwise it returns the name unchanged.
//
// frameCount: the number of frames in the animation group (typically derived
//             from animLength / max(1, animDuration), clamped to [1, 8]).
std::string animatedTextureName(const std::string& baseName, uint32_t ticks, int frameCount);

// ---- Texture Frame Table (dtft.bin) ----

// Raw on-disk layout: 20 bytes per entry
struct TextureFrameRaw
{
    char textureName[12];  // 0x00
    int16_t animDuration;  // 0x0C
    int16_t totalDuration; // 0x0E
    uint16_t flags;        // 0x10
    uint16_t reserved;     // 0x12
};
static_assert(sizeof(TextureFrameRaw) == 20, "TextureFrameRaw must be 20 bytes");

// Clean parsed representation
struct TextureFrame
{
    std::string textureName;
    int16_t animDuration = 0;
    int16_t totalDuration = 0;
    uint16_t flags = 0;
};

class TextureFrameTable
{
  public:
    bool parse(const std::vector<uint8_t>& data);
    bool parseText(std::string_view text);
    const std::vector<TextureFrame>& entries() const { return entries_; }

  private:
    std::vector<TextureFrame> entries_;
};

} // namespace runeharbor::formats
