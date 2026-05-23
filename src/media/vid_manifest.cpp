// SPDX-License-Identifier: MIT
#include "vid_manifest.hpp"

#include <algorithm>
#include <cstdint>

#include <cctype>
#include <fstream>

namespace runeharbor::media
{

namespace
{
bool isPrintable(uint8_t c)
{
    return c >= 32 && c <= 126;
}

bool isVideoName(const std::string& name)
{
    if (name.size() < 5)
    {
        return false;
    }

    std::string lower = name;
    for (char& c : lower)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower.size() >= 4 &&
        (lower.substr(lower.size() - 4) == ".smk" || lower.substr(lower.size() - 4) == ".bik"))
    {
        return true;
    }
    return false;
}
} // namespace

bool VidManifest::load(const std::filesystem::path& path)
{
    clipRefs.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }

    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!file || count == 0 || count > 10000)
    {
        return false;
    }

    clipRefs.reserve(count);
    for (uint32_t i = 0; i < count; i++)
    {
        char nameBuffer[40] = {};
        uint32_t offset = 0;
        file.read(nameBuffer, sizeof(nameBuffer));
        file.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        if (!file)
        {
            clipRefs.clear();
            return false;
        }

        std::string name;
        for (char c : nameBuffer)
        {
            if (c == '\0')
            {
                break;
            }
            const auto byte = static_cast<uint8_t>(c);
            if (!isPrintable(byte))
            {
                name.clear();
                break;
            }
            name.push_back(c);
        }

        if (isVideoName(name))
        {
            clipRefs.push_back({std::move(name)});
        }
    }

    if (clipRefs.empty())
    {
        return false;
    }

    // Deduplicate while preserving order
    std::vector<VideoClipRef> unique;
    unique.reserve(clipRefs.size());
    for (const auto& clip : clipRefs)
    {
        auto it = std::find_if(unique.begin(), unique.end(), [&](const VideoClipRef& existing)
                               { return existing.name == clip.name; });
        if (it == unique.end())
        {
            unique.push_back(clip);
        }
    }
    clipRefs = std::move(unique);

    return true;
}

} // namespace runeharbor::media
