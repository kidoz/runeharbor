// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::formats
{
class LODArchive;
class ImageLODArchive;
}

namespace runeharbor::engine
{

/// Virtual file system that can mount LOD archives and provide unified file access
///
/// This class manages multiple LOD archives and provides a unified interface
/// for reading game data files. Files are searched in mounted archives in
/// the order they were mounted.
///
/// Example:
///   VirtualFileSystem vfs(logger);
///   vfs.mountArchive("DATA/Events.lod");
///   vfs.mountArchive("DATA/Icons.lod");
///   auto data = vfs.readFile("SPELLS.TXT");
class VirtualFileSystem
{
public:
    explicit VirtualFileSystem(util::ILogger& logger);
    ~VirtualFileSystem();

    // Non-copyable, non-movable (due to reference member)
    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;
    VirtualFileSystem(VirtualFileSystem&&) noexcept = delete;
    VirtualFileSystem& operator=(VirtualFileSystem&&) noexcept = delete;

    /// Mount a text/data LOD archive (Events.lod, etc.)
    /// Returns true on success, false on failure
    bool mountArchive(const std::filesystem::path& archivePath);

    /// Mount an image LOD archive (BITMAPS.LOD, ICONS.LOD, SPRITES.LOD)
    /// Returns true on success, false on failure
    bool mountImageArchive(const std::filesystem::path& archivePath);

    /// Unmount all archives (both text and image)
    void unmountAll();

    /// Read a file from mounted archives (searches in mount order)
    /// Returns nullopt if file not found in any archive
    std::optional<std::vector<uint8_t>> readFile(const std::string& filename);

    /// Check if a file exists in any mounted archive
    bool fileExists(const std::string& filename);

    /// List all files across all mounted archives (deduplicated)
    std::vector<std::string> listAllFiles() const;

    /// Get number of mounted archives
    size_t getMountedArchiveCount() const;

private:
    util::ILogger& logger;

    // Mounted text/data archives (Events.lod, etc.) - in mount order
    std::vector<std::unique_ptr<formats::LODArchive>> archives;

    // Mounted image archives (BITMAPS.LOD, etc.) - in mount order
    std::vector<std::unique_ptr<formats::ImageLODArchive>> imageArchives;

    // Archive paths for debugging/logging
    std::vector<std::filesystem::path> archivePaths;
};

} // namespace runeharbor::engine
