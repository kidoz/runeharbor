// SPDX-License-Identifier: MIT
#include "virtual_filesystem.hpp"

#include <algorithm>
#include <format>

#include <cctype>

#include "../formats/game_lod_archive.hpp"
#include "../formats/image_lod_archive.hpp"
#include "../formats/lod_archive.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::engine
{

VirtualFileSystem::VirtualFileSystem(util::ILogger& logger) : logger(logger) {}

VirtualFileSystem::~VirtualFileSystem()
{
    unmountAll();
}

bool VirtualFileSystem::mountArchive(const std::filesystem::path& archivePath)
{
    auto archive = std::make_unique<formats::LODArchive>(logger);

    if (!archive->open(archivePath))
    {
        logger.error(std::format("Failed to mount archive: {}", archivePath.string()));
        return false;
    }

    archivePaths.push_back(archivePath);
    archives.push_back(std::move(archive));

    logger.info(std::format("Mounted text archive: {} ({} total text archives)",
                            archivePath.filename().string(), archives.size()));
    return true;
}

bool VirtualFileSystem::mountImageArchive(const std::filesystem::path& archivePath)
{
    auto archive = std::make_unique<formats::ImageLODArchive>(logger);

    if (!archive->open(archivePath))
    {
        logger.error(std::format("Failed to mount image archive: {}", archivePath.string()));
        return false;
    }

    archivePaths.push_back(archivePath);
    imageArchives.push_back(std::move(archive));

    logger.info(std::format("Mounted image archive: {} ({} total image archives)",
                            archivePath.filename().string(), imageArchives.size()));
    return true;
}

bool VirtualFileSystem::mountGameArchive(const std::filesystem::path& archivePath)
{
    auto archive = std::make_unique<formats::GameLODArchive>(logger);

    if (!archive->open(archivePath))
    {
        logger.error(std::format("Failed to mount game archive: {}", archivePath.string()));
        return false;
    }

    archivePaths.push_back(archivePath);
    gameArchives.push_back(std::move(archive));

    logger.info(std::format("Mounted game archive: {} ({} total game archives)",
                            archivePath.filename().string(), gameArchives.size()));
    return true;
}

void VirtualFileSystem::unmountAll()
{
    if (!archives.empty() || !imageArchives.empty() || !gameArchives.empty())
    {
        logger.info("Unmounted all archives");
    }

    archives.clear();
    imageArchives.clear();
    gameArchives.clear();
    archivePaths.clear();
}

std::optional<std::vector<uint8_t>> VirtualFileSystem::readFile(const std::string& filename)
{
    if (archives.empty() && imageArchives.empty() && gameArchives.empty())
    {
        logger.warning("Cannot read file: no archives mounted");
        return std::nullopt;
    }

    // Search text archives first
    for (size_t i = 0; i < archives.size(); i++)
    {
        auto data = archives[i]->extractFile(filename);
        if (data.has_value())
        {
            logger.debug(
                std::format("Read {} from text archive ({} bytes)", filename, data->size()));
            return data;
        }
    }

    // Then search image archives
    for (size_t i = 0; i < imageArchives.size(); i++)
    {
        auto data = imageArchives[i]->extractFile(filename);
        if (data.has_value())
        {
            logger.debug(
                std::format("Read {} from image archive ({} bytes)", filename, data->size()));
            return data;
        }
    }

    // Finally search game archives
    for (size_t i = 0; i < gameArchives.size(); i++)
    {
        auto data = gameArchives[i]->extractFile(filename);
        if (data.has_value())
        {
            logger.debug(
                std::format("Read {} from game archive ({} bytes)", filename, data->size()));
            return data;
        }
    }

    logger.warning(std::format("File not found in any archive: {}", filename));
    return std::nullopt;
}

bool VirtualFileSystem::fileExists(const std::string& filename)
{
    auto equalsIgnoreCase = [](const std::string& a, const std::string& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); i++)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
            {
                return false;
            }
        }
        return true;
    };

    // Check text archives
    for (const auto& archive : archives)
    {
        auto files = archive->listFiles();
        for (const auto& name : files)
        {
            if (equalsIgnoreCase(name, filename))
            {
                return true;
            }
        }
    }

    // Check image archives
    for (const auto& archive : imageArchives)
    {
        auto files = archive->listFiles();
        for (const auto& name : files)
        {
            if (equalsIgnoreCase(name, filename))
            {
                return true;
            }
        }
    }

    // Check game archives
    for (const auto& archive : gameArchives)
    {
        auto files = archive->listFiles();
        for (const auto& name : files)
        {
            if (equalsIgnoreCase(name, filename))
            {
                return true;
            }
        }
    }

    return false;
}

std::vector<std::string> VirtualFileSystem::listAllFiles() const
{
    std::vector<std::string> allFiles;

    // Collect files from text archives
    for (const auto& archive : archives)
    {
        auto files = archive->listFiles();
        allFiles.insert(allFiles.end(), files.begin(), files.end());
    }

    // Collect files from image archives
    for (const auto& archive : imageArchives)
    {
        auto files = archive->listFiles();
        allFiles.insert(allFiles.end(), files.begin(), files.end());
    }

    // Collect files from game archives
    for (const auto& archive : gameArchives)
    {
        auto files = archive->listFiles();
        allFiles.insert(allFiles.end(), files.begin(), files.end());
    }

    // Remove duplicates and sort
    std::sort(allFiles.begin(), allFiles.end());
    allFiles.erase(std::unique(allFiles.begin(), allFiles.end()), allFiles.end());

    return allFiles;
}

size_t VirtualFileSystem::getMountedArchiveCount() const
{
    return archives.size() + imageArchives.size() + gameArchives.size();
}

std::optional<formats::ImageFileHeader> VirtualFileSystem::getImageInfo(const std::string& filename)
{
    for (auto& archive : imageArchives)
    {
        auto info = archive->getFileInfo(filename);
        if (info.has_value())
        {
            return info;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> VirtualFileSystem::getImagePalette(const std::string& filename)
{
    for (auto& archive : imageArchives)
    {
        auto pal = archive->extractPalette(filename);
        if (pal.has_value())
        {
            return pal;
        }
    }
    return std::nullopt;
}

} // namespace runeharbor::engine
