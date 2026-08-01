// SPDX-License-Identifier: MIT
#include "frame_tables.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>
#include <string>
#include <vector>

#include <cstring>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

namespace
{
std::string extractName(const char* buf, size_t maxLen)
{
    size_t len = 0;
    while (len < maxLen && buf[len] != '\0')
    {
        len++;
    }
    return std::string(buf, len);
}

std::string trimCopy(std::string_view sv)
{
    size_t start = 0;
    size_t end = sv.size();
    while (start < end && (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\r'))
    {
        start++;
    }
    while (end > start && (sv[end - 1] == ' ' || sv[end - 1] == '\t' || sv[end - 1] == '\r'))
    {
        end--;
    }
    return std::string(sv.substr(start, end - start));
}

bool parseInt(std::string_view sv, int& out)
{
    const std::string trimmed = trimCopy(sv);
    if (trimmed.empty())
    {
        return false;
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    if (ec != std::errc{} || ptr != trimmed.data() + trimmed.size())
    {
        return false;
    }
    out = value;
    return true;
}

std::vector<std::string> splitLine(std::string_view line)
{
    const char delimiter = (line.find('\t') != std::string_view::npos) ? '\t' : ',';
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size())
    {
        size_t end = line.find(delimiter, start);
        if (end == std::string_view::npos)
        {
            end = line.size();
        }
        fields.push_back(trimCopy(line.substr(start, end - start)));
        if (end == line.size())
        {
            break;
        }
        start = end + 1;
    }
    return fields;
}

bool isCommentOrEmpty(std::string_view line)
{
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty())
    {
        return true;
    }
    if (trimmed[0] == ';' || trimmed[0] == '#')
    {
        return true;
    }
    if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/')
    {
        return true;
    }
    return false;
}

bool isLikelyHeader(const std::vector<std::string>& fields)
{
    if (fields.empty())
    {
        return false;
    }
    std::string first = fields.front();
    for (char& c : first)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return first == "icon" || first == "iconname" || first == "texture" || first == "texturename" ||
           first == "name";
}

template <typename T>
T readLE(const uint8_t* p);

template <>
uint32_t readLE<uint32_t>(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

bool SpriteFrameTable::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();
    iconIndex_.clear();

    // Header is two counts: the frames themselves, then a trailing group lookup
    // table we do not need. Reading only the first count leaves every field after
    // the two name strings shifted by four bytes.
    constexpr size_t kHeaderSize = 8;
    if (data.size() < kHeaderSize)
    {
        return false;
    }

    uint32_t count = readLE<uint32_t>(data.data());
    size_t expectedSize = kHeaderSize + static_cast<size_t>(count) * sizeof(SpriteFrameEntryRaw);
    if (count == 0 || data.size() < expectedSize)
    {
        return false;
    }

    entries_.reserve(count);
    const uint8_t* ptr = data.data() + kHeaderSize;

    for (uint32_t i = 0; i < count; i++)
    {
        SpriteFrameEntryRaw raw;
        std::memcpy(&raw, ptr, sizeof(SpriteFrameEntryRaw));
        ptr += sizeof(SpriteFrameEntryRaw);

        SpriteFrameEntry frame;
        frame.iconName = extractName(raw.iconName, 12);
        frame.textureName = extractName(raw.textureName, 12);
        frame.paletteId = raw.paletteId;
        frame.paletteIndex = raw.paletteIndex;
        frame.attributes = raw.attributes;
        // Scale is 16.16 fixed point; a zero scale means "unscaled".
        frame.scale = (raw.scale != 0) ? static_cast<float>(raw.scale) / 65536.0f : 1.0f;
        frame.animDuration = raw.animDuration;
        frame.animLength = raw.animLength;
        frame.animOffset = 0;
        frame.lightRadius = raw.glowRadius;
        frame.lightR = 0;
        frame.lightG = 0;
        frame.lightB = 0;
        entries_.push_back(std::move(frame));
    }

    rebuildIconIndex();
    return true;
}

bool SpriteFrameTable::parseText(std::string_view text)
{
    entries_.clear();
    iconIndex_.clear();
    if (text.empty())
    {
        return false;
    }

    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const std::string_view line = text.substr(pos, end - pos);
        pos = (end == text.size()) ? text.size() + 1 : end + 1;

        if (isCommentOrEmpty(line))
        {
            continue;
        }

        auto fields = splitLine(line);
        if (fields.size() < 2)
        {
            continue;
        }
        if (isLikelyHeader(fields))
        {
            continue;
        }

        SpriteFrameEntry frame;
        frame.iconName = fields[0];
        frame.textureName = fields[1];

        int value = 0;
        if (fields.size() > 2 && parseInt(fields[2], value))
            frame.paletteId = static_cast<int16_t>(value);
        if (fields.size() > 3 && parseInt(fields[3], value))
            frame.paletteIndex = static_cast<int16_t>(value);
        if (fields.size() > 4 && parseInt(fields[4], value))
            frame.attributes = static_cast<uint32_t>(value);
        if (fields.size() > 5 && parseInt(fields[5], value))
            frame.animDuration = static_cast<int16_t>(value);
        if (fields.size() > 6 && parseInt(fields[6], value))
            frame.animLength = static_cast<int16_t>(value);
        if (fields.size() > 7 && parseInt(fields[7], value))
            frame.animOffset = static_cast<int16_t>(value);
        if (fields.size() > 8 && parseInt(fields[8], value))
            frame.lightRadius = static_cast<int16_t>(value);

        entries_.push_back(std::move(frame));
    }

    rebuildIconIndex();
    return !entries_.empty();
}

const SpriteFrameEntry* SpriteFrameTable::findEntryByIcon(std::string_view iconName) const
{
    const auto it = iconIndex_.find(util::toLower(iconName));
    if (it != iconIndex_.end() && it->second < entries_.size())
    {
        return &entries_[it->second];
    }
    return nullptr;
}

void SpriteFrameTable::rebuildIconIndex()
{
    iconIndex_.clear();
    iconIndex_.reserve(entries_.size());
    for (size_t i = 0; i < entries_.size(); i++)
    {
        if (!entries_[i].iconName.empty())
        {
            iconIndex_.try_emplace(util::toLower(entries_[i].iconName), i);
        }
    }
}

bool TextureFrameTable::parse(const std::vector<uint8_t>& data)
{
    entries_.clear();

    if (data.size() < 4)
    {
        return false;
    }

    uint32_t count = readLE<uint32_t>(data.data());
    size_t expectedSize = 4 + static_cast<size_t>(count) * sizeof(TextureFrameRaw);
    if (data.size() < expectedSize)
    {
        return false;
    }

    entries_.reserve(count);
    const uint8_t* ptr = data.data() + 4;

    for (uint32_t i = 0; i < count; i++)
    {
        TextureFrameRaw raw;
        std::memcpy(&raw, ptr, sizeof(TextureFrameRaw));
        ptr += sizeof(TextureFrameRaw);

        TextureFrame frame;
        frame.textureName = extractName(raw.textureName, 12);
        frame.animDuration = raw.animDuration;
        frame.totalDuration = raw.totalDuration;
        frame.flags = raw.flags;
        entries_.push_back(std::move(frame));
    }

    return true;
}

bool TextureFrameTable::parseText(std::string_view text)
{
    entries_.clear();
    if (text.empty())
    {
        return false;
    }

    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const std::string_view line = text.substr(pos, end - pos);
        pos = (end == text.size()) ? text.size() + 1 : end + 1;

        if (isCommentOrEmpty(line))
        {
            continue;
        }

        auto fields = splitLine(line);
        if (fields.empty())
        {
            continue;
        }
        if (isLikelyHeader(fields))
        {
            continue;
        }

        TextureFrame frame;
        frame.textureName = fields[0];

        int value = 0;
        if (fields.size() > 1 && parseInt(fields[1], value))
            frame.animDuration = static_cast<int16_t>(value);
        if (fields.size() > 2 && parseInt(fields[2], value))
            frame.totalDuration = static_cast<int16_t>(value);
        if (fields.size() > 3 && parseInt(fields[3], value))
            frame.flags = static_cast<uint16_t>(value);

        entries_.push_back(std::move(frame));
    }

    return !entries_.empty();
}

std::string animatedTextureName(const std::string& baseName, uint32_t ticks, int frameCount)
{
    if (frameCount <= 1 || baseName.empty())
    {
        return baseName;
    }
    // RE FUN_0044e1c6: frame index = (tick >> 3) % groupLength.
    const int frame = static_cast<int>((ticks >> 3) % static_cast<uint32_t>(frameCount));
    // Split off any existing numeric suffix so we don't stack them.
    size_t baseLen = baseName.size();
    while (baseLen > 0 && baseName[baseLen - 1] >= '0' && baseName[baseLen - 1] <= '9')
    {
        --baseLen;
    }
    return std::format("{}{:02d}", baseName.substr(0, baseLen), frame + 1);
}

} // namespace runeharbor::formats
