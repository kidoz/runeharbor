// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace runeharbor::formats
{

// ---- Sprite Frame Table (dsft.bin) ----

// Raw on-disk layout: 60 bytes per entry
struct SpriteFrameEntryRaw
{
    char iconName[12];    // 0x00
    char textureName[12]; // 0x0C
    int16_t paletteId;    // 0x18
    int16_t paletteIndex; // 0x1A
    uint32_t attributes;  // 0x1C
    int16_t animDuration; // 0x20
    int16_t animLength;   // 0x22
    int16_t animOffset;   // 0x24
    int16_t lightRadius;  // 0x26
    uint8_t lightR;       // 0x28
    uint8_t lightG;       // 0x29
    uint8_t lightB;       // 0x2A
    uint8_t pad;          // 0x2B
    uint8_t reserved[16]; // 0x2C
};
static_assert(sizeof(SpriteFrameEntryRaw) == 60, "SpriteFrameEntryRaw must be 60 bytes");

// Clean parsed representation
struct SpriteFrameEntry
{
    std::string iconName;
    std::string textureName;
    int16_t paletteId = 0;
    int16_t paletteIndex = 0;
    uint32_t attributes = 0;
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
