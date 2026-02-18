// SPDX-License-Identifier: MIT
#include "save_game.hpp"

#include <chrono>
#include <filesystem>

#include <cstring>
#include <fstream>

#include "game_world.hpp"

namespace runeharbor::game
{

SaveGame::SaveGame(util::ILogger& logger) : logger_(logger) {}

std::string SaveGame::slotPath(int slotIndex) const
{
    return saveDir_ + "/save_" + std::to_string(slotIndex) + ".rhsav";
}

bool SaveGame::save(const GameWorld& world, int slotIndex)
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

    std::string path = slotPath(slotIndex);
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        logger_.error("Failed to open save file: " + path);
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    if (!file)
    {
        logger_.error("Failed to write save file: " + path);
        return false;
    }

    logger_.info("Saved game to slot " + std::to_string(slotIndex) + " (" +
                 std::to_string(data.size()) + " bytes)");
    return true;
}

bool SaveGame::load(GameWorld& world, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots)
    {
        logger_.error("Invalid save slot: " + std::to_string(slotIndex));
        return false;
    }

    std::string path = slotPath(slotIndex);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        logger_.error("Save file not found: " + path);
        return false;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (!deserializeWorld(world, data))
    {
        logger_.error("Failed to deserialize save file: " + path);
        return false;
    }

    logger_.info("Loaded game from slot " + std::to_string(slotIndex));
    return true;
}

std::vector<SaveSlotInfo> SaveGame::listSlots() const
{
    std::vector<SaveSlotInfo> slots;
    for (int i = 0; i < kMaxSlots; i++)
    {
        SaveSlotInfo info;
        info.slotIndex = i;
        info.filePath = slotPath(i);
        info.exists = std::filesystem::exists(info.filePath);

        if (info.exists)
        {
            std::ifstream file(info.filePath, std::ios::binary);
            if (file)
            {
                file.read(reinterpret_cast<char*>(&info.header), sizeof(SaveHeader));
                if (!file || info.header.magic != SaveHeader::kMagic)
                {
                    info.exists = false; // Corrupted
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
    return std::filesystem::exists(slotPath(slotIndex));
}

bool SaveGame::deleteSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kMaxSlots)
        return false;
    std::string path = slotPath(slotIndex);
    if (std::filesystem::exists(path))
    {
        std::filesystem::remove(path);
        logger_.info("Deleted save slot " + std::to_string(slotIndex));
        return true;
    }
    return false;
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
    header.gameTime = world.calendar().totalMinutes;

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

    // Calendar
    writeU64(out, world.calendar().totalMinutes);

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
        writeU8(out, static_cast<uint8_t>(ch.race));
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

        // Resistances
        writeI32(out, ch.fireResistance);
        writeI32(out, ch.airResistance);
        writeI32(out, ch.waterResistance);
        writeI32(out, ch.earthResistance);
        writeI32(out, ch.mindResistance);
        writeI32(out, ch.bodyResistance);
        writeI32(out, ch.spiritResistance);

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

        // Conditions
        writeU16(out, ch.conditions);
    }

    // Game variables
    // Write as key-value pairs; GameWorld stores them in unordered_map
    // We'll write a count + (id, value) pairs
    // Access game vars through a simple iteration approach
    // GameWorld doesn't expose iterating vars, so we write a sentinel count
    // and save known vars in a future version. For now, write 0 vars.
    writeU32(out, 0); // game var count (placeholder)

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
    if (header.version != SaveHeader::kVersion)
    {
        logger_.error("Unsupported save file version: " + std::to_string(header.version));
        return false;
    }

    world.reset();

    // Calendar
    uint64_t calMinutes;
    if (!readU64(ptr, end, calMinutes))
        return false;
    world.calendar().totalMinutes = calMinutes;

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

        uint8_t cls, race, gender;
        if (!readU8(ptr, end, cls) || !readU8(ptr, end, race) || !readU8(ptr, end, gender))
            return false;
        ch.charClass = static_cast<CharacterClass>(cls);
        ch.race = static_cast<Race>(race);
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

        // Resistances
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
        if (!readI32(ptr, end, res))
            return false;
        ch.spiritResistance = res;

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

        // Conditions
        uint16_t cond;
        if (!readU16(ptr, end, cond))
            return false;
        ch.conditions = cond;
    }

    // Game variables (placeholder)
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

    return true;
}

} // namespace runeharbor::game
