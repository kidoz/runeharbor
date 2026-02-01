// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace runeharbor::media
{

struct VideoClipRef
{
    std::string name;
};

class VidManifest
{
  public:
    bool load(const std::filesystem::path& path);
    const std::vector<VideoClipRef>& clips() const { return clipRefs; }

  private:
    std::vector<VideoClipRef> clipRefs;
};

} // namespace runeharbor::media
