// SPDX-License-Identifier: MIT
#include "vid_manifest.hpp"

#include <algorithm>
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

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    std::string current;
    for (size_t i = 0; i < data.size(); i++)
    {
        uint8_t c = data[i];
        if (isPrintable(c))
        {
            current.push_back(static_cast<char>(c));
            continue;
        }

        if (current.size() >= 4 && isVideoName(current))
        {
            clipRefs.push_back({current});
        }
        current.clear();
    }

    if (current.size() >= 4 && isVideoName(current))
    {
        clipRefs.push_back({current});
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
        auto it = std::find_if(unique.begin(), unique.end(),
                               [&](const VideoClipRef& existing)
                               {
                                   return existing.name == clip.name;
                               });
        if (it == unique.end())
        {
            unique.push_back(clip);
        }
    }
    clipRefs = std::move(unique);

    return true;
}

} // namespace runeharbor::media
