// SPDX-License-Identifier: MIT
#include "save_game.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <span>
#include <system_error>
#include <utility>

#include <cstring>
#include <fstream>

#include "../formats/game_lod_archive.hpp"
#include "game_world.hpp"
#include "inventory.hpp"

namespace runeharbor::game
{

namespace
{
#pragma pack(push, 1)
struct SaveArchiveHeader
{
    char magic[4];
    char gameId[4];
    char description[80];
    char chapterName[80];
    uint32_t fileSize;
    uint32_t dataStart;
    uint32_t numDirEntries;
    uint8_t reserved[76];
};

struct SaveArchiveDirectoryEntry
{
    char name[16];
    uint32_t offset;
    uint32_t size;
    uint32_t decompressedSize;
    uint32_t reserved;
};

struct SaveArchiveDataHeader
{
    uint32_t uncompressedSize;
    uint32_t flags;
};

struct SaveSlotHeaderBin
{
    char title[20];
    char locationName[20];
    uint32_t gameTimeLow;
    uint32_t gameTimeHigh;
    uint8_t reserved[52];
};
#pragma pack(pop)

static_assert(sizeof(SaveArchiveHeader) == 256);
static_assert(sizeof(SaveArchiveDirectoryEntry) == 32);
static_assert(sizeof(SaveArchiveDataHeader) == 8);
static_assert(sizeof(SaveSlotHeaderBin) == 100);

constexpr uint32_t kInventoryMagic = 0x56494852u; // "RHIV"
constexpr uint32_t kInventoryVersion = 1;
constexpr uint32_t kTimerRuntimeMagic = 0x54525645u; // "EVRT"
constexpr uint32_t kTimerRuntimeVersion = 1;
constexpr size_t kTimerBinSize = 1008;

#pragma pack(push, 1)
struct TimerRuntimeHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t payloadSize;
};
#pragma pack(pop)

static_assert(sizeof(TimerRuntimeHeader) == 12);

bool hasLodMagic(std::span<const uint8_t> data)
{
    return data.size() >= 4 && data[0] == 'L' && data[1] == 'O' && data[2] == 'D' &&
           data[3] == '\0';
}

bool hasRawSaveMagic(std::span<const uint8_t> data)
{
    if (data.size() < sizeof(uint32_t))
    {
        return false;
    }
    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    return magic == SaveHeader::kMagic;
}

std::vector<uint8_t> makeClockBin(uint64_t gameTicks)
{
    std::vector<uint8_t> clock(40, 0);
    const uint32_t low = static_cast<uint32_t>(gameTicks & 0xFFFFFFFFu);
    const uint32_t high = static_cast<uint32_t>(gameTicks >> 32);
    std::memcpy(clock.data(), &low, sizeof(low));
    std::memcpy(clock.data() + 4, &high, sizeof(high));
    return clock;
}

std::vector<uint8_t> makePlaceholderScreenshotPcx()
{
    constexpr uint16_t kWidth = 640;
    constexpr uint16_t kHeight = 480;
    std::array<uint8_t, 128> header{};

    auto writeU16 = [&](size_t offset, uint16_t value)
    {
        header[offset] = static_cast<uint8_t>(value & 0x00FFu);
        header[offset + 1] = static_cast<uint8_t>((value >> 8) & 0x00FFu);
    };

    header[0] = 0x0A;          // PCX manufacturer
    header[1] = 0x05;          // version 3.0+
    header[2] = 0x01;          // RLE encoding
    header[3] = 0x08;          // 8 bits per pixel
    writeU16(4, 0);            // xmin
    writeU16(6, 0);            // ymin
    writeU16(8, kWidth - 1);   // xmax
    writeU16(10, kHeight - 1); // ymax
    writeU16(12, kWidth);      // hres
    writeU16(14, kHeight);     // vres
    header[65] = 1;            // color planes
    writeU16(66, kWidth);      // bytes per line
    writeU16(68, 1);           // palette info (color/BW)
    writeU16(70, kWidth);      // hscreen size
    writeU16(72, kHeight);     // vscreen size

    std::vector<uint8_t> pcx;
    pcx.reserve(128 + 10000 + 1 + (256 * 3));
    pcx.insert(pcx.end(), header.begin(), header.end());

    // Solid black 640x480 image RLE-compressed per PCX rules.
    for (uint16_t y = 0; y < kHeight; y++)
    {
        uint16_t remaining = kWidth;
        while (remaining > 0)
        {
            const uint8_t run = static_cast<uint8_t>(std::min<uint16_t>(remaining, 63));
            pcx.push_back(static_cast<uint8_t>(0xC0u | run));
            pcx.push_back(0x00u);
            remaining = static_cast<uint16_t>(remaining - run);
        }
    }

    // 256-color VGA palette marker + grayscale palette.
    pcx.push_back(0x0Cu);
    for (int i = 0; i < 256; i++)
    {
        const uint8_t c = static_cast<uint8_t>(i);
        pcx.push_back(c);
        pcx.push_back(c);
        pcx.push_back(c);
    }

    return pcx;
}

std::optional<uint64_t> readClockBinTicks(std::span<const uint8_t> clockData)
{
    if (clockData.size() < 8)
    {
        return std::nullopt;
    }

    uint32_t low = 0;
    uint32_t high = 0;
    std::memcpy(&low, clockData.data(), sizeof(low));
    std::memcpy(&high, clockData.data() + 4, sizeof(high));
    return (static_cast<uint64_t>(high) << 32) | static_cast<uint64_t>(low);
}

std::vector<uint8_t> makeTimerBin(const std::vector<uint8_t>* eventRuntimeState)
{
    std::vector<uint8_t> timer(kTimerBinSize, 0);
    if (!eventRuntimeState || eventRuntimeState->empty())
    {
        return timer;
    }

    if (eventRuntimeState->size() > (kTimerBinSize - sizeof(TimerRuntimeHeader)))
    {
        return timer;
    }

    const TimerRuntimeHeader header{
        .magic = kTimerRuntimeMagic,
        .version = kTimerRuntimeVersion,
        .payloadSize = static_cast<uint32_t>(eventRuntimeState->size()),
    };
    std::memcpy(timer.data(), &header, sizeof(header));
    std::memcpy(timer.data() + sizeof(header), eventRuntimeState->data(),
                eventRuntimeState->size());
    return timer;
}

std::optional<std::vector<uint8_t>> extractEventRuntimeFromTimerBin(std::span<const uint8_t> timer)
{
    if (timer.size() < sizeof(TimerRuntimeHeader))
    {
        return std::nullopt;
    }

    TimerRuntimeHeader header{};
    std::memcpy(&header, timer.data(), sizeof(header));
    if (header.magic != kTimerRuntimeMagic || header.version != kTimerRuntimeVersion)
    {
        return std::nullopt;
    }

    if (header.payloadSize == 0 || header.payloadSize > (timer.size() - sizeof(TimerRuntimeHeader)))
    {
        return std::nullopt;
    }

    std::vector<uint8_t> payload(header.payloadSize, 0);
    std::memcpy(payload.data(), timer.data() + sizeof(TimerRuntimeHeader), header.payloadSize);
    return payload;
}

void extractEventRuntimeFromArchive(formats::GameLODArchive& archive,
                                    std::vector<uint8_t>& eventRuntimeState)
{
    if (auto eventBlob = archive.extractFile("eventrt.bin");
        eventBlob.has_value() && !eventBlob->empty())
    {
        eventRuntimeState = *eventBlob;
        return;
    }

    auto timerBlob = archive.extractFile("timer.bin");
    if (!timerBlob.has_value() || timerBlob->empty())
    {
        eventRuntimeState.clear();
        return;
    }

    auto timerPayload = extractEventRuntimeFromTimerBin(*timerBlob);
    if (!timerPayload.has_value())
    {
        eventRuntimeState.clear();
        return;
    }

    eventRuntimeState = std::move(*timerPayload);
}

SaveSlotHeaderBin makeHeaderBin(const SaveHeader& header)
{
    SaveSlotHeaderBin result{};
    std::strncpy(result.title, header.partyLeader, sizeof(result.title) - 1);
    std::strncpy(result.locationName, header.mapName, sizeof(result.locationName) - 1);
    result.gameTimeLow = static_cast<uint32_t>(header.gameTime & 0xFFFFFFFFu);
    result.gameTimeHigh = static_cast<uint32_t>(header.gameTime >> 32);
    return result;
}

std::vector<uint8_t>
buildSaveArchive(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files)
{
    SaveArchiveHeader header{};
    std::memcpy(header.magic, "LOD\0", 4);
    std::memcpy(header.gameId, "mvii", 4);
    std::strncpy(header.description, "RuneHarbor MM7 Save", sizeof(header.description) - 1);
    std::strncpy(header.chapterName, "newmaps for MMVII", sizeof(header.chapterName) - 1);
    header.numDirEntries = static_cast<uint32_t>(files.size());

    const size_t directoryBytes =
        static_cast<size_t>(header.numDirEntries + 1) * sizeof(SaveArchiveDirectoryEntry);
    header.dataStart = static_cast<uint32_t>(sizeof(SaveArchiveHeader) + directoryBytes);

    size_t totalDataBytes = static_cast<size_t>(header.dataStart);
    for (const auto& [_, payload] : files)
    {
        totalDataBytes += sizeof(SaveArchiveDataHeader) + payload.size();
    }
    header.fileSize = static_cast<uint32_t>(totalDataBytes);

    std::vector<uint8_t> archive(totalDataBytes, 0);
    std::memcpy(archive.data(), &header, sizeof(header));

    uint32_t offset = header.dataStart;
    size_t dirOffset = sizeof(SaveArchiveHeader);
    for (const auto& [name, payload] : files)
    {
        SaveArchiveDirectoryEntry entry{};
        std::strncpy(entry.name, name.c_str(), sizeof(entry.name) - 1);
        entry.offset = offset;
        entry.size = static_cast<uint32_t>(sizeof(SaveArchiveDataHeader) + payload.size());
        entry.decompressedSize = static_cast<uint32_t>(payload.size());
        std::memcpy(archive.data() + dirOffset, &entry, sizeof(entry));
        dirOffset += sizeof(SaveArchiveDirectoryEntry);

        SaveArchiveDataHeader dataHeader{};
        dataHeader.uncompressedSize = static_cast<uint32_t>(payload.size());
        dataHeader.flags = 0;
        std::memcpy(archive.data() + offset, &dataHeader, sizeof(dataHeader));
        offset += static_cast<uint32_t>(sizeof(dataHeader));

        if (!payload.empty())
        {
            std::memcpy(archive.data() + offset, payload.data(), payload.size());
            offset += static_cast<uint32_t>(payload.size());
        }
    }

    return archive;
}

std::optional<SaveHeader> readArchiveSaveHeader(util::ILogger& logger, const std::string& path)
{
    formats::GameLODArchive archive(logger);
    if (!archive.open(path))
    {
        return std::nullopt;
    }

    SaveHeader header{};
    header.magic = SaveHeader::kMagic;
    header.version = SaveHeader::kVersion;
    bool found = false;

    if (auto slotHeaderData = archive.extractFile("header.bin");
        slotHeaderData.has_value() && slotHeaderData->size() >= sizeof(SaveSlotHeaderBin))
    {
        SaveSlotHeaderBin slotHeader{};
        std::memcpy(&slotHeader, slotHeaderData->data(), sizeof(slotHeader));
        std::strncpy(header.mapName, slotHeader.locationName, sizeof(header.mapName) - 1);
        std::strncpy(header.partyLeader, slotHeader.title, sizeof(header.partyLeader) - 1);
        header.gameTime = (static_cast<uint64_t>(slotHeader.gameTimeHigh) << 32) |
                          static_cast<uint64_t>(slotHeader.gameTimeLow);
        found = true;
    }

    if (auto partyData = archive.extractFile("party.bin");
        partyData.has_value() && partyData->size() >= sizeof(SaveHeader) &&
        hasRawSaveMagic(*partyData))
    {
        SaveHeader partyHeader{};
        std::memcpy(&partyHeader, partyData->data(), sizeof(partyHeader));
        header = partyHeader;
        found = true;
    }

    return found ? std::optional<SaveHeader>(header) : std::nullopt;
}

std::optional<std::vector<uint8_t>> buildRawSaveFromArchive(formats::GameLODArchive& archive)
{
    auto partyBlob = archive.extractFile("party.bin");
    if (!partyBlob.has_value() || partyBlob->empty())
    {
        return std::nullopt;
    }

    if (hasRawSaveMagic(*partyBlob))
    {
        return std::vector<uint8_t>(*partyBlob);
    }

    SaveHeader header{};
    header.magic = SaveHeader::kMagic;
    header.version = SaveHeader::kVersion;

    if (auto verBlob = archive.extractFile("runeharbor.ver");
        verBlob.has_value() && verBlob->size() >= sizeof(uint32_t))
    {
        uint32_t version = SaveHeader::kVersion;
        std::memcpy(&version, verBlob->data(), sizeof(version));
        if (version >= 1 && version <= SaveHeader::kVersion)
        {
            header.version = version;
        }
    }

    if (auto slotHeaderData = archive.extractFile("header.bin");
        slotHeaderData.has_value() && slotHeaderData->size() >= sizeof(SaveSlotHeaderBin))
    {
        SaveSlotHeaderBin slotHeader{};
        std::memcpy(&slotHeader, slotHeaderData->data(), sizeof(slotHeader));
        std::strncpy(header.mapName, slotHeader.locationName, sizeof(header.mapName) - 1);
        std::strncpy(header.partyLeader, slotHeader.title, sizeof(header.partyLeader) - 1);
        header.gameTime = (static_cast<uint64_t>(slotHeader.gameTimeHigh) << 32) |
                          static_cast<uint64_t>(slotHeader.gameTimeLow);
    }

    if (auto clockBlob = archive.extractFile("clock.bin"); clockBlob.has_value())
    {
        if (auto clockTicks = readClockBinTicks(*clockBlob); clockTicks.has_value())
        {
            header.gameTime = *clockTicks;
        }
    }

    std::vector<uint8_t> raw;
    raw.resize(sizeof(SaveHeader) + partyBlob->size());
    std::memcpy(raw.data(), &header, sizeof(header));
    std::memcpy(raw.data() + sizeof(SaveHeader), partyBlob->data(), partyBlob->size());
    return raw;
}

bool loadArchiveFallbackState(util::ILogger& logger, const std::string& path, GameWorld& world)
{
    formats::GameLODArchive archive(logger);
    if (!archive.open(path))
    {
        return false;
    }

    auto maybeHeader = readArchiveSaveHeader(logger, path);
    if (!maybeHeader.has_value())
    {
        return false;
    }

    auto header = *maybeHeader;
    uint64_t ticks = header.gameTime;
    if (auto clockBlob = archive.extractFile("clock.bin"); clockBlob.has_value())
    {
        if (auto clockTicks = readClockBinTicks(*clockBlob); clockTicks.has_value())
        {
            ticks = *clockTicks;
        }
    }

    world.reset();
    world.calendar().totalTicks = static_cast<int64_t>(ticks);
    world.party().setGameTime(ticks);

    std::string mapName = header.mapName;
    if (mapName.empty())
    {
        mapName = "out01.odm";
    }
    world.setCurrentMap(mapName);

    std::string leader = header.partyLeader;
    if (!leader.empty())
    {
        world.party().member(0).name = leader;
    }

    logger.warning("Loaded save in compatibility mode (header/clock only): " + path);
    return true;
}

void appendU8(std::vector<uint8_t>& out, uint8_t value)
{
    out.push_back(value);
}

void appendI32(std::vector<uint8_t>& out, int32_t value)
{
    uint32_t raw = static_cast<uint32_t>(value);
    out.push_back(static_cast<uint8_t>(raw & 0xFFu));
    out.push_back(static_cast<uint8_t>((raw >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((raw >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((raw >> 24) & 0xFFu));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

bool popU8(const uint8_t*& ptr, const uint8_t* end, uint8_t& value)
{
    if (ptr + 1 > end)
    {
        return false;
    }
    value = ptr[0];
    ptr += 1;
    return true;
}

bool popI32(const uint8_t*& ptr, const uint8_t* end, int32_t& value)
{
    if (ptr + 4 > end)
    {
        return false;
    }
    uint32_t raw = static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
                   (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    value = static_cast<int32_t>(raw);
    return true;
}

bool popU32(const uint8_t*& ptr, const uint8_t* end, uint32_t& value)
{
    if (ptr + 4 > end)
    {
        return false;
    }
    value = static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
            (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    return true;
}

void appendInventoryItem(std::vector<uint8_t>& out, const Item& item)
{
    appendI32(out, item.itemId);
    appendI32(out, item.enchantId);
    appendI32(out, item.specialEnchantId);
    appendI32(out, item.chargeCount);
    appendU8(out, item.identified ? 1u : 0u);
    appendU8(out, item.broken ? 1u : 0u);
    appendU8(out, item.temporaryEnchant ? 1u : 0u);
}

bool popInventoryItem(const uint8_t*& ptr, const uint8_t* end, Item& item)
{
    int32_t i32 = 0;
    uint8_t u8 = 0;
    if (!popI32(ptr, end, i32))
    {
        return false;
    }
    item.itemId = i32;
    if (!popI32(ptr, end, i32))
    {
        return false;
    }
    item.enchantId = i32;
    if (!popI32(ptr, end, i32))
    {
        return false;
    }
    item.specialEnchantId = i32;
    if (!popI32(ptr, end, i32))
    {
        return false;
    }
    item.chargeCount = i32;
    if (!popU8(ptr, end, u8))
    {
        return false;
    }
    item.identified = (u8 != 0);
    if (!popU8(ptr, end, u8))
    {
        return false;
    }
    item.broken = (u8 != 0);
    if (!popU8(ptr, end, u8))
    {
        return false;
    }
    item.temporaryEnchant = (u8 != 0);
    return true;
}

std::vector<uint8_t> serializeInventoryState(const Inventory& inventory)
{
    std::vector<uint8_t> out;
    out.reserve(2048);
    appendU32(out, kInventoryMagic);
    appendU32(out, kInventoryVersion);

    const auto& inventories = inventory.inventories();
    for (const auto& charInv : inventories)
    {
        for (const auto& item : charInv.equipped)
        {
            appendInventoryItem(out, item);
        }
        for (const auto& item : charInv.backpack)
        {
            appendInventoryItem(out, item);
        }
    }
    return out;
}

bool deserializeInventoryState(const std::vector<uint8_t>& data, Inventory& inventory)
{
    if (data.empty())
    {
        return true;
    }

    const uint8_t* ptr = data.data();
    const uint8_t* end = data.data() + data.size();
    uint32_t magic = 0;
    uint32_t version = 0;
    if (!popU32(ptr, end, magic) || !popU32(ptr, end, version))
    {
        return false;
    }
    if (magic != kInventoryMagic || version != kInventoryVersion)
    {
        return false;
    }

    std::array<CharacterInventory, 4> restored = {};
    for (auto& charInv : restored)
    {
        for (auto& item : charInv.equipped)
        {
            if (!popInventoryItem(ptr, end, item))
            {
                return false;
            }
        }
        for (auto& item : charInv.backpack)
        {
            if (!popInventoryItem(ptr, end, item))
            {
                return false;
            }
        }
    }

    inventory.setInventories(std::move(restored));
    return true;
}
} // namespace

SaveGame::SaveGame(util::ILogger& logger) : logger_(logger) {}

std::string SaveGame::slotPath(int slotIndex) const
{
    return (std::filesystem::path(saveDir_) / std::format("save{:03}.mm7", slotIndex)).string();
}

std::string SaveGame::legacySlotPath(int slotIndex) const
{
    return (std::filesystem::path(saveDir_) / ("save_" + std::to_string(slotIndex) + ".rhsav"))
        .string();
}

std::string SaveGame::autosavePath() const
{
    return (std::filesystem::path(saveDir_) / "autosave.mm7").string();
}

std::string SaveGame::resolveExistingSlotPath(int slotIndex) const
{
    const std::string mm7Path = slotPath(slotIndex);
    if (std::filesystem::exists(mm7Path))
    {
        return mm7Path;
    }

    const std::string rhsavPath = legacySlotPath(slotIndex);
    if (std::filesystem::exists(rhsavPath))
    {
        return rhsavPath;
    }

    return {};
}

bool SaveGame::save(const GameWorld& world, int slotIndex,
                    const std::vector<uint8_t>* eventRuntimeState, const Inventory* inventory)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots)
    {
        logger_.error("Invalid save slot: " + std::to_string(slotIndex));
        return false;
    }

    // Ensure save directory exists
    std::filesystem::create_directories(saveDir_);

    std::vector<uint8_t> data;
    if (!serializeWorld(world, data))
    {
        logger_.error("Failed to serialize game state");
        return false;
    }

    SaveHeader saveHeader{};
    if (data.size() < sizeof(saveHeader))
    {
        logger_.error("Serialized save payload is too small");
        return false;
    }
    std::memcpy(&saveHeader, data.data(), sizeof(saveHeader));

    const SaveSlotHeaderBin slotHeader = makeHeaderBin(saveHeader);
    std::vector<uint8_t> headerBin(sizeof(slotHeader));
    std::memcpy(headerBin.data(), &slotHeader, sizeof(slotHeader));

    std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
    files.push_back({"header.bin", std::move(headerBin)});
    files.push_back({"image.pcx", makePlaceholderScreenshotPcx()});

    std::vector<uint8_t> partyPayload;
    if (data.size() > sizeof(SaveHeader))
    {
        partyPayload.assign(data.begin() + sizeof(SaveHeader), data.end());
    }
    files.push_back({"party.bin", std::move(partyPayload)});
    files.push_back({"runeharbor.bin", std::move(data)});
    const uint32_t version = SaveHeader::kVersion;
    std::vector<uint8_t> versionBlob(sizeof(version), 0);
    std::memcpy(versionBlob.data(), &version, sizeof(version));
    files.push_back({"runeharbor.ver", std::move(versionBlob)});
    files.push_back({"clock.bin", makeClockBin(saveHeader.gameTime)});
    if (eventRuntimeState && !eventRuntimeState->empty())
    {
        files.push_back({"eventrt.bin", *eventRuntimeState});
    }
    if (inventory)
    {
        files.push_back({"inventory.bin", serializeInventoryState(*inventory)});
    }
    files.push_back({"timer.bin", makeTimerBin(eventRuntimeState)});
    files.push_back({"npcdata.bin", std::vector<uint8_t>(38076, 0)});
    files.push_back({"npcgroup.bin", std::vector<uint8_t>(102, 0)});

    std::vector<uint8_t> archiveData = buildSaveArchive(files);
    if (archiveData.empty())
    {
        logger_.error("Failed to build save archive payload");
        return false;
    }

    std::string path = slotPath(slotIndex);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        logger_.error("Failed to open save file: " + path);
        return false;
    }

    file.write(reinterpret_cast<const char*>(archiveData.data()),
               static_cast<std::streamsize>(archiveData.size()));
    if (!file)
    {
        logger_.error("Failed to write save file: " + path);
        return false;
    }

    const std::string autosave = autosavePath();
    std::error_code ec;
    std::filesystem::copy_file(path, autosave, std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec)
    {
        logger_.warning("Failed to update autosave: " + autosave + " (" + ec.message() + ")");
    }

    logger_.info("Saved game to slot " + std::to_string(slotIndex) + " (" +
                 std::to_string(archiveData.size()) + " bytes)");
    return true;
}

bool SaveGame::load(GameWorld& world, int slotIndex, std::vector<uint8_t>* eventRuntimeState,
                    Inventory* inventory)
{
    if (eventRuntimeState)
    {
        eventRuntimeState->clear();
    }

    if (slotIndex < 0 || slotIndex >= kMaxSlots)
    {
        logger_.error("Invalid save slot: " + std::to_string(slotIndex));
        return false;
    }

    std::string path = resolveExistingSlotPath(slotIndex);
    if (path.empty())
    {
        logger_.error("Save file not found for slot: " + std::to_string(slotIndex));
        return false;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        logger_.error("Save file not found: " + path);
        return false;
    }

    auto size = file.tellg();
    if (size <= 0)
    {
        logger_.error("Save file is empty or unreadable: " + path);
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file)
    {
        logger_.error("Failed to read save file: " + path);
        return false;
    }

    bool loaded = false;
    if (hasRawSaveMagic(data))
    {
        loaded = deserializeWorld(world, data);
    }
    else if (hasLodMagic(data))
    {
        formats::GameLODArchive archive(logger_);
        if (!archive.open(path))
        {
            logger_.error("Failed to open save archive: " + path);
            return false;
        }

        auto rhBlob = archive.extractFile("runeharbor.bin");
        if (rhBlob.has_value() && hasRawSaveMagic(*rhBlob))
        {
            loaded = deserializeWorld(world, *rhBlob);
            if (inventory)
            {
                auto inventoryBlob = archive.extractFile("inventory.bin");
                if (inventoryBlob.has_value() && !inventoryBlob->empty() &&
                    !deserializeInventoryState(*inventoryBlob, *inventory))
                {
                    logger_.warning("Failed to deserialize inventory payload from save archive");
                }
            }
        }
        else
        {
            auto reconstructed = buildRawSaveFromArchive(archive);
            if (reconstructed.has_value() && hasRawSaveMagic(*reconstructed))
            {
                loaded = deserializeWorld(world, *reconstructed);
            }
            else
            {
                loaded = loadArchiveFallbackState(logger_, path, world);
            }
        }

        if (loaded && eventRuntimeState)
        {
            extractEventRuntimeFromArchive(archive, *eventRuntimeState);
        }
    }

    if (!loaded)
    {
        logger_.error("Failed to deserialize save file: " + path);
        return false;
    }

    logger_.info("Loaded game from slot " + std::to_string(slotIndex));
    return true;
}

bool SaveGame::loadAutosave(GameWorld& world, std::vector<uint8_t>* eventRuntimeState,
                            Inventory* inventory)
{
    if (eventRuntimeState)
    {
        eventRuntimeState->clear();
    }

    const std::string path = autosavePath();
    if (!std::filesystem::exists(path))
    {
        logger_.error("Autosave file not found: " + path);
        return false;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        logger_.error("Failed to open autosave file: " + path);
        return false;
    }

    const auto size = file.tellg();
    if (size <= 0)
    {
        logger_.error("Autosave file is empty or unreadable: " + path);
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file)
    {
        logger_.error("Failed to read autosave file: " + path);
        return false;
    }

    bool loaded = false;
    if (hasRawSaveMagic(data))
    {
        loaded = deserializeWorld(world, data);
    }
    else if (hasLodMagic(data))
    {
        formats::GameLODArchive archive(logger_);
        if (!archive.open(path))
        {
            logger_.error("Failed to open autosave archive: " + path);
            return false;
        }

        auto rhBlob = archive.extractFile("runeharbor.bin");
        if (rhBlob.has_value() && hasRawSaveMagic(*rhBlob))
        {
            loaded = deserializeWorld(world, *rhBlob);
            if (inventory)
            {
                auto inventoryBlob = archive.extractFile("inventory.bin");
                if (inventoryBlob.has_value() && !inventoryBlob->empty() &&
                    !deserializeInventoryState(*inventoryBlob, *inventory))
                {
                    logger_.warning("Failed to deserialize inventory payload from autosave");
                }
            }
        }
        else
        {
            auto reconstructed = buildRawSaveFromArchive(archive);
            if (reconstructed.has_value() && hasRawSaveMagic(*reconstructed))
            {
                loaded = deserializeWorld(world, *reconstructed);
            }
            else
            {
                loaded = loadArchiveFallbackState(logger_, path, world);
            }
        }

        if (loaded && eventRuntimeState)
        {
            extractEventRuntimeFromArchive(archive, *eventRuntimeState);
        }
    }

    if (!loaded)
    {
        logger_.error("Failed to deserialize autosave file: " + path);
        return false;
    }

    logger_.info("Loaded game from autosave");
    return true;
}

std::vector<SaveSlotInfo> SaveGame::listSlots() const
{
    std::filesystem::create_directories(saveDir_);

    std::vector<SaveSlotInfo> slots;
    for (int i = 0; i < kMaxSlots; i++)
    {
        SaveSlotInfo info;
        info.slotIndex = i;
        info.filePath = slotPath(i);
        const std::string resolvedPath = resolveExistingSlotPath(i);
        info.exists = !resolvedPath.empty();

        if (info.exists)
        {
            std::ifstream file(resolvedPath, std::ios::binary | std::ios::ate);
            if (!file)
            {
                info.exists = false;
            }
            else
            {
                const auto size = file.tellg();
                if (size <= 0)
                {
                    info.exists = false;
                    slots.push_back(info);
                    continue;
                }
                file.seekg(0, std::ios::beg);
                std::vector<uint8_t> raw(static_cast<size_t>(size));
                file.read(reinterpret_cast<char*>(raw.data()), size);
                if (!file)
                {
                    info.exists = false;
                }
                else if (hasRawSaveMagic(raw))
                {
                    if (raw.size() < sizeof(SaveHeader))
                    {
                        info.exists = false;
                    }
                    else
                    {
                        std::memcpy(&info.header, raw.data(), sizeof(SaveHeader));
                    }
                }
                else if (hasLodMagic(raw))
                {
                    auto archiveHeader = readArchiveSaveHeader(logger_, resolvedPath);
                    if (!archiveHeader.has_value())
                    {
                        info.exists = false;
                    }
                    else
                    {
                        info.header = *archiveHeader;
                    }
                }
                else
                {
                    info.exists = false;
                }
            }
        }
        slots.push_back(info);
    }
    return slots;
}

bool SaveGame::slotExists(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots)
        return false;
    return !resolveExistingSlotPath(slotIndex).empty();
}

std::optional<SaveHeader> SaveGame::autosaveHeader() const
{
    const std::string path = autosavePath();
    if (!std::filesystem::exists(path))
    {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return std::nullopt;
    }
    const auto size = file.tellg();
    if (size <= 0)
    {
        return std::nullopt;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> raw(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(raw.data()), size);
    if (!file)
    {
        return std::nullopt;
    }

    if (hasRawSaveMagic(raw))
    {
        if (raw.size() < sizeof(SaveHeader))
        {
            return std::nullopt;
        }
        SaveHeader header{};
        std::memcpy(&header, raw.data(), sizeof(SaveHeader));
        return header;
    }

    if (hasLodMagic(raw))
    {
        return readArchiveSaveHeader(logger_, path);
    }

    return std::nullopt;
}

bool SaveGame::autosaveExists() const
{
    return std::filesystem::exists(autosavePath());
}

bool SaveGame::deleteSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots)
        return false;
    bool removed = false;
    const std::string mm7Path = slotPath(slotIndex);
    const std::string rhsavPath = legacySlotPath(slotIndex);

    if (std::filesystem::exists(mm7Path))
    {
        std::filesystem::remove(mm7Path);
        removed = true;
    }

    if (std::filesystem::exists(rhsavPath))
    {
        std::filesystem::remove(rhsavPath);
        removed = true;
    }

    if (removed)
    {
        logger_.info("Deleted save slot " + std::to_string(slotIndex));
    }
    return removed;
}

// ── Serialization primitives ─────────────────────────────────────────────────

void SaveGame::writeU8(std::vector<uint8_t>& buf, uint8_t val)
{
    buf.push_back(val);
}

void SaveGame::writeU16(std::vector<uint8_t>& buf, uint16_t val)
{
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void SaveGame::writeU32(std::vector<uint8_t>& buf, uint32_t val)
{
    for (int i = 0; i < 4; i++)
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
}

void SaveGame::writeU64(std::vector<uint8_t>& buf, uint64_t val)
{
    for (int i = 0; i < 8; i++)
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
}

void SaveGame::writeI32(std::vector<uint8_t>& buf, int32_t val)
{
    writeU32(buf, static_cast<uint32_t>(val));
}

void SaveGame::writeFloat(std::vector<uint8_t>& buf, float val)
{
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    writeU32(buf, bits);
}

void SaveGame::writeString(std::vector<uint8_t>& buf, const std::string& str)
{
    writeU16(buf, static_cast<uint16_t>(str.size()));
    buf.insert(buf.end(), str.begin(), str.end());
}

bool SaveGame::readU8(const uint8_t*& ptr, const uint8_t* end, uint8_t& val)
{
    if (ptr + 1 > end)
        return false;
    val = *ptr++;
    return true;
}

bool SaveGame::readU16(const uint8_t*& ptr, const uint8_t* end, uint16_t& val)
{
    if (ptr + 2 > end)
        return false;
    val = static_cast<uint16_t>(ptr[0] | (ptr[1] << 8));
    ptr += 2;
    return true;
}

bool SaveGame::readU32(const uint8_t*& ptr, const uint8_t* end, uint32_t& val)
{
    if (ptr + 4 > end)
        return false;
    val = static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
          (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    return true;
}

bool SaveGame::readU64(const uint8_t*& ptr, const uint8_t* end, uint64_t& val)
{
    if (ptr + 8 > end)
        return false;
    val = 0;
    for (int i = 0; i < 8; i++)
        val |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    ptr += 8;
    return true;
}

bool SaveGame::readI32(const uint8_t*& ptr, const uint8_t* end, int32_t& val)
{
    uint32_t u;
    if (!readU32(ptr, end, u))
        return false;
    val = static_cast<int32_t>(u);
    return true;
}

bool SaveGame::readFloat(const uint8_t*& ptr, const uint8_t* end, float& val)
{
    uint32_t bits;
    if (!readU32(ptr, end, bits))
        return false;
    std::memcpy(&val, &bits, sizeof(val));
    return true;
}

bool SaveGame::readString(const uint8_t*& ptr, const uint8_t* end, std::string& str)
{
    uint16_t len;
    if (!readU16(ptr, end, len))
        return false;
    if (ptr + len > end)
        return false;
    str.assign(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return true;
}

// ── World serialization ──────────────────────────────────────────────────────

bool SaveGame::serializeWorld(const GameWorld& world, std::vector<uint8_t>& out)
{
    out.clear();
    out.reserve(4096);

    // Header
    SaveHeader header;
    header.magic = SaveHeader::kMagic;
    header.version = SaveHeader::kVersion;
    header.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    header.gameTime = static_cast<uint64_t>(world.calendar().totalTicks);

    const auto& party = world.party();
    const auto& mapName = world.currentMap();
    std::strncpy(header.mapName, mapName.c_str(), sizeof(header.mapName) - 1);
    std::strncpy(header.partyLeader, party.member(0).name.c_str(), sizeof(header.partyLeader) - 1);

    int totalLevel = 0;
    for (int i = 0; i < kPartySize; i++)
        totalLevel += party.member(i).level;
    header.partyLevel = totalLevel / kPartySize;

    // Write header as raw bytes
    const auto* headerBytes = reinterpret_cast<const uint8_t*>(&header);
    out.insert(out.end(), headerBytes, headerBytes + sizeof(header));

    // Calendar (ticks at 128/sec)
    writeU64(out, static_cast<uint64_t>(world.calendar().totalTicks));

    // Current map
    writeString(out, world.currentMap());

    // Party resources
    writeI32(out, party.gold());
    writeI32(out, party.food());
    writeU8(out, static_cast<uint8_t>(party.alignment()));
    writeI32(out, party.reputation());

    // Party position
    writeFloat(out, party.worldX());
    writeFloat(out, party.worldY());
    writeFloat(out, party.worldZ());
    writeFloat(out, party.yaw());
    writeFloat(out, party.pitch());

    // Game time
    writeU64(out, party.gameTime());

    // Characters
    for (int i = 0; i < kPartySize; i++)
    {
        const auto& ch = party.member(i);
        writeString(out, ch.name);
        writeI32(out, ch.faceId);
        writeU8(out, static_cast<uint8_t>(ch.charClass));
        writeU8(out, static_cast<uint8_t>(ch.gender));

        // Stats
        for (int s = 0; s < Stats::kCount; s++)
            writeI32(out, ch.stats.byIndex(s));
        for (int s = 0; s < Stats::kCount; s++)
            writeI32(out, ch.baseStats.byIndex(s));

        // Derived
        writeI32(out, ch.level);
        writeI32(out, ch.experience);
        writeI32(out, ch.hitPoints);
        writeI32(out, ch.maxHitPoints);
        writeI32(out, ch.spellPoints);
        writeI32(out, ch.maxSpellPoints);
        writeI32(out, ch.armorClass);
        writeI32(out, ch.age);

        // Resistances (6 base: Fire, Air, Water, Earth, Mind, Body)
        writeI32(out, ch.fireResistance);
        writeI32(out, ch.airResistance);
        writeI32(out, ch.waterResistance);
        writeI32(out, ch.earthResistance);
        writeI32(out, ch.mindResistance);
        writeI32(out, ch.bodyResistance);

        // Skills
        constexpr int skillCount = static_cast<int>(SkillId::Count);
        writeU16(out, static_cast<uint16_t>(skillCount));
        for (int s = 0; s < skillCount; s++)
        {
            writeU8(out, ch.skillLevels[static_cast<size_t>(s)].level);
            writeU8(out, static_cast<uint8_t>(ch.skillLevels[static_cast<size_t>(s)].mastery));
        }

        // Equipment
        constexpr int equipCount = static_cast<int>(EquipSlot::Count);
        writeU16(out, static_cast<uint16_t>(equipCount));
        for (int s = 0; s < equipCount; s++)
        {
            writeI32(out, ch.equipment[static_cast<size_t>(s)].itemId);
        }

        // Conditions (18 int64 timestamps)
        for (int c = 0; c < Character::kConditionCount; c++)
        {
            writeU64(out, static_cast<uint64_t>(ch.conditionTimestamps[static_cast<size_t>(c)]));
        }
    }

    // Game variables
    std::vector<std::pair<GameVarId, int>> vars;
    vars.reserve(world.vars().size());
    for (const auto& [id, value] : world.vars())
    {
        vars.emplace_back(id, value);
    }
    std::sort(vars.begin(), vars.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    writeU32(out, static_cast<uint32_t>(vars.size()));
    for (const auto& [id, value] : vars)
    {
        writeU16(out, id);
        writeI32(out, value);
    }

    // Extended world state (version 2+)
    writeI32(out, party.activeMemberIndex());

    const auto runtimeConfig = world.runtimeConfig();
    writeU8(out, runtimeConfig.noMonsters ? 1 : 0);
    writeU8(out, runtimeConfig.noDamage ? 1 : 0);
    writeU8(out, runtimeConfig.noDecorations ? 1 : 0);
    writeU8(out, runtimeConfig.noSky ? 1 : 0);
    writeU8(out, runtimeConfig.noWavyWater ? 1 : 0);
    writeU8(out, runtimeConfig.noMist ? 1 : 0);
    writeI32(out, runtimeConfig.walkSpeed);
    writeI32(out, runtimeConfig.partyHeight);
    writeI32(out, runtimeConfig.partyEyeLevel);
    writeI32(out, runtimeConfig.gridBand1);
    writeI32(out, runtimeConfig.gridBand2);
    writeI32(out, runtimeConfig.gridBand3);
    writeI32(out, runtimeConfig.terrainGamma);
    writeI32(out, runtimeConfig.buildingGamma);
    writeI32(out, runtimeConfig.distShade);
    writeI32(out, runtimeConfig.distShadeMist);
    writeI32(out, runtimeConfig.distMist);
    for (uint8_t c : runtimeConfig.skyDayTop)
        writeU8(out, c);
    for (uint8_t c : runtimeConfig.skyDayBottom)
        writeU8(out, c);
    for (uint8_t c : runtimeConfig.skyNightTop)
        writeU8(out, c);
    for (uint8_t c : runtimeConfig.skyNightBottom)
        writeU8(out, c);

    std::vector<std::string> visited(world.visitedMaps().begin(), world.visitedMaps().end());
    std::sort(visited.begin(), visited.end());
    writeU32(out, static_cast<uint32_t>(visited.size()));
    for (const auto& visitedMap : visited)
    {
        writeString(out, visitedMap);
    }

    std::vector<std::string> generatedMaps;
    generatedMaps.reserve(world.generatedContentEntries().size());
    for (const auto& [map, _] : world.generatedContentEntries())
    {
        generatedMaps.push_back(map);
    }
    std::sort(generatedMaps.begin(), generatedMaps.end());

    writeU32(out, static_cast<uint32_t>(generatedMaps.size()));
    for (const auto& generatedMap : generatedMaps)
    {
        auto it = world.generatedContentEntries().find(generatedMap);
        if (it == world.generatedContentEntries().end())
        {
            continue;
        }
        const auto& content = it->second;
        writeString(out, generatedMap);
        writeU64(out, content.seed);
        writeU64(out, static_cast<uint64_t>(std::max<int64_t>(0, content.generatedAtTicks)));
        writeI32(out, std::max(0, content.respawnDays));

        writeU32(out, static_cast<uint32_t>(content.monsters.size()));
        for (const auto& monster : content.monsters)
        {
            writeI32(out, monster.monsterId);
            writeFloat(out, monster.x);
            writeFloat(out, monster.y);
            writeFloat(out, monster.z);
            writeI32(out, monster.group);
        }

        writeU32(out, static_cast<uint32_t>(content.chests.size()));
        for (const auto& chest : content.chests)
        {
            writeI32(out, chest.chestId);
            writeU32(out, static_cast<uint32_t>(chest.itemIds.size()));
            for (int itemId : chest.itemIds)
            {
                writeI32(out, itemId);
            }
        }
    }

    std::vector<std::string> savedStateMaps;
    savedStateMaps.reserve(world.savedMapStates().size());
    for (const auto& [map, _] : world.savedMapStates())
    {
        savedStateMaps.push_back(map);
    }
    std::sort(savedStateMaps.begin(), savedStateMaps.end());

    writeU32(out, static_cast<uint32_t>(savedStateMaps.size()));
    for (const auto& stateMap : savedStateMaps)
    {
        auto it = world.savedMapStates().find(stateMap);
        if (it == world.savedMapStates().end())
        {
            continue;
        }

        writeString(out, stateMap);
        const auto& state = it->second;

        writeU32(out, static_cast<uint32_t>(state.indoorFaceAttributes.size()));
        for (uint32_t attr : state.indoorFaceAttributes)
        {
            writeU32(out, attr);
        }

        writeU32(out, static_cast<uint32_t>(state.outdoorBuildingFaceAttributes.size()));
        for (const auto& buildingAttrs : state.outdoorBuildingFaceAttributes)
        {
            writeU32(out, static_cast<uint32_t>(buildingAttrs.size()));
            for (uint32_t attr : buildingAttrs)
            {
                writeU32(out, attr);
            }
        }

        writeU32(out, static_cast<uint32_t>(state.indoorDecorationHidden.size()));
        for (uint8_t hidden : state.indoorDecorationHidden)
        {
            writeU8(out, hidden != 0 ? 1u : 0u);
        }
    }

    std::vector<std::string> spawnedItemMaps;
    spawnedItemMaps.reserve(world.spawnedMapItems().size());
    for (const auto& [map, _] : world.spawnedMapItems())
    {
        spawnedItemMaps.push_back(map);
    }
    std::sort(spawnedItemMaps.begin(), spawnedItemMaps.end());

    writeU32(out, static_cast<uint32_t>(spawnedItemMaps.size()));
    for (const auto& mapName : spawnedItemMaps)
    {
        auto it = world.spawnedMapItems().find(mapName);
        if (it == world.spawnedMapItems().end())
        {
            continue;
        }

        writeString(out, mapName);
        const auto& items = it->second;
        writeU32(out, static_cast<uint32_t>(items.size()));
        for (const auto& item : items)
        {
            writeI32(out, item.itemType);
            writeI32(out, std::max(1, item.count));
            writeFloat(out, item.x);
            writeFloat(out, item.y);
            writeFloat(out, item.z);
            writeU64(out, static_cast<uint64_t>(std::max<int64_t>(0, item.createdAtTicks)));
        }
    }

    std::vector<std::pair<int, MapTransition>> transitions;
    transitions.reserve(world.transitions().size());
    for (const auto& [triggerId, transition] : world.transitions())
    {
        transitions.emplace_back(triggerId, transition);
    }
    std::sort(transitions.begin(), transitions.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    writeU32(out, static_cast<uint32_t>(transitions.size()));
    for (const auto& [triggerId, transition] : transitions)
    {
        writeI32(out, triggerId);
        writeString(out, transition.targetMap);
        writeString(out, transition.targetDisplayName);
        writeFloat(out, transition.targetX);
        writeFloat(out, transition.targetY);
        writeFloat(out, transition.targetZ);
        writeFloat(out, transition.targetYaw);
        writeU8(out, transition.hasArrivalOverride ? 1u : 0u);
    }

    return true;
}

bool SaveGame::deserializeWorld(GameWorld& world, const std::vector<uint8_t>& data)
{
    if (data.size() < sizeof(SaveHeader))
    {
        logger_.error("Save file too small");
        return false;
    }

    const uint8_t* ptr = data.data();
    const uint8_t* end = data.data() + data.size();

    // Read and validate header
    SaveHeader header;
    std::memcpy(&header, ptr, sizeof(header));
    ptr += sizeof(header);

    if (header.magic != SaveHeader::kMagic)
    {
        logger_.error("Invalid save file magic");
        return false;
    }
    if (header.version < 1 || header.version > SaveHeader::kVersion)
    {
        logger_.error("Unsupported save file version: " + std::to_string(header.version));
        return false;
    }

    world.reset();

    // Calendar (ticks at 128/sec)
    uint64_t calTicks;
    if (!readU64(ptr, end, calTicks))
        return false;
    world.calendar().totalTicks = static_cast<int64_t>(calTicks);

    // Current map
    std::string mapName;
    if (!readString(ptr, end, mapName))
        return false;
    world.setCurrentMap(mapName);

    // Party resources
    auto& party = world.party();
    int32_t gold, food, reputation;
    uint8_t alignment;
    if (!readI32(ptr, end, gold))
        return false;
    if (!readI32(ptr, end, food))
        return false;
    if (!readU8(ptr, end, alignment))
        return false;
    if (!readI32(ptr, end, reputation))
        return false;

    party.setGold(gold);
    party.setFood(food);
    party.setAlignment(static_cast<Alignment>(alignment));
    party.adjustReputation(reputation - party.reputation());

    // Party position
    float px, py, pz, yaw, pitch;
    if (!readFloat(ptr, end, px) || !readFloat(ptr, end, py) || !readFloat(ptr, end, pz))
        return false;
    if (!readFloat(ptr, end, yaw) || !readFloat(ptr, end, pitch))
        return false;
    party.setWorldPosition(px, py, pz);
    party.setOrientation(yaw, pitch);
    fprintf(stderr, "[SAVE-LOAD] map='%s' pos=(%.1f,%.1f,%.1f) yaw=%.1f pitch=%.1f\n",
            mapName.c_str(), px, py, pz, yaw, pitch);

    // Game time
    uint64_t gameTime;
    if (!readU64(ptr, end, gameTime))
        return false;
    party.setGameTime(gameTime);

    // Characters
    for (int i = 0; i < kPartySize; i++)
    {
        auto& ch = party.member(i);

        if (!readString(ptr, end, ch.name))
            return false;
        int32_t faceId;
        if (!readI32(ptr, end, faceId))
            return false;
        ch.faceId = faceId;

        uint8_t cls, gender;
        if (!readU8(ptr, end, cls) || !readU8(ptr, end, gender))
            return false;
        ch.charClass = static_cast<CharacterClass>(cls);
        ch.gender = static_cast<Gender>(gender);

        // Stats
        for (int s = 0; s < Stats::kCount; s++)
        {
            int32_t v;
            if (!readI32(ptr, end, v))
                return false;
            ch.stats.byIndex(s) = v;
        }
        for (int s = 0; s < Stats::kCount; s++)
        {
            int32_t v;
            if (!readI32(ptr, end, v))
                return false;
            ch.baseStats.byIndex(s) = v;
        }

        // Derived
        int32_t level, exp, hp, maxHp, sp, maxSp, ac, age;
        if (!readI32(ptr, end, level) || !readI32(ptr, end, exp))
            return false;
        if (!readI32(ptr, end, hp) || !readI32(ptr, end, maxHp))
            return false;
        if (!readI32(ptr, end, sp) || !readI32(ptr, end, maxSp))
            return false;
        if (!readI32(ptr, end, ac) || !readI32(ptr, end, age))
            return false;

        ch.level = level;
        ch.experience = exp;
        ch.hitPoints = hp;
        ch.maxHitPoints = maxHp;
        ch.spellPoints = sp;
        ch.maxSpellPoints = maxSp;
        ch.armorClass = ac;
        ch.age = age;

        // Resistances (6 base: Fire, Air, Water, Earth, Mind, Body)
        int32_t res;
        if (!readI32(ptr, end, res))
            return false;
        ch.fireResistance = res;
        if (!readI32(ptr, end, res))
            return false;
        ch.airResistance = res;
        if (!readI32(ptr, end, res))
            return false;
        ch.waterResistance = res;
        if (!readI32(ptr, end, res))
            return false;
        ch.earthResistance = res;
        if (!readI32(ptr, end, res))
            return false;
        ch.mindResistance = res;
        if (!readI32(ptr, end, res))
            return false;
        ch.bodyResistance = res;

        // Skills
        uint16_t skillCount;
        if (!readU16(ptr, end, skillCount))
            return false;
        int readSkills = std::min(static_cast<int>(skillCount), static_cast<int>(SkillId::Count));
        for (int s = 0; s < readSkills; s++)
        {
            uint8_t lvl, mastery;
            if (!readU8(ptr, end, lvl) || !readU8(ptr, end, mastery))
                return false;
            ch.skillLevels[static_cast<size_t>(s)].level = lvl;
            ch.skillLevels[static_cast<size_t>(s)].mastery = static_cast<SkillMastery>(mastery);
        }
        // Skip extra skills if file has more than we support
        for (int s = readSkills; s < static_cast<int>(skillCount); s++)
        {
            uint8_t dummy;
            if (!readU8(ptr, end, dummy) || !readU8(ptr, end, dummy))
                return false;
        }

        // Equipment
        uint16_t equipCount;
        if (!readU16(ptr, end, equipCount))
            return false;
        int readEquip = std::min(static_cast<int>(equipCount), static_cast<int>(EquipSlot::Count));
        for (int s = 0; s < readEquip; s++)
        {
            int32_t itemId;
            if (!readI32(ptr, end, itemId))
                return false;
            ch.equipment[static_cast<size_t>(s)].itemId = itemId;
        }
        for (int s = readEquip; s < static_cast<int>(equipCount); s++)
        {
            int32_t dummy;
            if (!readI32(ptr, end, dummy))
                return false;
        }

        // Conditions (18 int64 timestamps)
        for (int c = 0; c < Character::kConditionCount; c++)
        {
            uint64_t ts;
            if (!readU64(ptr, end, ts))
                return false;
            ch.conditionTimestamps[static_cast<size_t>(c)] = static_cast<int64_t>(ts);
        }
    }

    // Game variables
    uint32_t varCount;
    if (!readU32(ptr, end, varCount))
        return false;
    for (uint32_t i = 0; i < varCount; i++)
    {
        uint16_t varId;
        int32_t varVal;
        if (!readU16(ptr, end, varId) || !readI32(ptr, end, varVal))
            return false;
        world.setVar(varId, varVal);
    }

    if (header.version >= 2)
    {
        int32_t activeMember = 0;
        if (!readI32(ptr, end, activeMember))
            return false;
        party.setActiveMemberIndex(activeMember);

        RuntimeConfig runtimeConfig = world.runtimeConfig();
        uint8_t flag = 0;
        if (!readU8(ptr, end, flag))
            return false;
        runtimeConfig.noMonsters = flag != 0;
        if (!readU8(ptr, end, flag))
            return false;
        runtimeConfig.noDamage = flag != 0;
        if (!readU8(ptr, end, flag))
            return false;
        runtimeConfig.noDecorations = flag != 0;
        if (!readU8(ptr, end, flag))
            return false;
        runtimeConfig.noSky = flag != 0;
        if (!readU8(ptr, end, flag))
            return false;
        runtimeConfig.noWavyWater = flag != 0;
        if (!readU8(ptr, end, flag))
            return false;
        runtimeConfig.noMist = flag != 0;
        if (!readI32(ptr, end, runtimeConfig.walkSpeed))
            return false;
        if (header.version >= 11)
        {
            if (!readI32(ptr, end, runtimeConfig.partyHeight))
                return false;
            if (!readI32(ptr, end, runtimeConfig.partyEyeLevel))
                return false;
        }
        if (!readI32(ptr, end, runtimeConfig.gridBand1))
            return false;
        if (!readI32(ptr, end, runtimeConfig.gridBand2))
            return false;
        if (!readI32(ptr, end, runtimeConfig.gridBand3))
            return false;
        if (header.version >= 7)
        {
            if (!readI32(ptr, end, runtimeConfig.terrainGamma))
                return false;
            if (!readI32(ptr, end, runtimeConfig.buildingGamma))
                return false;
            if (!readI32(ptr, end, runtimeConfig.distShade))
                return false;
            if (!readI32(ptr, end, runtimeConfig.distShadeMist))
                return false;
            if (!readI32(ptr, end, runtimeConfig.distMist))
                return false;
        }
        if (header.version >= 4)
        {
            for (auto& c : runtimeConfig.skyDayTop)
            {
                if (!readU8(ptr, end, c))
                    return false;
            }
            for (auto& c : runtimeConfig.skyDayBottom)
            {
                if (!readU8(ptr, end, c))
                    return false;
            }
            for (auto& c : runtimeConfig.skyNightTop)
            {
                if (!readU8(ptr, end, c))
                    return false;
            }
            for (auto& c : runtimeConfig.skyNightBottom)
            {
                if (!readU8(ptr, end, c))
                    return false;
            }
        }
        runtimeConfig.walkSpeed = std::max(1, runtimeConfig.walkSpeed);
        runtimeConfig.partyHeight = std::max(1, runtimeConfig.partyHeight);
        runtimeConfig.partyEyeLevel = std::max(0, runtimeConfig.partyEyeLevel);
        runtimeConfig.gridBand1 = std::max(1, runtimeConfig.gridBand1);
        runtimeConfig.gridBand2 = std::max(runtimeConfig.gridBand1, runtimeConfig.gridBand2);
        runtimeConfig.gridBand3 = std::max(runtimeConfig.gridBand2, runtimeConfig.gridBand3);
        runtimeConfig.distShade = std::max(0, runtimeConfig.distShade);
        runtimeConfig.distShadeMist =
            std::max(runtimeConfig.distShade, runtimeConfig.distShadeMist);
        runtimeConfig.distMist = std::max(runtimeConfig.distShadeMist, runtimeConfig.distMist);
        world.setRuntimeConfig(runtimeConfig);

        uint32_t visitedCount = 0;
        if (!readU32(ptr, end, visitedCount))
            return false;
        for (uint32_t i = 0; i < visitedCount; i++)
        {
            std::string visitedMap;
            if (!readString(ptr, end, visitedMap))
                return false;
            if (!visitedMap.empty())
            {
                world.markVisited(visitedMap);
            }
        }

        uint32_t generatedMapCount = 0;
        if (!readU32(ptr, end, generatedMapCount))
            return false;
        for (uint32_t i = 0; i < generatedMapCount; i++)
        {
            std::string generatedMap;
            uint64_t seed = 0;
            if (!readString(ptr, end, generatedMap) || !readU64(ptr, end, seed))
                return false;

            GeneratedMapContent content;
            content.seed = seed;
            if (header.version >= 3)
            {
                uint64_t generatedAt = 0;
                int32_t respawnDays = 0;
                if (!readU64(ptr, end, generatedAt) || !readI32(ptr, end, respawnDays))
                    return false;
                content.generatedAtTicks = static_cast<int64_t>(generatedAt);
                content.respawnDays = std::max(0, respawnDays);
            }

            uint32_t monsterCount = 0;
            if (!readU32(ptr, end, monsterCount))
                return false;
            content.monsters.reserve(monsterCount);
            for (uint32_t m = 0; m < monsterCount; m++)
            {
                GeneratedMonster monster;
                if (!readI32(ptr, end, monster.monsterId))
                    return false;
                if (!readFloat(ptr, end, monster.x))
                    return false;
                if (!readFloat(ptr, end, monster.y))
                    return false;
                if (!readFloat(ptr, end, monster.z))
                    return false;
                if (!readI32(ptr, end, monster.group))
                    return false;
                content.monsters.push_back(monster);
            }

            uint32_t chestCount = 0;
            if (!readU32(ptr, end, chestCount))
                return false;
            content.chests.reserve(chestCount);
            for (uint32_t c = 0; c < chestCount; c++)
            {
                GeneratedChest chest;
                if (!readI32(ptr, end, chest.chestId))
                    return false;

                uint32_t itemCount = 0;
                if (!readU32(ptr, end, itemCount))
                    return false;
                chest.itemIds.reserve(itemCount);
                for (uint32_t item = 0; item < itemCount; item++)
                {
                    int32_t itemId = 0;
                    if (!readI32(ptr, end, itemId))
                        return false;
                    chest.itemIds.push_back(itemId);
                }
                content.chests.push_back(std::move(chest));
            }

            if (!generatedMap.empty())
            {
                world.setGeneratedContent(generatedMap, std::move(content));
            }
        }

        if (header.version >= 5)
        {
            uint32_t savedMapStateCount = 0;
            if (!readU32(ptr, end, savedMapStateCount))
                return false;

            for (uint32_t i = 0; i < savedMapStateCount; i++)
            {
                std::string stateMap;
                if (!readString(ptr, end, stateMap))
                    return false;

                SavedMapState state;

                uint32_t indoorCount = 0;
                if (!readU32(ptr, end, indoorCount))
                    return false;
                state.indoorFaceAttributes.reserve(indoorCount);
                for (uint32_t face = 0; face < indoorCount; face++)
                {
                    uint32_t attr = 0;
                    if (!readU32(ptr, end, attr))
                        return false;
                    state.indoorFaceAttributes.push_back(attr);
                }

                uint32_t buildingCount = 0;
                if (!readU32(ptr, end, buildingCount))
                    return false;
                state.outdoorBuildingFaceAttributes.resize(buildingCount);
                for (uint32_t bi = 0; bi < buildingCount; bi++)
                {
                    uint32_t faceCount = 0;
                    if (!readU32(ptr, end, faceCount))
                        return false;
                    auto& buildingAttrs = state.outdoorBuildingFaceAttributes[bi];
                    buildingAttrs.reserve(faceCount);
                    for (uint32_t fi = 0; fi < faceCount; fi++)
                    {
                        uint32_t attr = 0;
                        if (!readU32(ptr, end, attr))
                            return false;
                        buildingAttrs.push_back(attr);
                    }
                }

                if (header.version >= 10)
                {
                    uint32_t decorCount = 0;
                    if (!readU32(ptr, end, decorCount))
                        return false;
                    state.indoorDecorationHidden.resize(decorCount);
                    for (uint32_t di = 0; di < decorCount; di++)
                    {
                        uint8_t hidden = 0;
                        if (!readU8(ptr, end, hidden))
                            return false;
                        state.indoorDecorationHidden[di] = (hidden != 0) ? 1u : 0u;
                    }
                }

                if (!stateMap.empty())
                {
                    world.setSavedMapState(stateMap, std::move(state));
                }
            }
        }

        if (header.version >= 8)
        {
            uint32_t spawnedItemMapCount = 0;
            if (!readU32(ptr, end, spawnedItemMapCount))
                return false;

            for (uint32_t i = 0; i < spawnedItemMapCount; i++)
            {
                std::string mapName;
                if (!readString(ptr, end, mapName))
                    return false;

                uint32_t itemCount = 0;
                if (!readU32(ptr, end, itemCount))
                    return false;

                std::vector<SpawnedMapItem> items;
                items.reserve(itemCount);
                for (uint32_t itemIndex = 0; itemIndex < itemCount; itemIndex++)
                {
                    SpawnedMapItem item;
                    if (!readI32(ptr, end, item.itemType))
                        return false;
                    if (!readI32(ptr, end, item.count))
                        return false;
                    if (!readFloat(ptr, end, item.x))
                        return false;
                    if (!readFloat(ptr, end, item.y))
                        return false;
                    if (!readFloat(ptr, end, item.z))
                        return false;
                    uint64_t createdAtTicks = 0;
                    if (!readU64(ptr, end, createdAtTicks))
                        return false;
                    item.count = std::max(1, item.count);
                    item.createdAtTicks = static_cast<int64_t>(createdAtTicks);
                    items.push_back(item);
                }

                if (!mapName.empty())
                {
                    world.setSpawnedMapItems(mapName, std::move(items));
                }
            }
        }

        if (header.version >= 6)
        {
            uint32_t transitionCount = 0;
            if (!readU32(ptr, end, transitionCount))
                return false;

            for (uint32_t i = 0; i < transitionCount; i++)
            {
                int32_t triggerId = 0;
                if (!readI32(ptr, end, triggerId))
                    return false;

                MapTransition transition;
                if (!readString(ptr, end, transition.targetMap))
                    return false;
                if (!readString(ptr, end, transition.targetDisplayName))
                    return false;
                if (!readFloat(ptr, end, transition.targetX))
                    return false;
                if (!readFloat(ptr, end, transition.targetY))
                    return false;
                if (!readFloat(ptr, end, transition.targetZ))
                    return false;
                if (!readFloat(ptr, end, transition.targetYaw))
                    return false;
                if (header.version >= 9)
                {
                    uint8_t hasArrivalOverride = 0;
                    if (!readU8(ptr, end, hasArrivalOverride))
                        return false;
                    transition.hasArrivalOverride = hasArrivalOverride != 0;
                }
                else
                {
                    transition.hasArrivalOverride = false;
                }

                if (!transition.targetMap.empty())
                {
                    world.addTransition(triggerId, transition);
                }
            }
        }
    }

    return true;
}

} // namespace runeharbor::game
