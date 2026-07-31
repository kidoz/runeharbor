// SPDX-License-Identifier: MIT
#include "tile_table.hpp"

#include <cstring>

namespace runeharbor::formats
{
namespace
{
// Map-local tile bytes below this are already global tile ids.
constexpr int kFirstTilesetTile = 90;
// Each of the map's four tilesets owns this many consecutive local ids.
constexpr int kTilesetSpan = 36;
// Local ids at or above this are unused by MM7 maps.
constexpr int kLastTilesetTile = kFirstTilesetTile + 4 * kTilesetSpan; // 234

std::string readFixedName(const char* raw, size_t capacity)
{
    const size_t length = ::strnlen(raw, capacity);
    return std::string(raw, length);
}

const TileEntry& emptyTile()
{
    static const TileEntry entry;
    return entry;
}

} // namespace

bool TileTable::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();

    if (data.size() < sizeof(uint32_t))
    {
        return false;
    }

    uint32_t count = 0;
    std::memcpy(&count, data.data(), sizeof(count));

    const size_t required = sizeof(uint32_t) + static_cast<size_t>(count) * sizeof(TileDataRaw);
    if (count == 0 || required > data.size())
    {
        return false;
    }

    entries_.reserve(count);
    for (uint32_t i = 0; i < count; i++)
    {
        TileDataRaw raw = {};
        std::memcpy(&raw, data.data() + sizeof(uint32_t) + static_cast<size_t>(i) * sizeof(raw),
                    sizeof(raw));

        TileEntry entry;
        entry.textureName = readFixedName(raw.textureName, sizeof(raw.textureName));
        entry.tileset = raw.tileset;
        entry.variant = raw.variant;
        entry.flags = raw.flags;
        entries_.push_back(std::move(entry));
    }

    return true;
}

const TileEntry& TileTable::tile(int tileId) const
{
    if (tileId < 0 || static_cast<size_t>(tileId) >= entries_.size())
    {
        return emptyTile();
    }
    return entries_[static_cast<size_t>(tileId)];
}

int TileTable::baseTileId(uint16_t tileset) const
{
    if (tileset == kInvalidTileset)
    {
        return -1;
    }

    for (size_t i = 0; i < entries_.size(); i++)
    {
        const TileEntry& entry = entries_[i];
        if (entry.tileset == tileset && entry.variant == kBaseTileVariant &&
            !entry.textureName.empty())
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int TileTable::resolveLocalTileId(const std::array<uint8_t, 4>& mapTilesets,
                                  uint8_t localTileId) const
{
    if (entries_.empty())
    {
        return -1;
    }

    const int local = static_cast<int>(localTileId);
    if (local < kFirstTilesetTile)
    {
        return local;
    }
    if (local >= kLastTilesetTile)
    {
        return -1;
    }

    const int tilesetIndex = (local - kFirstTilesetTile) / kTilesetSpan;
    const int variantOffset = (local - kFirstTilesetTile) % kTilesetSpan;

    const int base = baseTileId(mapTilesets[static_cast<size_t>(tilesetIndex)]);
    if (base < 0)
    {
        return -1;
    }

    const int resolved = base + variantOffset;
    if (static_cast<size_t>(resolved) >= entries_.size())
    {
        return -1;
    }
    return resolved;
}

} // namespace runeharbor::formats
