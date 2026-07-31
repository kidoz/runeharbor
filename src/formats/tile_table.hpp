// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace runeharbor::formats
{

// ---- Global terrain tile table (dtile.bin) ----
//
// Every outdoor map stores terrain as one byte per grid cell. That byte is a
// *map-local* tile id which only becomes meaningful once combined with the four
// tilesets named in the ODM header: it selects a tileset and a variant within it
// (base ground, shoreline, road piece, grass↔dirt transition, …). dtile.bin is
// the global table those local ids resolve into, and it is what maps a resolved
// tile id to an actual BITMAPS.LOD texture name.

// Raw on-disk layout: 26 bytes per entry, preceded by a uint32 entry count.
#pragma pack(push, 1)
struct TileDataRaw
{
    char textureName[16]; // 0x00 — texture name in BITMAPS.LOD, e.g. "grastyl"
    uint16_t tileId;      // 0x10 — unused in MM7 data (always 0)
    uint16_t bitmapId;    // 0x12 — unused in MM7 data (always 0)
    uint16_t tileset;     // 0x14 — tileset this tile belongs to (255 = invalid)
    uint16_t variant;     // 0x16 — variant within the tileset (255 = invalid)
    uint16_t flags;       // 0x18 — TileFlag bits (water, shore, wavy, …)
};
#pragma pack(pop)
static_assert(sizeof(TileDataRaw) == 26, "TileDataRaw must be 26 bytes");

// Tile id 0 is the "no tile" entry; MM7 marks it with tileset/variant 255.
inline constexpr uint16_t kInvalidTileset = 255;
inline constexpr uint16_t kInvalidTileVariant = 255;

// Variant 0 is the plain ground tile for terrain tilesets and the four-way
// crossing for road tilesets — either way it is the tileset's base tile.
inline constexpr uint16_t kBaseTileVariant = 0;

struct TileEntry
{
    std::string textureName;
    uint16_t tileset = kInvalidTileset;
    uint16_t variant = kInvalidTileVariant;
    uint16_t flags = 0;
};

// Tile flag bits that matter to the renderer.
inline constexpr uint16_t kTileFlagWater = 0x0002;
inline constexpr uint16_t kTileFlagWavy = 0x0020;
inline constexpr uint16_t kTileFlagDontDraw = 0x0040;
inline constexpr uint16_t kTileFlagShore = 0x0100;

class TileTable
{
  public:
    bool parse(const std::vector<uint8_t>& data);

    bool empty() const { return entries_.empty(); }
    const std::vector<TileEntry>& entries() const { return entries_; }

    /// Tile for a resolved global id; returns a shared empty entry when out of range.
    const TileEntry& tile(int tileId) const;

    /// Id of the base (variant 0) tile of a tileset, or -1 when the tileset is absent.
    int baseTileId(uint16_t tileset) const;

    /// Resolve one map-local tile byte using the map's four tileset ids.
    /// Returns -1 when the byte cannot be resolved.
    int resolveLocalTileId(const std::array<uint8_t, 4>& mapTilesets, uint8_t localTileId) const;

  private:
    std::vector<TileEntry> entries_;
};

} // namespace runeharbor::formats
