// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cctype>
#include <fstream>

#include "../formats/autonote_parser.hpp"
#include "../formats/awards_parser.hpp"
#include "../formats/blv_map.hpp"
#include "../formats/classes_parser.hpp"
#include "../formats/credits_parser.hpp"
#include "../formats/evt_script_parser.hpp"
#include "../formats/frame_tables.hpp"
#include "../formats/game_lod_archive.hpp"
#include "../formats/global_text_parser.hpp"
#include "../formats/hostile_parser.hpp"
#include "../formats/image_lod_archive.hpp"
#include "../formats/items_parser.hpp"
#include "../formats/mapstats_parser.hpp"
#include "../formats/monsters_parser.hpp"
#include "../formats/npcdata_parser.hpp"
#include "../formats/npcgreet_parser.hpp"
#include "../formats/npcnames_parser.hpp"
#include "../formats/npcprof_parser.hpp"
#include "../formats/npctext_parser.hpp"
#include "../formats/npctopic_parser.hpp"
#include "../formats/odm_map.hpp"
#include "../formats/placemon_parser.hpp"
#include "../formats/quests_parser.hpp"
#include "../formats/rnditems_parser.hpp"
#include "../formats/snd_archive.hpp"
#include "../formats/sound_list.hpp"
#include "../formats/spells_parser.hpp"
#include "../formats/sprite_lod_archive.hpp"
#include "../formats/two_d_events_parser.hpp"
#include "../media/vid_archive.hpp"
#include "../util/ilogger.hpp"

namespace fs = std::filesystem;

namespace
{

class QuietLogger final : public runeharbor::util::ILogger
{
  public:
    void log(runeharbor::util::LogLevel level, std::string_view message) override
    {
        if (level == runeharbor::util::LogLevel::Error ||
            level == runeharbor::util::LogLevel::Critical)
        {
            errors.push_back(std::string(message));
        }
        else if (level == runeharbor::util::LogLevel::Warning)
        {
            warnings.push_back(std::string(message));
        }
    }

    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

struct CheckResult
{
    std::string name;
    bool ok = true;
    std::map<std::string, uint64_t> metrics;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

struct Report
{
    fs::path installRoot;
    fs::path dataRoot;
    std::vector<CheckResult> checks;

    bool ok(bool strict) const
    {
        return std::ranges::all_of(checks, [strict](const CheckResult& check)
                                   { return check.ok && (!strict || check.warnings.empty()); });
    }

    uint64_t errorCount() const
    {
        uint64_t total = 0;
        for (const auto& check : checks)
        {
            total += check.errors.size();
        }
        return total;
    }

    uint64_t warningCount() const
    {
        uint64_t total = 0;
        for (const auto& check : checks)
        {
            total += check.warnings.size();
        }
        return total;
    }
};

std::string lowerCopy(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool endsWithIgnoreCase(std::string_view text, std::string_view suffix)
{
    if (text.size() < suffix.size())
    {
        return false;
    }
    return lowerCopy(text.substr(text.size() - suffix.size())) == lowerCopy(suffix);
}

bool startsWithIgnoreCase(std::string_view text, std::string_view prefix)
{
    if (text.size() < prefix.size())
    {
        return false;
    }
    return lowerCopy(text.substr(0, prefix.size())) == lowerCopy(prefix);
}

std::string stemLower(std::string_view filename)
{
    const std::string name(filename);
    const auto dot = name.find_last_of('.');
    return lowerCopy(dot == std::string::npos ? name : name.substr(0, dot));
}

std::string jsonEscape(std::string_view text)
{
    std::ostringstream out;
    for (unsigned char c : text)
    {
        switch (c)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 0x20 || c >= 0x7F)
            {
                out << "\\u";
                constexpr char kHex[] = "0123456789abcdef";
                out << '0' << '0' << kHex[(c >> 4) & 0xF] << kHex[c & 0xF];
            }
            else
            {
                out << static_cast<char>(c);
            }
            break;
        }
    }
    return out.str();
}

void addError(CheckResult& result, std::string message)
{
    result.ok = false;
    result.errors.push_back(std::move(message));
}

void addWarning(CheckResult& result, std::string message)
{
    result.warnings.push_back(std::move(message));
}

std::optional<fs::path> findCaseInsensitive(const fs::path& dir, std::string_view filename)
{
    const fs::path exact = dir / std::string(filename);
    if (fs::exists(exact))
    {
        return exact;
    }

    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
        return std::nullopt;
    }

    const std::string target = lowerCopy(filename);
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (lowerCopy(entry.path().filename().string()) == target)
        {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::unordered_map<std::string, std::string> indexByLower(const std::vector<std::string>& names)
{
    std::unordered_map<std::string, std::string> result;
    result.reserve(names.size());
    for (const auto& name : names)
    {
        result.emplace(lowerCopy(name), name);
    }
    return result;
}

void absorbLoggerMessages(CheckResult& result, QuietLogger& logger)
{
    result.metrics["loggerWarnings"] += logger.warnings.size();
    result.metrics["loggerErrors"] += logger.errors.size();

    constexpr size_t kMessageLimit = 20;
    for (const auto& error : logger.errors)
    {
        if (result.errors.size() < kMessageLimit)
        {
            addError(result, error);
        }
    }
    for (const auto& warning : logger.warnings)
    {
        if (result.warnings.size() < kMessageLimit)
        {
            addWarning(result, warning);
        }
    }
    logger.warnings.clear();
    logger.errors.clear();
}

bool parseKnownTxt(std::string_view lowerName, const std::vector<uint8_t>& data,
                   QuietLogger& logger)
{
    using namespace runeharbor::formats;

    if (lowerName == "2devents.txt")
    {
        TwoDEventsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "autonote.txt")
    {
        AutonoteParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "awards.txt")
    {
        AwardsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "class.txt")
    {
        ClassesParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "credits.txt")
    {
        CreditsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "global.txt")
    {
        GlobalTextParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "hostile.txt")
    {
        HostileParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "items.txt")
    {
        ItemsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "mapstats.txt")
    {
        MapStatsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "monsters.txt")
    {
        MonstersParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "npcdata.txt")
    {
        NPCDataParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "npcgreet.txt")
    {
        NPCGreetingParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "npcnames.txt")
    {
        NPCNamesParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "npcprof.txt")
    {
        NPCProfessionParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "npctext.txt")
    {
        NPCTextParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "npctopic.txt")
    {
        NPCTopicParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "placemon.txt")
    {
        PlacemonParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "quests.txt")
    {
        QuestsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "rnditems.txt")
    {
        RndItemsParser parser(logger);
        return parser.parse(data);
    }
    if (lowerName == "spells.txt")
    {
        SpellsParser parser(logger);
        return parser.parse(data);
    }

    return true;
}

CheckResult checkImageArchive(const fs::path& dataRoot, std::string_view archiveName,
                              QuietLogger& logger)
{
    using namespace runeharbor::formats;

    CheckResult result;
    result.name = std::string(archiveName);

    const auto path = findCaseInsensitive(dataRoot, archiveName);
    if (!path)
    {
        addError(result, "archive not found");
        return result;
    }

    ImageLODArchive archive(logger);
    if (!archive.open(*path))
    {
        addError(result, "failed to open archive");
        absorbLoggerMessages(result, logger);
        return result;
    }

    const auto files = archive.listFiles();
    result.metrics["entries"] = files.size();

    for (const auto& filename : files)
    {
        if (filename.empty())
        {
            continue;
        }

        auto data = archive.extractFile(filename);
        if (!data)
        {
            result.metrics["extractFailures"]++;
            if (result.errors.size() < 25)
            {
                addError(result, "failed to extract " + filename);
            }
            else
            {
                result.ok = false;
            }
            continue;
        }

        const std::string lower = lowerCopy(filename);
        if (startsWithIgnoreCase(filename, "pal"))
        {
            result.metrics["palettes"]++;
            if (data->size() != 768)
            {
                addError(result, "palette " + filename + " extracted to " +
                                     std::to_string(data->size()) + " bytes");
            }
        }
        else if (endsWithIgnoreCase(lower, ".txt"))
        {
            result.metrics["txt"]++;
        }
        else if (endsWithIgnoreCase(lower, ".evt"))
        {
            result.metrics["evt"]++;
        }
        else if (endsWithIgnoreCase(lower, ".str"))
        {
            result.metrics["str"]++;
        }
        else if (endsWithIgnoreCase(lower, ".bin"))
        {
            result.metrics["bin"]++;
        }
        else
        {
            result.metrics["images"]++;
        }
        result.metrics["decodedBytes"] += data->size();
    }

    absorbLoggerMessages(result, logger);
    return result;
}

CheckResult checkSprites(const fs::path& dataRoot, QuietLogger& logger)
{
    using namespace runeharbor::formats;

    CheckResult result;
    result.name = "SPRITES.LOD";

    const auto path = findCaseInsensitive(dataRoot, "SPRITES.LOD");
    if (!path)
    {
        addError(result, "archive not found");
        return result;
    }

    SpriteLODArchive archive(logger);
    if (!archive.open(*path))
    {
        addError(result, "failed to open archive");
        absorbLoggerMessages(result, logger);
        return result;
    }

    const auto files = archive.listFiles();
    result.metrics["entries"] = files.size();
    for (const auto& filename : files)
    {
        if (filename.empty() || lowerCopy(filename) == "sprites08")
        {
            result.metrics["metadataEntries"]++;
            continue;
        }

        auto info = archive.getFileInfo(filename);
        if (!info)
        {
            result.metrics["headerFailures"]++;
            if (result.errors.size() < 25)
            {
                addError(result, "failed to read sprite header " + filename);
            }
            else
            {
                result.ok = false;
            }
            continue;
        }

        result.metrics["spriteHeaders"]++;
        if (info->width == 0 || info->height == 0 || info->width > 1024 || info->height > 1024)
        {
            addError(result, "invalid sprite dimensions for " + filename);
        }
    }

    absorbLoggerMessages(result, logger);
    return result;
}

CheckResult checkGameMaps(const fs::path& dataRoot, QuietLogger& logger)
{
    using namespace runeharbor::formats;

    CheckResult result;
    result.name = "GAMES.LOD";

    const auto path = findCaseInsensitive(dataRoot, "GAMES.LOD");
    if (!path)
    {
        addError(result, "archive not found");
        return result;
    }

    GameLODArchive archive(logger);
    if (!archive.open(*path))
    {
        addError(result, "failed to open archive");
        absorbLoggerMessages(result, logger);
        return result;
    }

    const auto files = archive.listFiles();
    result.metrics["entries"] = files.size();
    for (const auto& filename : files)
    {
        const std::string lower = lowerCopy(filename);
        auto data = archive.extractFile(filename);
        if (!data)
        {
            result.metrics["extractFailures"]++;
            if (result.errors.size() < 25)
            {
                addError(result, "failed to extract " + filename);
            }
            else
            {
                result.ok = false;
            }
            continue;
        }

        if (endsWithIgnoreCase(lower, ".blv"))
        {
            result.metrics["blv"]++;
            BLVMap map(logger);
            if (!map.parse(*data))
            {
                result.metrics["blvFailures"]++;
                addError(result, "failed to parse BLV " + filename);
            }
            else
            {
                const auto& parsed = map.getData();
                result.metrics["blvSpawns"] += parsed.spawns.size();
                result.metrics["blvOutlines"] += parsed.outlines.size();
            }
        }
        else if (endsWithIgnoreCase(lower, ".odm"))
        {
            result.metrics["odm"]++;
            runeharbor::formats::ODMMap map(logger);
            if (!map.parse(*data))
            {
                result.metrics["odmFailures"]++;
                addError(result, "failed to parse ODM " + filename);
            }
        }
        else if (endsWithIgnoreCase(lower, ".dlv"))
        {
            result.metrics["dlv"]++;
        }
        else if (endsWithIgnoreCase(lower, ".ddm"))
        {
            result.metrics["ddm"]++;
        }
        result.metrics["decodedBytes"] += data->size();
    }

    absorbLoggerMessages(result, logger);
    return result;
}

CheckResult checkEvents(const fs::path& dataRoot, QuietLogger& logger)
{
    using namespace runeharbor::formats;

    CheckResult result;
    result.name = "Events.lod scripts and tables";

    const auto path = findCaseInsensitive(dataRoot, "Events.lod");
    if (!path)
    {
        addError(result, "archive not found");
        return result;
    }

    ImageLODArchive archive(logger);
    if (!archive.open(*path))
    {
        addError(result, "failed to open archive");
        absorbLoggerMessages(result, logger);
        return result;
    }

    const auto files = archive.listFiles();
    const auto fileIndex = indexByLower(files);
    EvtScriptParser evtParser(logger);

    for (const auto& filename : files)
    {
        const std::string lower = lowerCopy(filename);
        auto data = archive.extractFile(filename);
        if (!data)
        {
            result.metrics["extractFailures"]++;
            addError(result, "failed to extract " + filename);
            continue;
        }

        if (endsWithIgnoreCase(lower, ".txt"))
        {
            result.metrics["txt"]++;
            if (!parseKnownTxt(lower, *data, logger))
            {
                result.metrics["txtFailures"]++;
                addError(result, "failed to parse TXT " + filename);
            }
        }
        else if (endsWithIgnoreCase(lower, ".str"))
        {
            result.metrics["str"]++;
            const auto strings = evtParser.parseStringTable(*data);
            result.metrics["strings"] += strings.size();
        }
        else if (endsWithIgnoreCase(lower, ".evt"))
        {
            result.metrics["evt"]++;
            std::vector<std::string> strings;
            const auto strIt = fileIndex.find(stemLower(filename) + ".str");
            if (strIt != fileIndex.end())
            {
                if (auto strData = archive.extractFile(strIt->second))
                {
                    strings = evtParser.parseStringTable(*strData);
                }
            }

            const auto scripts = evtParser.parseEventData(*data, strings);
            result.metrics["eventScripts"] += scripts.size();
            for (const auto& script : scripts)
            {
                result.metrics["eventCommands"] += script.commands.size();
                for (const auto& command : script.commands)
                {
                    result.metrics["opcode." + std::to_string(static_cast<int>(command.opcode))]++;
                }
            }
        }
        else if (endsWithIgnoreCase(lower, ".bin"))
        {
            result.metrics["bin"]++;
            if (lower == "dsft.bin")
            {
                SpriteFrameTable table;
                if (!table.parse(*data))
                {
                    addError(result, "failed to parse dsft.bin");
                }
                else
                {
                    result.metrics["spriteFrames"] = table.entries().size();
                }
            }
            else if (lower == "dtft.bin")
            {
                TextureFrameTable table;
                if (!table.parse(*data))
                {
                    addError(result, "failed to parse dtft.bin");
                }
                else
                {
                    result.metrics["textureFrames"] = table.entries().size();
                }
            }
            else if (lower == "dsounds.bin")
            {
                SoundList sounds(logger);
                if (!sounds.parse(*data))
                {
                    addError(result, "failed to parse dsounds.bin");
                }
                else
                {
                    result.metrics["soundListEntries"] = sounds.getAllSounds().size();
                }
            }
        }
    }

    absorbLoggerMessages(result, logger);
    return result;
}

CheckResult checkSoundArchive(const fs::path& installRoot, const fs::path& dataRoot,
                              QuietLogger& logger)
{
    using namespace runeharbor::formats;

    CheckResult result;
    result.name = "Audio.snd";

    std::vector<fs::path> candidates = {
        dataRoot / "Audio.snd",
        dataRoot / "AUDIO.SND",
        dataRoot.parent_path() / "SOUNDS" / "Audio.snd",
        dataRoot.parent_path() / "SOUNDS" / "AUDIO.SND",
        installRoot / "SOUNDS" / "Audio.snd",
        installRoot / "SOUNDS" / "AUDIO.SND",
    };

    std::optional<fs::path> path;
    for (const auto& candidate : candidates)
    {
        if (fs::exists(candidate))
        {
            path = candidate;
            break;
        }
    }

    if (!path)
    {
        addError(result, "Audio.snd not found");
        return result;
    }

    SndArchive archive(logger);
    if (!archive.open(*path))
    {
        addError(result, "failed to open Audio.snd");
        absorbLoggerMessages(result, logger);
        return result;
    }

    // 02Flame01.wav is corrupt in the retail GOG Audio.snd (zlib error -3).
    // Record it as an accepted data defect so new failures still fail the gate.
    const auto isAcceptedFailure = [](std::string_view filename)
    { return lowerCopy(filename) == "02flame01.wav"; };

    const auto files = archive.listFiles();
    result.metrics["entries"] = files.size();
    bool acceptedCorruptEntry = false;
    for (const auto& filename : files)
    {
        if (filename.empty())
        {
            continue;
        }
        auto data = archive.extractFile(filename);
        if (!data || data->empty())
        {
            result.metrics["extractFailures"]++;
            if (isAcceptedFailure(filename))
            {
                result.metrics["acceptedFailures"]++;
                acceptedCorruptEntry = true;
                addWarning(result, "accepted corrupt sound entry " + filename);
                continue;
            }
            if (result.errors.size() < 25)
            {
                addError(result, "failed to extract sound " + filename);
            }
            else
            {
                result.ok = false;
            }
            continue;
        }
        result.metrics["decodedBytes"] += data->size();
    }

    absorbLoggerMessages(result, logger);

    // The accepted retail defect also logs a zlib error from SndArchive; it is
    // the same 02Flame01.wav entry, not a new failure.
    if (acceptedCorruptEntry)
    {
        std::erase_if(result.errors, [](const std::string& error)
                      { return error.find("zlib decompression failed") != std::string::npos; });
        result.ok = result.errors.empty();
    }
    return result;
}

CheckResult checkVidArchive(const fs::path& installRoot, std::string_view relativePath)
{
    CheckResult result;
    result.name = std::string(relativePath);

    const fs::path path = installRoot / fs::path(std::string(relativePath));
    if (!fs::exists(path))
    {
        addError(result, "archive not found");
        return result;
    }

    runeharbor::media::VidArchive archive;
    if (!archive.open(path))
    {
        addError(result, "failed to open VID archive");
        return result;
    }

    result.metrics["entries"] = archive.entryCount();
    for (const auto& entry : archive.entries())
    {
        result.metrics["bytes"] += entry.size;
        switch (entry.format)
        {
        case runeharbor::media::VideoFormat::Bink:
            result.metrics["bink"]++;
            break;
        case runeharbor::media::VideoFormat::Smacker:
            result.metrics["smacker"]++;
            break;
        case runeharbor::media::VideoFormat::Unknown:
            result.metrics["unknown"]++;
            addWarning(result, "unknown video format: " + entry.name);
            break;
        }
    }

    return result;
}

std::string writeJson(const Report& report, bool strict)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"ok\": " << (report.ok(strict) ? "true" : "false") << ",\n";
    out << "  \"strict\": " << (strict ? "true" : "false") << ",\n";
    out << "  \"warnings\": " << report.warningCount() << ",\n";
    out << "  \"installRoot\": \"" << jsonEscape(report.installRoot.string()) << "\",\n";
    out << "  \"dataRoot\": \"" << jsonEscape(report.dataRoot.string()) << "\",\n";
    out << "  \"checks\": [\n";

    for (size_t i = 0; i < report.checks.size(); i++)
    {
        const auto& check = report.checks[i];
        out << "    {\n";
        out << "      \"name\": \"" << jsonEscape(check.name) << "\",\n";
        out << "      \"ok\": " << (check.ok ? "true" : "false") << ",\n";
        out << "      \"metrics\": {";
        bool firstMetric = true;
        for (const auto& [key, value] : check.metrics)
        {
            if (!firstMetric)
            {
                out << ", ";
            }
            firstMetric = false;
            out << "\"" << jsonEscape(key) << "\": " << value;
        }
        out << "},\n";

        auto writeStringArray = [&](std::string_view name, const std::vector<std::string>& values)
        {
            out << "      \"" << name << "\": [";
            for (size_t j = 0; j < values.size(); j++)
            {
                if (j != 0)
                {
                    out << ", ";
                }
                out << "\"" << jsonEscape(values[j]) << "\"";
            }
            out << "]";
        };

        writeStringArray("warnings", check.warnings);
        out << ",\n";
        writeStringArray("errors", check.errors);
        out << "\n    }";
        if (i + 1 < report.checks.size())
        {
            out << ',';
        }
        out << '\n';
    }

    out << "  ]\n";
    out << "}\n";
    return out.str();
}

void printSummary(const Report& report, bool strict)
{
    std::cout << "RuneHarbor asset check\n";
    std::cout << "  installRoot: " << report.installRoot << '\n';
    std::cout << "  dataRoot:    " << report.dataRoot << '\n';
    std::cout << "  mode:        " << (strict ? "strict" : "compatibility") << '\n';
    std::cout << "  status:      " << (report.ok(strict) ? "OK" : "FAILED") << '\n';
    std::cout << "  errors:      " << report.errorCount() << '\n';
    std::cout << "  warnings:    " << report.warningCount() << "\n\n";

    for (const auto& check : report.checks)
    {
        std::cout << (check.ok ? "[OK]   " : "[FAIL] ") << check.name;
        if (auto it = check.metrics.find("entries"); it != check.metrics.end())
        {
            std::cout << " entries=" << it->second;
        }
        if (auto it = check.metrics.find("decodedBytes"); it != check.metrics.end())
        {
            std::cout << " decodedBytes=" << it->second;
        }
        std::cout << '\n';

        for (const auto& error : check.errors)
        {
            std::cout << "       error: " << error << '\n';
        }
    }
}

void printUsage(const char* argv0)
{
    std::cerr << "Usage: " << argv0
              << " <MM7 install root or DATA dir> [--json <path>] [--strict]\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage(argv[0]);
        return 2;
    }

    fs::path inputPath = argv[1];
    std::optional<fs::path> jsonPath;
    bool strict = false;
    for (int i = 2; i < argc; i++)
    {
        const std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc)
        {
            jsonPath = argv[++i];
        }
        else if (arg == "--strict")
        {
            strict = true;
        }
        else
        {
            printUsage(argv[0]);
            return 2;
        }
    }

    if (!fs::exists(inputPath))
    {
        std::cerr << "Input path does not exist: " << inputPath << '\n';
        return 2;
    }

    Report report;
    if (lowerCopy(inputPath.filename().string()) == "data")
    {
        report.dataRoot = fs::absolute(inputPath);
        report.installRoot = report.dataRoot.parent_path();
    }
    else
    {
        report.installRoot = fs::absolute(inputPath);
        report.dataRoot = report.installRoot / "DATA";
    }

    QuietLogger logger;
    report.checks.push_back(checkImageArchive(report.dataRoot, "Events.lod", logger));
    report.checks.push_back(checkImageArchive(report.dataRoot, "BITMAPS.LOD", logger));
    report.checks.push_back(checkImageArchive(report.dataRoot, "ICONS.LOD", logger));
    report.checks.push_back(checkSprites(report.dataRoot, logger));
    report.checks.push_back(checkGameMaps(report.dataRoot, logger));
    report.checks.push_back(checkEvents(report.dataRoot, logger));
    report.checks.push_back(checkSoundArchive(report.installRoot, report.dataRoot, logger));
    report.checks.push_back(checkVidArchive(report.installRoot, "Anims/Magic7.vid"));
    report.checks.push_back(checkVidArchive(report.installRoot, "Anims/Might7.vid"));

    printSummary(report, strict);

    if (jsonPath)
    {
        std::ofstream out(*jsonPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::cerr << "Failed to write JSON report: " << *jsonPath << '\n';
            return 2;
        }
        const std::string json = writeJson(report, strict);
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        std::cout << "\nJSON report written to " << *jsonPath << '\n';
    }

    return report.ok(strict) ? 0 : 1;
}
