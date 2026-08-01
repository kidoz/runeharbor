// SPDX-License-Identifier: MIT
#include "application.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <vector>

#include <cctype>
#include <cmath>

#include "../formats/autonote_parser.hpp"
#include "../formats/awards_parser.hpp"
#include "../formats/credits_parser.hpp"
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
#include "../formats/pcx_image.hpp"
#include "../formats/placemon_parser.hpp"
#include "../formats/quests_parser.hpp"
#include "../formats/rnditems_parser.hpp"
#include "../formats/spells_parser.hpp"
#include "../formats/sprite_lod_archive.hpp"
#include "../formats/sprite_parser.hpp"
#include "../formats/two_d_events_parser.hpp"
#include "../graphics/bitmap_font.hpp"
#include "../graphics/image.hpp"
#include "../graphics/line_renderer.hpp"
#include "../graphics/palette.hpp"
#include "../graphics/sdl_renderer.hpp"
#include "../graphics/sprite_decoder.hpp"
#include "../graphics/visibility.hpp"
#include "../graphics/world_coordinates.hpp"
#include "../graphics/world_renderer.hpp"
#include "../media/vid_archive.hpp"
#include "../media/vid_manifest.hpp"
#include "../media/video_player.hpp"
#include "../platform/iwindow.hpp"
#include "../util/ilogger.hpp"
#include "map_transition_resolver.hpp"
#include "outdoor_terrain.hpp"
#include "states/character_creation_state.hpp"
#include "states/credits_state.hpp"
#include "states/ingame_state.hpp"
#include "states/intro_state.hpp"
#include "states/load_game_state.hpp"
#include "states/loading_state.hpp"
#include "states/state_context.hpp"
#include "states/title_state.hpp"
#include "virtual_filesystem.hpp"

namespace runeharbor::engine
{

Application::Application(util::ILogger& logger, platform::IWindow& window)
    : logger(logger), window(window), audioSystem_(std::make_unique<audio::AudioSystem>(logger)),
      vfs(std::make_unique<VirtualFileSystem>(logger)),
      evtParser_(std::make_unique<formats::EvtScriptParser>(logger)),
      contentGenerator_(std::make_unique<game::ContentGenerator>(logger)),
      gameWorld_(std::make_unique<game::GameWorld>()),
      eventEngine_(std::make_unique<game::EventEngine>(logger)),
      combatSystem_(std::make_unique<game::CombatSystem>(logger)),
      spellSystem_(std::make_unique<game::SpellSystem>(logger)),
      inventory_(std::make_unique<game::Inventory>(logger)),
      questLog_(std::make_unique<game::QuestLog>(logger)),
      saveGame_(std::make_unique<game::SaveGame>(logger)),
      sndArchive_(std::make_unique<formats::SndArchive>(logger)),
      soundList_(std::make_unique<formats::SoundList>(logger))
{
    if (eventEngine_)
    {
        eventEngine_->setGameWorld(gameWorld_.get());
        eventEngine_->setCombatSystem(combatSystem_.get());
        eventEngine_->setSpellSystem(spellSystem_.get());
        eventEngine_->setInventory(inventory_.get());
    }
    if (combatSystem_)
    {
        combatSystem_->setGameWorld(gameWorld_.get());
        combatSystem_->setInventory(inventory_.get());
    }
    if (spellSystem_)
    {
        spellSystem_->setGameWorld(gameWorld_.get());
    }

    configureGameplayCallbacks();
    initDefaultParty();
}

namespace
{
std::string toLower(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::string getLowerExtension(const std::string& filename)
{
    const std::string lower = toLower(filename);
    const size_t dot = lower.find_last_of('.');
    if (dot == std::string::npos)
    {
        return "";
    }
    return lower.substr(dot);
}

std::string normalizeMapName(std::string name, bool preferOutdoor)
{
    if (name.empty())
    {
        return name;
    }

    const std::string ext = getLowerExtension(name);
    if (ext == ".blv" || ext == ".odm")
    {
        return name;
    }

    const std::string lower = toLower(name);
    if (preferOutdoor || lower.rfind("out", 0) == 0 || lower.rfind("i", 0) == 0)
    {
        return name + ".odm";
    }

    return name + ".blv";
}

const char* directionName(int direction)
{
    switch (resolveSpawnIndexFromDirection(direction))
    {
    case 1:
        return "North";
    case 2:
        return "South";
    case 3:
        return "East";
    case 4:
        return "West";
    default:
        return "Default";
    }
}

// Base stats per face group (indexed by faceId ranges)
// MM7 has no character races; base stats are determined by face/portrait
constexpr int kFaceBaseStats[4][7] = {
    {11, 11, 11, 9, 11, 11, 9}, // Faces 0-7
    {7, 14, 11, 7, 11, 14, 9},  // Faces 8-11
    {14, 11, 11, 14, 7, 7, 9},  // Faces 12-15
    {14, 7, 7, 11, 11, 14, 9},  // Faces 16-19
};

// Face group index from faceId
int faceGroupFromId(int faceId)
{
    if (faceId < 8)
        return 0;
    if (faceId < 12)
        return 1;
    if (faceId < 16)
        return 2;
    return 3;
}

// Starting skills per base class (indexed by baseClassIndex)
struct ClassSkills
{
    const char* skill1;
    const char* skill2;
};

constexpr ClassSkills kClassStartingSkills[] = {
    {"Sword", "Leather Armor"}, // Knight (base 0)
    {"Dagger", "Stealing"},     // Thief (base 1)
    {"Dodging", "Unarmed"},     // Monk (base 2)
    {"Mace", "Spirit Magic"},   // Paladin (base 3)
    {"Bow", "Air Magic"},       // Archer (base 4)
    {"Axe", "Perception"},      // Ranger (base 5)
    {"Mace", "Body Magic"},     // Cleric (base 6)
    {"Dagger", "Earth Magic"},  // Druid (base 7)
    {"Staff", "Fire Magic"},    // Sorcerer (base 8)
};

void groundPartyToOutdoorTerrain(game::Party& party, const MapScene* scene,
                                 util::ILogger* /*logger*/ = nullptr, const char* tag = "ground")
{
    if (!scene || scene->getODMData().heightmap.empty())
    {
        fprintf(stderr, "[GROUND/%s] skipped (no heightmap), pos=(%.1f,%.1f,%.1f) yaw=%.1f\n", tag,
                party.worldX(), party.worldY(), party.worldZ(), party.yaw());
        return;
    }

    const float beforeZ = party.worldZ();
    const float floor =
        sampleOutdoorTerrainHeight(scene->getODMData(), party.worldX(), party.worldY());
    const float newZ =
        clampOutdoorPartyZ(scene->getODMData(), party.worldX(), party.worldY(), party.worldZ());
    party.setWorldPosition(party.worldX(), party.worldY(), newZ);

    fprintf(stderr, "[GROUND/%s] pos=(%.1f,%.1f) z: %.1f -> %.1f (terrain floor=%.1f) yaw=%.1f\n",
            tag, party.worldX(), party.worldY(), beforeZ, newZ, floor, party.yaw());
}
} // namespace

Application::~Application()
{
    if (initialized)
    {
        shutdown();
    }
}

bool Application::initialize(const platform::WindowConfig& windowConfig)
{
    logger.info("RuneHarbor Engine v0.1.0");
    logger.info("Initializing...");

    if (!window.initialize(windowConfig))
    {
        logger.error("Failed to initialize window");
        return false;
    }

    logger.info("Window created successfully");

    // Create renderer
    SDL_Window* sdlWindow = window.getSDLWindow();
    if (!sdlWindow)
    {
        logger.error("Failed to get SDL window for renderer creation");
        return false;
    }

    renderer = std::make_unique<graphics::SDLRenderer>(sdlWindow, logger);
    if (!renderer->getSDLRenderer())
    {
        logger.error("Renderer initialization failed");
        return false;
    }
    logger.info("Renderer created successfully");

    worldRenderer = std::make_unique<graphics::WorldRenderer>(
        *dynamic_cast<graphics::SDLRenderer*>(renderer.get()), logger);
    lineRenderer = std::make_unique<graphics::LineRenderer>(renderer->getSDLRenderer(), logger);
    debugText = std::make_unique<graphics::DebugText>();
    mapScene = std::make_unique<MapScene>(logger);
    mapScene->setTileTable(&tileTable_);
    videoPlayer = std::make_unique<media::VideoPlayer>();
    if (audioSystem_ && !audioSystem_->initialize())
    {
        logger.warning("Audio subsystem unavailable; continuing without sound effects");
    }
    if (audioSystem_)
    {
        audioSystem_->setMaxChannels(bootConfig_.mixerChannels);
    }
    updateViewport();

    logger.info("Press ESC or close window to exit");

    if (contentGenerator_)
    {
        const uint64_t seed = static_cast<uint64_t>(SDL_GetTicks());
        contentGenerator_->setWorldSeed(seed);
        logger.info(std::format("Content generator seed initialized from startup ticks: {}", seed));
    }

    initStates();

    setGameState(GameState::IntroVideo);
    transitionTo(GameStateId::IntroVideo);
    initialized = true;
    return true;
}

void Application::initStates()
{
    // Create shared data
    sharedData = std::make_unique<SharedGameData>();
    sharedData->party = &party;
    sharedData->gameWorld = gameWorld_.get();
    sharedData->eventEngine = eventEngine_.get();
    sharedData->combatSystem = combatSystem_.get();
    sharedData->spellSystem = spellSystem_.get();
    sharedData->inventory = inventory_.get();
    sharedData->questLog = questLog_.get();
    sharedData->autonoteCatalog = &autonoteEntries_;
    sharedData->awardCatalog = &awardEntries_;
    sharedData->saveGame = saveGame_.get();
    sharedData->newGameStartMapName = defaultStartMapName_;

    // Create state context
    stateCtx = std::make_unique<StateContext>(StateContext{
        .logger = logger,
        .window = window,
        .renderer = renderer.get(),
        .sdlRenderer = dynamic_cast<graphics::SDLRenderer*>(renderer.get()),
        .lineRenderer = lineRenderer.get(),
        .worldRenderer = worldRenderer.get(),
        .debugText = debugText.get(),
        .videoPlayer = videoPlayer.get(),
        .vfs = vfs.get(),
        .camera = &camera,
        .shared = sharedData.get(),
    });
    updateStateContext();

    // Create state objects
    introState = std::make_unique<IntroState>(*stateCtx);
    titleState = std::make_unique<TitleState>(*stateCtx);
    charCreationState = std::make_unique<CharacterCreationState>(*stateCtx);
    loadingState = std::make_unique<LoadingState>(*stateCtx);
    inGameState = std::make_unique<InGameState>(*stateCtx);
    creditsState = std::make_unique<CreditsState>(*stateCtx);
    loadGameState = std::make_unique<LoadGameState>(*stateCtx);

    // Wire up loading state callbacks
    loadingState->setProgress(&loadProgress);
    loadingState->setCallbacks([this]() { startLoadingTask(); },
                               [this]() -> bool { return loadingTaskDone.load(); },
                               [this]() -> bool
                               {
                                   finalizeLoadingTask();
                                   return mapLoaded;
                               });
}

void Application::updateStateContext()
{
    if (!stateCtx)
    {
        return;
    }
    stateCtx->viewportWidth = viewportWidth;
    stateCtx->viewportHeight = viewportHeight;
    stateCtx->keyState = keyState;
    stateCtx->previousKeyState = &previousKeyState;
    stateCtx->keyCount = keyCount;

    // Wire bitmap fonts
    stateCtx->createFont = createFont_.get();
    stateCtx->ccharFont = ccharFont_.get();
    stateCtx->arrusFont = arrusFont_.get();
    stateCtx->smallnumFont = smallnumFont_.get();

    // Audio
    stateCtx->playUiSound = [this](const std::string& name) { playUiSound(name); };

    if (sharedData)
    {
        sharedData->mapScene = mapScene.get();
    }
}

void Application::transitionTo(GameStateId id)
{
    logger.info(std::format("transitionTo: {}", static_cast<int>(id)));

    if (activeState)
    {
        activeState->exit();
    }

    activeStateId = id;

    switch (id)
    {
    case GameStateId::IntroVideo:
        activeState = introState.get();
        break;
    case GameStateId::TitleScreen:
        activeState = titleState.get();
        break;
    case GameStateId::CharacterCreation:
        activeState = charCreationState.get();
        break;
    case GameStateId::Loading:
        activeState = loadingState.get();
        break;
    case GameStateId::InGame:
        activeState = inGameState.get();
        break;
    case GameStateId::Credits:
        activeState = creditsState.get();
        break;
    case GameStateId::LoadGame:
        activeState = loadGameState.get();
        break;
    case GameStateId::Quit:
    {
        SDL_Event quitEvent = {};
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
        return;
    }
    default:
        // Unknown id: do not touch activeState — the previous state was already
        // exit()-ed above, and entering it again would be wrong. Log and bail.
        logger.warning(std::format("transitionTo: unknown GameStateId {}", static_cast<int>(id)));
        return;
    }

    if (activeState)
    {
        activeState->enter();
    }
}

bool Application::loadGameData(const std::filesystem::path& dataPath)
{
    logger.info(std::format("Loading game data from: {}", dataPath.string()));

    if (!std::filesystem::exists(dataPath))
    {
        logger.error(std::format("Game data path does not exist: {}", dataPath.string()));
        return false;
    }

    dataRoot = dataPath;
    gameRoot = dataPath.parent_path();
    if (saveGame_)
    {
        const std::filesystem::path saveDir =
            gameRoot.empty() ? std::filesystem::path("saves") : (gameRoot / "saves");
        saveGame_->setSaveDirectory(saveDir.string());
    }

    // Text/data archives
    const std::vector<std::string> textArchives = {
        // "Events.lod" is actually using the Image LOD format with 48-byte headers
    };

    // Image archives (use different format)
    std::vector<std::string> imageArchives = {
        "Events.lod",
        "BITMAPS.LOD",
        "ICONS.LOD",
    };
    std::vector<std::string> spriteArchives;
    if (preferLowResSprites_)
    {
        spriteArchives.push_back("SPRITELO.LOD");
        spriteArchives.push_back("SPRITES.LOD");
        logger.info("Low-resolution sprite mode enabled (preferring SPRITELO.LOD)");
    }
    else
    {
        spriteArchives.push_back("SPRITES.LOD");
    }

    const std::vector<std::string> gameArchives = {
        "GAMES.LOD",
    };

    size_t mountedCount = 0;

    // Mount text archives
    for (const auto& archiveName : textArchives)
    {
        auto archivePath = dataPath / archiveName;

        // Try both exact case and lowercase
        if (!std::filesystem::exists(archivePath))
        {
            std::string lowerName = archiveName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            archivePath = dataPath / lowerName;
        }

        if (std::filesystem::exists(archivePath))
        {
            if (vfs->mountArchive(archivePath))
            {
                mountedCount++;
            }
        }
        else
        {
            logger.debug(std::format("Text archive not found (skipping): {}", archiveName));
        }
    }

    // Mount image archives
    for (const auto& archiveName : imageArchives)
    {
        auto archivePath = dataPath / archiveName;

        // Try both exact case and lowercase
        if (!std::filesystem::exists(archivePath))
        {
            std::string lowerName = archiveName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            archivePath = dataPath / lowerName;
        }

        if (std::filesystem::exists(archivePath))
        {
            if (vfs->mountImageArchive(archivePath))
            {
                mountedCount++;
            }
        }
        else
        {
            logger.debug(std::format("Image archive not found (skipping): {}", archiveName));
        }
    }

    // Mount sprite archives
    for (const auto& archiveName : spriteArchives)
    {
        auto archivePath = dataPath / archiveName;

        if (!std::filesystem::exists(archivePath))
        {
            std::string lowerName = archiveName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            archivePath = dataPath / lowerName;
        }

        if (std::filesystem::exists(archivePath))
        {
            if (vfs->mountSpriteArchive(archivePath))
            {
                mountedCount++;
            }
        }
        else
        {
            logger.debug(std::format("Sprite archive not found (skipping): {}", archiveName));
        }
    }

    // Mount game archives
    for (const auto& archiveName : gameArchives)
    {
        auto archivePath = dataPath / archiveName;

        // Try both exact case and lowercase
        if (!std::filesystem::exists(archivePath))
        {
            std::string lowerName = archiveName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            archivePath = dataPath / lowerName;
        }

        if (std::filesystem::exists(archivePath))
        {
            if (vfs->mountGameArchive(archivePath))
            {
                mountedCount++;
            }
        }
        else
        {
            logger.debug(std::format("Game archive not found (skipping): {}", archiveName));
        }
    }

    if (mountedCount == 0)
    {
        logger.error("No LOD archives were mounted");
        return false;
    }

    logger.info(std::format("Successfully mounted {} LOD archive(s)", mountedCount));

    // Optional sound archive (Audio.snd) used for event-driven WAV playback.
    if (sndArchive_)
    {
        sndArchive_->close();
    }
    loadedSounds_.clear();
    const std::array<std::string, 6> sndCandidates = {"Audio.snd",           "audio.snd",
                                                      "AUDIO.SND",           "../SOUNDS/Audio.snd",
                                                      "../SOUNDS/audio.snd", "../SOUNDS/AUDIO.SND"};
    for (const auto& name : sndCandidates)
    {
        const auto sndPath = dataPath / name;
        if (!std::filesystem::exists(sndPath))
        {
            // Also try relative to the parent of dataPath for better flexibility
            const auto altPath = dataPath.parent_path() / "SOUNDS" / name;
            if (std::filesystem::exists(altPath) && sndArchive_ && sndArchive_->open(altPath))
            {
                break;
            }
            continue;
        }
        if (sndArchive_ && sndArchive_->open(sndPath))
        {
            break;
        }
    }

    // Demo: list some files
    auto allFiles = vfs->listAllFiles();
    logger.info(std::format("Total files available: {}", allFiles.size()));

    // Demo: read a test file if available
    if (vfs->fileExists("Global.txt"))
    {
        auto data = vfs->readFile("Global.txt");
        if (data.has_value())
        {
            logger.info(std::format("Demo: Successfully read Global.txt ({} bytes)", data->size()));
        }
    }

    loadMapStatsTable();
    loadDataTables();
    loadGlobalEventScripts();

    gameDataLoaded = true;

    auto readFirstExisting =
        [this](const std::vector<std::string>& names) -> std::optional<std::vector<uint8_t>>
    {
        for (const auto& name : names)
        {
            if (auto data = vfs->readFile(name); data.has_value())
            {
                return data;
            }
        }
        return std::nullopt;
    };

    bool spriteFramesLoaded = false;
    bool textureFramesLoaded = false;

    if (useDefsMode_)
    {
        logger.info("Development data mode enabled (-usedefs)");

        if (auto sftData = readFirstExisting({"sft.txt", "SFT.TXT"}); sftData.has_value())
        {
            const std::string text(reinterpret_cast<const char*>(sftData->data()), sftData->size());
            if (spriteFrameTable_.parseText(text))
            {
                spriteFramesLoaded = true;
                logger.info(std::format("Parsed {} sprite frame entries from sft.txt",
                                        spriteFrameTable_.entries().size()));
            }
            else
            {
                logger.warning("Failed to parse sft.txt (sprite frame table)");
            }
        }

        if (auto tftData = readFirstExisting({"tft.def", "TFT.DEF"}); tftData.has_value())
        {
            const std::string text(reinterpret_cast<const char*>(tftData->data()), tftData->size());
            if (textureFrameTable_.parseText(text))
            {
                textureFramesLoaded = true;
                logger.info(std::format("Parsed {} texture frame entries from tft.def",
                                        textureFrameTable_.entries().size()));
            }
            else
            {
                logger.warning("Failed to parse tft.def (texture frame table)");
            }
        }
    }

    // Fallback to binary frame tables from events.lod.
    if (!spriteFramesLoaded)
    {
        if (auto dsftData = vfs->readFile("dsft.bin"); dsftData.has_value())
        {
            if (spriteFrameTable_.parse(*dsftData))
            {
                spriteFramesLoaded = true;
                logger.info(std::format("Parsed {} sprite frame entries",
                                        spriteFrameTable_.entries().size()));
            }
            else
            {
                logger.warning("Failed to parse dsft.bin (sprite frame table)");
            }
        }
    }

    if (!textureFramesLoaded)
    {
        if (auto dtftData = vfs->readFile("dtft.bin"); dtftData.has_value())
        {
            if (textureFrameTable_.parse(*dtftData))
            {
                textureFramesLoaded = true;
                logger.info(std::format("Parsed {} texture frame entries",
                                        textureFrameTable_.entries().size()));
            }
            else
            {
                logger.warning("Failed to parse dtft.bin (texture frame table)");
            }
        }
    }

    if (auto dtileData = vfs->readFile("dtile.bin"); dtileData.has_value())
    {
        if (tileTable_.parse(*dtileData))
        {
            logger.info(std::format("Parsed {} terrain tile entries", tileTable_.entries().size()));
        }
        else
        {
            logger.warning("Failed to parse dtile.bin (terrain tile table)");
        }
    }
    else
    {
        logger.warning("dtile.bin not found; outdoor terrain will use fallback tile textures");
    }

    refreshIntroPlaylist();
    loadUiAssets();
    return true;
}

void Application::refreshIntroPlaylist()
{
    buildIntroPlaylist();
    if (!introState)
    {
        return;
    }

    introState->setPlaylist(introPlaylist);

    // initialize() enters the intro state before any game data is available, so that
    // first enter() always saw an empty playlist and never started playback. Re-enter
    // now that the clips are known, keeping IntroState the single owner of startup.
    if (activeStateId == GameStateId::IntroVideo)
    {
        introState->enter();
    }
}

void Application::configureBootFlow(const std::string& mapName, bool preferOutdoor, bool autoLoad)
{
    startupMapName = mapName;
    startupPreferOutdoor = preferOutdoor;
    autoLoadMap = autoLoad;
    pendingEntryDirection_ = 0;
    pendingArrivalOverride_.active = false;
    pendingTransition_ = {};

    // Sync to shared data
    if (sharedData)
    {
        sharedData->startupMapName = mapName;
        sharedData->startupPreferOutdoor = preferOutdoor;
        sharedData->autoLoadMap = autoLoad;
        sharedData->loadingScreenIndex = 0;
        sharedData->loadFromSave = false;
    }

    // If map name is specified via CLI, skip menu and go directly to loading
    if (!mapName.empty() && autoLoad &&
        (gameState == GameState::TitleScreen || gameState == GameState::IntroVideo))
    {
        quickStartReady = true;
        if (sharedData)
            sharedData->quickStartReady = true;
        setGameState(GameState::Loading);
        transitionTo(GameStateId::Loading);
    }
}

bool Application::loadMap(const std::string& mapName)
{
    if (!gameDataLoaded || !vfs)
    {
        logger.warning("Cannot load map: game data not loaded");
        return false;
    }

    if (mapName.empty())
    {
        logger.warning("Cannot load map: empty map name");
        return false;
    }

    std::string resolvedName = mapName;
    std::string ext = getLowerExtension(mapName);
    if (!ext.empty() && ext != ".blv")
    {
        logger.error(std::format("Unsupported map extension '{}'; only .blv is supported", ext));
        return false;
    }

    if (ext.empty())
    {
        resolvedName = mapName + ".blv";
        ext = ".blv";
    }

    if (!vfs->fileExists(resolvedName))
    {
        logger.error(std::format("Map not found in archives: {}", resolvedName));
        logger.info("Tip: run with --list-maps to see available BLV maps");
        return false;
    }

    auto data = vfs->readFile(resolvedName);
    if (!data.has_value())
    {
        logger.error(std::format("Failed to read map data: {}", resolvedName));
        return false;
    }

    updateLoadProgress(0.1f);

    if (!mapScene)
    {
        mapScene = std::make_unique<MapScene>(logger);
    }
    mapScene->setTileTable(&tileTable_);

    auto progress = [this](float value) { updateLoadProgress(0.1f + value * 0.85f); };
    if (!mapScene->loadBLV(resolvedName, *data, progress))
    {
        logger.error(std::format("Failed to parse BLV map: {}", resolvedName));
        return false;
    }

    mapLoaded = true;
    configureCameraForMap();
    wireUpMapTextures();

    logger.info(std::format("Loaded BLV map: {}", resolvedName));
    logger.info("Controls: Arrow keys move/turn, PageUp/PageDown look, Insert/Delete strafe");
    return true;
}

bool Application::loadFirstMap()
{
    if (!gameDataLoaded || !vfs)
    {
        logger.warning("Cannot load map: game data not loaded");
        return false;
    }

    auto files = vfs->listAllFiles();
    std::vector<std::string> blvFiles;
    blvFiles.reserve(files.size());

    for (const auto& name : files)
    {
        if (getLowerExtension(name) == ".blv")
        {
            blvFiles.push_back(name);
        }
    }

    if (blvFiles.empty())
    {
        logger.warning("No BLV maps found in archives");
        return false;
    }

    std::sort(blvFiles.begin(), blvFiles.end());
    for (const auto& name : blvFiles)
    {
        if (toLower(name) == "d01.blv")
        {
            return loadMap(name);
        }
    }

    return loadMap(blvFiles.front());
}

bool Application::loadOutdoorMap(const std::string& mapName)
{
    if (!gameDataLoaded || !vfs)
    {
        logger.warning("Cannot load map: game data not loaded");
        return false;
    }

    if (mapName.empty())
    {
        logger.warning("Cannot load map: empty map name");
        return false;
    }

    std::string resolvedName = mapName;
    std::string ext = getLowerExtension(mapName);
    if (!ext.empty() && ext != ".odm")
    {
        logger.error(std::format("Unsupported map extension '{}'; only .odm is supported", ext));
        return false;
    }

    if (ext.empty())
    {
        resolvedName = mapName + ".odm";
        ext = ".odm";
    }

    if (!vfs->fileExists(resolvedName))
    {
        logger.error(std::format("Map not found in archives: {}", resolvedName));
        logger.info("Tip: run with --list-maps to see available maps");
        return false;
    }

    auto data = vfs->readFile(resolvedName);
    if (!data.has_value())
    {
        logger.error(std::format("Failed to read map data: {}", resolvedName));
        return false;
    }

    updateLoadProgress(0.1f);

    if (!mapScene)
    {
        mapScene = std::make_unique<MapScene>(logger);
    }
    mapScene->setTileTable(&tileTable_);

    auto progress = [this](float value) { updateLoadProgress(0.1f + value * 0.85f); };
    if (!mapScene->loadODM(resolvedName, *data, progress))
    {
        logger.error(std::format("Failed to parse ODM map: {}", resolvedName));
        return false;
    }

    mapLoaded = true;
    configureCameraForMap();
    wireUpMapTextures();

    logger.info(std::format("Loaded ODM map: {}", resolvedName));
    logger.info("Controls: Arrow keys move/turn, PageUp/PageDown look, Insert/Delete strafe");
    return true;
}

bool Application::loadFirstOutdoorMap()
{
    if (!gameDataLoaded || !vfs)
    {
        logger.warning("Cannot load map: game data not loaded");
        return false;
    }

    auto files = vfs->listAllFiles();
    std::vector<std::string> odmFiles;
    odmFiles.reserve(files.size());

    for (const auto& name : files)
    {
        if (getLowerExtension(name) == ".odm")
        {
            odmFiles.push_back(name);
        }
    }

    if (odmFiles.empty())
    {
        logger.warning("No ODM maps found in archives");
        return false;
    }

    std::sort(odmFiles.begin(), odmFiles.end());
    for (const auto& name : odmFiles)
    {
        if (toLower(name) == "out01.odm")
        {
            return loadOutdoorMap(name);
        }
    }

    return loadOutdoorMap(odmFiles.front());
}

std::vector<std::string> Application::listMaps() const
{
    std::vector<std::string> blvFiles;
    if (!gameDataLoaded || !vfs)
    {
        return blvFiles;
    }

    auto files = vfs->listAllFiles();
    blvFiles.reserve(files.size());

    for (const auto& name : files)
    {
        if (getLowerExtension(name) == ".blv")
        {
            blvFiles.push_back(name);
        }
    }

    std::sort(blvFiles.begin(), blvFiles.end());
    return blvFiles;
}

std::vector<std::string> Application::listOutdoorMaps() const
{
    std::vector<std::string> odmFiles;
    if (!gameDataLoaded || !vfs)
    {
        return odmFiles;
    }

    auto files = vfs->listAllFiles();
    odmFiles.reserve(files.size());

    for (const auto& name : files)
    {
        if (getLowerExtension(name) == ".odm")
        {
            odmFiles.push_back(name);
        }
    }

    std::sort(odmFiles.begin(), odmFiles.end());
    return odmFiles;
}

void Application::run()
{
    if (!initialized)
    {
        logger.error("Cannot run application: not initialized");
        return;
    }

    if (!renderer)
    {
        logger.error("Cannot run application: renderer not initialized");
        return;
    }

    logger.info("Entering main loop...");

    // Main loop
    while (!window.shouldClose())
    {
        window.processEvents();
        updateStateMachine();

        updateViewport();
        renderFrame();

        // Reset per-frame input state
        window.resetFrameState();

        SDL_Delay(16); // ~60 FPS
    }

    logger.info("Exited main loop");
}

void Application::renderFrame()
{
    if (!renderer)
    {
        return;
    }

    renderer->clear(20, 30, 60, 255);

    if (activeState)
    {
        activeState->render();
    }

    captureDevScreenshot();

    renderer->present();
}

void Application::captureDevScreenshot()
{
    // Dev-only frame dump: RUNEHARBOR_SCREENSHOT=<path.bmp> grabs the composed
    // frame once RUNEHARBOR_SCREENSHOT_FRAME frames have been rendered, then
    // asks the window to close so the capture is reproducible from a script.
    static const char* const outPath = SDL_getenv("RUNEHARBOR_SCREENSHOT");
    if (!outPath)
    {
        return;
    }

    static const long targetFrame = []
    {
        const char* value = SDL_getenv("RUNEHARBOR_SCREENSHOT_FRAME");
        return value ? std::strtol(value, nullptr, 10) : 120L;
    }();

    if (++devFrameCounter_ < targetFrame)
    {
        return;
    }

    if (SDL_Surface* surface = SDL_RenderReadPixels(renderer->getSDLRenderer(), nullptr))
    {
        if (!SDL_SaveBMP(surface, outPath))
        {
            logger.error(std::format("Screenshot failed: {}", SDL_GetError()));
        }
        else
        {
            logger.info(std::format("Wrote screenshot {}", outPath));
        }
        SDL_DestroySurface(surface);
    }
    else
    {
        logger.error(std::format("SDL_RenderReadPixels failed: {}", SDL_GetError()));
    }

    SDL_Event quit = {};
    quit.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit);
}

void Application::updateViewport()
{
    SDL_Window* sdlWindow = window.getSDLWindow();
    if (!sdlWindow || !lineRenderer)
    {
        return;
    }

    int w = 0;
    int h = 0;
    SDL_GetWindowSize(sdlWindow, &w, &h);
    if (w <= 0 || h <= 0)
    {
        return;
    }

    if (w != viewportWidth || h != viewportHeight)
    {
        viewportWidth = w;
        viewportHeight = h;
        lineRenderer->setViewport(w, h);
        camera.setAspectRatio(static_cast<float>(w) / static_cast<float>(h));
    }
}

void Application::configureCameraForMap()
{
    if (!mapScene || !mapScene->isLoaded() || !gameWorld_)
    {
        return;
    }

    const auto& party = gameWorld_->party();
    const auto& config = gameWorld_->runtimeConfig();

    const float yawRad = party.yaw() * M_PI / 1024.0f;
    const float pitchRad = party.pitch() * M_PI / 1024.0f;
    const float dx = std::cos(yawRad);
    const float dy = std::sin(yawRad);
    const float dz = std::sin(pitchRad);

    const graphics::Vec3 eye = graphics::gameplayToRenderPosition(
        party.worldX(), party.worldY(), party.worldZ() + config.partyEyeLevel);
    const graphics::Vec3 lookTarget = graphics::gameplayToRenderPosition(
        party.worldX() + dx * 100.0f, party.worldY() + dy * 100.0f,
        party.worldZ() + config.partyEyeLevel + dz * 100.0f);

    camera.setTarget(lookTarget);
    camera.setPosition(eye);
}

void Application::wireUpMapTextures()
{
    if (!worldRenderer || !vfs || !renderer)
    {
        return;
    }

    clearMapTextureCache();

    worldRenderer->setSpriteFrameTable(&spriteFrameTable_);

    worldRenderer->setMonsterSpriteLookup(
        [this](uint16_t objectType) -> std::string
        {
            if (combatSystem_)
            {
                auto* def = combatSystem_->getMonsterDef(objectType);
                if (def)
                {
                    return def->picture;
                }
            }
            return "";
        });

    // Feed live CombatSystem monsters to the renderers each frame (RE: the live
    // actor table at 0x5FF06A). Without this only static map spawn markers show.
    worldRenderer->setLiveActorProvider(
        [this]() -> std::vector<graphics::LiveActor>
        {
            std::vector<graphics::LiveActor> actors;
            if (!combatSystem_)
            {
                return actors;
            }
            for (const auto& m : combatSystem_->getMonsters())
            {
                // Skip monsters that were never positioned (placeholder entries).
                if (m.x == 0.0f && m.y == 0.0f && m.z == 0.0f && !m.isAlive())
                {
                    continue;
                }
                graphics::LiveActor a;
                a.x = m.x;
                a.y = m.y;
                a.z = m.z;
                a.monsterId = static_cast<uint16_t>(m.monsterId);
                a.facingAngle = m.facingAngle;
                a.dead = (m.aiState == game::MonsterInstance::AIState::Dead);
                a.flying = false; // no fly flag on MonsterInstance yet
                actors.push_back(a);
            }
            return actors;
        });

    // Feed world-dropped items (loot piles) to the renderers so they appear in
    // the 3D world as small ground sprites.
    worldRenderer->setWorldItemProvider(
        [this]() -> std::vector<graphics::WorldItem>
        {
            std::vector<graphics::WorldItem> items;
            if (!gameWorld_ || !mapScene || !mapScene->isLoaded())
            {
                return items;
            }
            const std::string currentMap = gameWorld_->currentMap();
            const auto* spawned = gameWorld_->getSpawnedMapItems(currentMap);
            if (spawned == nullptr)
            {
                return items;
            }
            items.reserve(spawned->size());
            for (const auto& smi : *spawned)
            {
                graphics::WorldItem wi;
                wi.x = smi.x;
                wi.y = smi.y;
                wi.z = smi.z;
                wi.itemId = smi.itemType;
                items.push_back(wi);
            }
            return items;
        });
    worldRenderer->setWorldItemSpriteLookup(
        [this](int itemId) -> std::string
        {
            if (inventory_)
            {
                const auto* def = inventory_->getItemDef(itemId);
                if (def && !def->picFile.empty())
                {
                    return def->picFile;
                }
            }
            return {};
        });

    worldRenderer->setTextureLookup(
        [this](const std::string& name) -> SDL_Texture*
        {
            // Check cache first
            auto it = mapTextureCache.find(name);
            if (it != mapTextureCache.end())
            {
                return static_cast<SDL_Texture*>(it->second);
            }

            auto loadPaletteById = [this](int paletteId) -> graphics::Palette
            {
                if (!vfs)
                {
                    return graphics::Palette::createDefaultPalette();
                }

                const int resolvedPaletteId = (paletteId > 0) ? paletteId : 1;
                const std::array<std::string, 3> candidates = {
                    std::format("PAL{:03d}", resolvedPaletteId),
                    std::format("pal{:03d}", resolvedPaletteId),
                    std::format("pal{}", resolvedPaletteId),
                };

                for (const auto& palName : candidates)
                {
                    auto palData = vfs->readFile(palName);
                    if (!palData || palData->size() < 768)
                    {
                        continue;
                    }

                    std::vector<uint8_t> rgb;
                    if (palData->size() > 768)
                    {
                        rgb.assign(palData->end() - 768, palData->end());
                    }
                    else
                    {
                        rgb = *palData;
                    }

                    if (auto result = graphics::Palette::fromRGBData(rgb); result.has_value())
                    {
                        result->setColor(0, graphics::Palette::Color(0, 0, 0, 0));
                        return *result;
                    }
                }

                auto palette = graphics::Palette::createDefaultPalette();
                palette.setColor(0, graphics::Palette::Color(0, 0, 0, 0));
                return palette;
            };

            if (auto info = vfs->getImageInfo(name); info.has_value())
            {
                auto data = vfs->readFile(name);
                if (data)
                {
                    graphics::Palette palette = loadPaletteById(info->paletteId);
                    if (auto embeddedPalette = vfs->getImagePalette(name);
                        embeddedPalette && embeddedPalette->size() >= 768)
                    {
                        std::vector<uint8_t> rgb;
                        if (embeddedPalette->size() > 768)
                        {
                            rgb.assign(embeddedPalette->end() - 768, embeddedPalette->end());
                        }
                        else
                        {
                            rgb = *embeddedPalette;
                        }
                        if (auto result = graphics::Palette::fromRGBData(rgb); result.has_value())
                        {
                            palette = *result;
                            palette.setColor(0, graphics::Palette::Color(0, 0, 0, 0));
                        }
                    }

                    auto imageResult = graphics::Image::fromPalettedData(*data, info->width,
                                                                         info->height, palette);
                    if (imageResult.has_value() && *imageResult)
                    {
                        void* tex = renderer->createTexture(**imageResult);
                        if (tex)
                        {
                            mapTextureCache[name] = tex;
                            return static_cast<SDL_Texture*>(tex);
                        }
                    }
                }
            }

            if (auto spriteInfo = vfs->getSpriteInfo(name); spriteInfo.has_value())
            {
                auto data = vfs->readFile(name);
                if (data)
                {
                    graphics::Palette palette = loadPaletteById(spriteInfo->paletteId);
                    auto image = graphics::SpriteDecoder::decode(*data, palette, logger);
                    if (image)
                    {
                        void* tex = renderer->createTexture(*image);
                        if (tex)
                        {
                            mapTextureCache[name] = tex;
                            return static_cast<SDL_Texture*>(tex);
                        }
                    }
                }
            }

            return nullptr;
        });
}

void Application::clearMapTextureCache()
{
    if (renderer)
    {
        for (auto& [name, tex] : mapTextureCache)
        {
            renderer->destroyTexture(tex);
        }
    }
    mapTextureCache.clear();
}

void Application::setGameState(GameState state)
{
    gameState = state;
    if (state == GameState::TitleScreen)
    {
        startupMapName.clear();
        startupPreferOutdoor = false;
        pendingEntryDirection_ = 0;
        pendingArrivalOverride_.active = false;
        pendingTransition_ = {};
        if (sharedData)
        {
            sharedData->loadFromSave = false;
            sharedData->loadingScreenIndex = 0;
            sharedData->hasPendingEventRuntimeState = false;
            sharedData->pendingEventRuntimeState.clear();
        }
    }
    else if (state == GameState::Loading)
    {
        loadProgress.store(0.0f);
        loadProgressActive.store(true);
    }
    else
    {
        loadProgressActive.store(false);
    }
}

void Application::updateStateMachine()
{
    pollKeyboardState();
    updateStateContext();

    if (activeState)
    {
        auto next = activeState->update();
        if (next.has_value())
        {
            // Sync shared data back to Application fields
            if (sharedData)
            {
                startupMapName = sharedData->startupMapName;
                startupPreferOutdoor = sharedData->startupPreferOutdoor;
                quickStartReady = sharedData->quickStartReady;
                autoLoadMap = sharedData->autoLoadMap;

                if (sharedData->loadFromSave && gameWorld_)
                {
                    pendingEntryDirection_ = 0;
                    pendingArrivalOverride_.active = true;
                    pendingArrivalOverride_.x = gameWorld_->party().worldX();
                    pendingArrivalOverride_.y = gameWorld_->party().worldY();
                    pendingArrivalOverride_.z = gameWorld_->party().worldZ();
                    pendingArrivalOverride_.yaw = gameWorld_->party().yaw();

                    pendingTransition_.active = true;
                    pendingTransition_.sourceMap.clear();
                    pendingTransition_.targetMap = startupMapName;
                    pendingTransition_.exitDirection = 0;
                    pendingTransition_.transitionParam = 0;
                    pendingTransition_.hasArrivalOverride = true;
                    pendingTransition_.arrivalX = pendingArrivalOverride_.x;
                    pendingTransition_.arrivalY = pendingArrivalOverride_.y;
                    pendingTransition_.arrivalZ = pendingArrivalOverride_.z;
                    pendingTransition_.arrivalYaw = pendingArrivalOverride_.yaw;

                    sharedData->loadingScreenIndex = 0;
                    sharedData->loadFromSave = false;
                }
            }

            // When transitioning from CharacterCreation to Loading, commit party
            // and set up the Emerald Island spawn
            if (activeStateId == GameStateId::CharacterCreation && *next == GameStateId::Loading)
            {
                commitPartyToGameWorld();
                if (gameWorld_)
                {
                    auto& gp = gameWorld_->party();
                    gameWorld_->setCurrentMap(gp.currentMap());
                }
            }
            if (activeStateId == GameStateId::InGame && *next == GameStateId::Loading)
            {
                preserveCurrentMapState();
            }

            transitionTo(*next);
        }
    }

    commitKeyboardState();
}

void Application::buildIntroPlaylist()
{
    introPlaylist.clear();

    if (gameRoot.empty())
    {
        logger.warning("Intro playlist: game root is empty, no intro videos will play");
        return;
    }

    const std::filesystem::path animsPath = gameRoot / "Anims";
    const std::filesystem::path magicVid = animsPath / "Magic7.vid";
    const std::filesystem::path mightVid = animsPath / "Might7.vid";

    media::VidManifest manifest;
    std::filesystem::path vidPath;
    bool loaded = false;
    if (std::filesystem::exists(magicVid) && manifest.load(magicVid))
    {
        vidPath = magicVid;
        loaded = true;
    }
    else if (std::filesystem::exists(mightVid) && manifest.load(mightVid))
    {
        vidPath = mightVid;
        loaded = true;
    }

    if (!loaded)
    {
        logger.warning(std::format("Intro playlist: no VID manifest under {}, using placeholder",
                                   animsPath.string()));
        introPlaylist.push_back({"Intro", 2500});
        return;
    }

    logger.info(std::format("Intro playlist: loaded {} with {} clips", vidPath.string(),
                            manifest.clips().size()));

    // Load the VID archive into the video player
    if (videoPlayer && !vidPath.empty())
    {
        if (!videoPlayer->loadArchive(vidPath))
        {
            logger.warning(
                std::format("Intro playlist: failed to open VID archive {}", vidPath.string()));
        }
    }

    auto clipMatches = [](const std::string& name, const std::string& target)
    {
        if (name.size() != target.size())
        {
            return false;
        }
        for (size_t i = 0; i < name.size(); i++)
        {
            if (std::tolower(static_cast<unsigned char>(name[i])) !=
                std::tolower(static_cast<unsigned char>(target[i])))
            {
                return false;
            }
        }
        return true;
    };

    auto isLogoClip = [&](const std::string& name)
    {
        return clipMatches(name, "3DOLOGO.SMK") || clipMatches(name, "JVC.BIK") ||
               clipMatches(name, "NEW WORLD LOGO.BIK");
    };

    // Build a filtered intro list (logos + intro, unless noLogo is enabled)
    const std::vector<std::string> preferred = {
        "3DOLOGO.SMK", "JVC.BIK", "NEW WORLD LOGO.BIK", "INTRO.BIK", "INTRO POST.BIK",
    };

    for (const auto& want : preferred)
    {
        if (bootConfig_.noLogo && isLogoClip(want))
        {
            continue;
        }
        for (const auto& clip : manifest.clips())
        {
            if (clipMatches(clip.name, want))
            {
                introPlaylist.push_back({clip.name, 3000});
                break;
            }
        }
    }

    if (introPlaylist.empty())
    {
        for (const auto& clip : manifest.clips())
        {
            if (bootConfig_.noLogo && isLogoClip(clip.name))
            {
                continue;
            }
            introPlaylist.push_back({clip.name, 2500});
            if (introPlaylist.size() >= 3)
            {
                break;
            }
        }
    }

    std::string names;
    for (const auto& clip : introPlaylist)
    {
        if (!names.empty())
        {
            names += ", ";
        }
        names += clip.name;
    }
    logger.info(std::format("Intro playlist: {} clips [{}]", introPlaylist.size(), names));
}

bool Application::loadUiAssets()
{
    if (uiAssetsLoaded || !renderer || !vfs)
    {
        return uiAssetsLoaded;
    }

    unloadUiAssets();

    loadPcxTexture({"Title.pcx", "TITLE.PCX", "MM6TITLE.PCX"}, "Title", titleBackground,
                   titleBackgroundWidth, titleBackgroundHeight);
    loadPcxTexture({"makeme.pcx", "MAKEME.PCX", "Create.pcx", "CREATE.PCX"}, "Create",
                   createBackground, createBackgroundWidth, createBackgroundHeight);

    // Extract palette for paletted textures with paletteId=0 ("use screen palette").
    // In the original VGA engine, the background PCX set the hardware palette.
    // Try Title.pcx first (title screen buttons need its palette), then makeme.pcx.
    // Both may be 24-bit (3 planes) but still embed a 768-byte VGA palette at the end.
    for (const auto& pcxName :
         {"Title.pcx", "TITLE.PCX", "makeme.pcx", "MAKEME.PCX", "Create.pcx", "CREATE.PCX"})
    {
        auto pcxData = vfs->readFile(pcxName);
        if (!pcxData.has_value())
        {
            continue;
        }
        auto pcx = formats::decodePCX(*pcxData, logger);
        if (pcx.has_value() && !pcx->is24Bit())
        {
            // 8-bit paletted PCX: use the decoded palette directly
            screenPaletteRGB.resize(768);
            for (int i = 0; i < 256; i++)
            {
                auto c = pcx->palette.getColor(static_cast<uint8_t>(i));
                screenPaletteRGB[i * 3] = c.r;
                screenPaletteRGB[i * 3 + 1] = c.g;
                screenPaletteRGB[i * 3 + 2] = c.b;
            }
            logger.info(std::format("Extracted screen palette from {} (8-bit paletted)", pcxName));
            break;
        }
        // 24-bit PCX: check for embedded VGA palette at end of raw data (0x0C marker + 768 bytes)
        if (pcxData->size() >= 769 && (*pcxData)[pcxData->size() - 769] == 0x0C)
        {
            screenPaletteRGB.assign(pcxData->end() - 768, pcxData->end());
            logger.info(std::format(
                "Extracted screen palette from {} (24-bit, embedded VGA palette)", pcxName));
            break;
        }
    }

    // Load bitmap fonts (FONTPAL + .fnt files from ICONS.LOD)
    {
        // FONTPAL is stored as a 1x1 image whose trailing 768 bytes are the
        // actual font palette, not as a standalone 768-byte file.
        auto fontPalData = vfs->getImagePalette("FONTPAL");
        if (!fontPalData.has_value())
            fontPalData = vfs->getImagePalette("fontpal");
        // Keep compatibility with archives that expose FONTPAL as raw bytes.
        if (!fontPalData.has_value())
            fontPalData = vfs->readFile("FONTPAL");
        if (!fontPalData.has_value())
            fontPalData = vfs->readFile("fontpal");
        if (fontPalData.has_value() && fontPalData->size() >= 768)
        {
            fontPalRGB_.assign(fontPalData->begin(), fontPalData->begin() + 768);
            logger.info("Loaded FONTPAL palette");
        }
        else
        {
            logger.warning("FONTPAL not found; bitmap fonts will be unavailable");
        }

        if (!fontPalRGB_.empty())
        {
            SDL_Renderer* sdlR = renderer->getSDLRenderer();
            struct FontEntry
            {
                const char* name;
                std::unique_ptr<graphics::BitmapFont>& target;
            };
            FontEntry fonts[] = {
                {"create.fnt", createFont_},
                {"cchar.fnt", ccharFont_},
                {"arrus.fnt", arrusFont_},
                {"smallnum.fnt", smallnumFont_},
            };
            for (auto& fe : fonts)
            {
                auto fntData = vfs->readFile(fe.name);
                if (!fntData.has_value())
                {
                    // Try uppercase
                    std::string upper = fe.name;
                    for (auto& c : upper)
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    fntData = vfs->readFile(upper);
                }
                if (fntData.has_value())
                {
                    fe.target = std::make_unique<graphics::BitmapFont>();
                    if (fe.target->load(*fntData, fontPalRGB_))
                    {
                        if (sdlR && fe.target->createAtlas(sdlR))
                        {
                            logger.info(std::format("Loaded bitmap font: {} (height={})", fe.name,
                                                    fe.target->height()));
                        }
                        else
                        {
                            logger.warning(
                                std::format("Failed to create atlas for font: {}", fe.name));
                        }
                    }
                    else
                    {
                        logger.warning(std::format("Failed to parse font: {}", fe.name));
                        fe.target.reset();
                    }
                }
                else
                {
                    logger.warning(std::format("Font file not found: {}", fe.name));
                }
            }
        }
    }

    loadPcxSequence("loading", loadingFrames, loadingFrameWidths, loadingFrameHeights,
                    &loadingFrameNumbers);
    loadPcxTexture({"lsave640.pcx", "LSave640.pcx", "LSAVE640.PCX", "loading.pcx", "Loading.pcx",
                    "LOADING.PCX"},
                   "Loading", loadingBackground, loadingBackgroundWidth, loadingBackgroundHeight);

    // Load per-button hover textures from ICONS.LOD
    // title_* are the correct title screen overlays (~85x30)
    // New1/Load1/Quit1 are in-game pause menu assets (214x40) — used as fallbacks
    const std::vector<std::string> hoverNames[] = {
        {"title_new", "New1"},   // NEW GAME
        {"title_load", "Load1"}, // LOAD GAME
        {"title_cred", "Cred1"}, // CREDITS
        {"title_exit", "Quit1"}, // EXIT GAME
    };
    for (int i = 0; i < kTitleButtonCount; i++)
    {
        loadPcxTexture(hoverNames[i], std::string("TitleBtn ") + std::to_string(i),
                       titleButtonHoverTextures[i], titleButtonHoverWidths[i],
                       titleButtonHoverHeights[i]);
    }

    // Load Inventory frame
    loadPcxTexture({"fr_inven.pcx", "FR_inven", "FR_inven.pcx", "FR_INVEN.PCX"}, "InventoryFrame",
                   inventoryBackground, inventoryBackgroundWidth, inventoryBackgroundHeight);

    // Load portrait textures (pc01 through pc20) from ICONS.LOD
    for (int i = 0; i < kPortraitCount; i++)
    {
        std::string num = std::format("{:02d}", i + 1);
        std::vector<std::string> portraitCandidates = {
            "pc" + num,
            "pc" + num + "01",
            "pc" + num + "-01",
        };
        loadPcxTexture(portraitCandidates, std::format("Portrait {}", i + 1), portraitTextures[i],
                       portraitWidths[i], portraitHeights[i]);
    }

    // Load character creation overlay textures from ICONS.LOD
    loadPcxTexture({"MAKESKY", "makesky"}, "CreateSkyHeader", ccSkyHeader_.tex, ccSkyHeader_.w,
                   ccSkyHeader_.h);
    loadPcxTexture({"MAKETOP", "maketop"}, "CreateTitleHeader", ccTitleHeader_.tex,
                   ccTitleHeader_.w, ccTitleHeader_.h);
    loadPcxTexture({"FACEMASK", "facemask"}, "FaceMask", ccFaceMask_.tex, ccFaceMask_.w,
                   ccFaceMask_.h);
    loadPcxTexture({"BUTTMAKE", "buttmake"}, "OkBtn", ccOkButton_.tex, ccOkButton_.w,
                   ccOkButton_.h);
    loadPcxTexture({"buttmake2", "BUTTMAKE2"}, "ClearBtn", ccClearButton_.tex, ccClearButton_.w,
                   ccClearButton_.h);
    loadPcxTexture({"MAKEMINU", "makeminu"}, "MinusBtn", ccMinusButton_.tex, ccMinusButton_.w,
                   ccMinusButton_.h);
    loadPcxTexture({"MAKEPLUS", "makeplus"}, "PlusBtn", ccPlusButton_.tex, ccPlusButton_.w,
                   ccPlusButton_.h);
    loadPcxTexture({"presleft", "PRESLEFT"}, "LeftArrow", ccLeftArrow_.tex, ccLeftArrow_.w,
                   ccLeftArrow_.h);
    loadPcxTexture({"presrigh", "PRESRIGH"}, "RightArrow", ccRightArrow_.tex, ccRightArrow_.w,
                   ccRightArrow_.h);

    const char* classIconNames[] = {"IC_Knight", "IC_Thief", "IC_monk",  "IC_PALAD", "IC_ARCH",
                                    "IC_Ranger", "IC_CLER",  "IC_DRUID", "IC_SORC"};
    for (int i = 0; i < kClassIconCount; i++)
    {
        std::string lower = classIconNames[i];
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        loadPcxTexture({classIconNames[i], lower}, std::string("ClassIcon_") + classIconNames[i],
                       ccClassIcons_[i].tex, ccClassIcons_[i].w, ccClassIcons_[i].h);
    }

    // Load credits from CREDITS.TXT in events.lod
    if (creditsState && vfs->fileExists("CREDITS.TXT"))
    {
        auto creditsData = vfs->readFile("CREDITS.TXT");
        if (creditsData.has_value())
        {
            formats::CreditsParser creditsParser(logger);
            if (creditsParser.parse(*creditsData))
            {
                creditsState->setCreditsSections(creditsParser.getCreditsSections());
                logger.info(std::format("Parsed {} credits sections",
                                        creditsParser.getCreditsSections().size()));
            }
        }
    }

    // Wire textures to state objects
    if (titleState)
    {
        titleState->setBackground(titleBackground, titleBackgroundWidth, titleBackgroundHeight);
        for (int i = 0; i < kTitleButtonCount; i++)
        {
            titleState->setButtonTextures(i, titleButtonHoverTextures[i], titleButtonHoverWidths[i],
                                          titleButtonHoverHeights[i]);
        }
    }
    if (charCreationState)
    {
        charCreationState->setBackground(createBackground, createBackgroundWidth,
                                         createBackgroundHeight);
        charCreationState->setFallbackBackground(titleBackground, titleBackgroundWidth,
                                                 titleBackgroundHeight);
        for (int i = 0; i < kPortraitCount; i++)
        {
            charCreationState->setPortraitTexture(i, portraitTextures[i], portraitWidths[i],
                                                  portraitHeights[i]);
        }
        charCreationState->setSkyHeader(ccSkyHeader_.tex, ccSkyHeader_.w, ccSkyHeader_.h);
        charCreationState->setTitleHeader(ccTitleHeader_.tex, ccTitleHeader_.w, ccTitleHeader_.h);
        charCreationState->setFaceMask(ccFaceMask_.tex, ccFaceMask_.w, ccFaceMask_.h);
        charCreationState->setOkButton(ccOkButton_.tex, ccOkButton_.w, ccOkButton_.h);
        charCreationState->setClearButton(ccClearButton_.tex, ccClearButton_.w, ccClearButton_.h);
        charCreationState->setMinusButton(ccMinusButton_.tex, ccMinusButton_.w, ccMinusButton_.h);
        charCreationState->setPlusButton(ccPlusButton_.tex, ccPlusButton_.w, ccPlusButton_.h);
        charCreationState->setLeftArrow(ccLeftArrow_.tex, ccLeftArrow_.w, ccLeftArrow_.h);
        charCreationState->setRightArrow(ccRightArrow_.tex, ccRightArrow_.w, ccRightArrow_.h);
        for (int i = 0; i < kClassIconCount; i++)
        {
            charCreationState->setClassIcon(i, ccClassIcons_[i].tex, ccClassIcons_[i].w,
                                            ccClassIcons_[i].h);
        }
    }
    if (loadingState)
    {
        loadingState->setBackground(loadingBackground, loadingBackgroundWidth,
                                    loadingBackgroundHeight);
        loadingState->setFallbackBackground(titleBackground, titleBackgroundWidth,
                                            titleBackgroundHeight);
        loadingState->setAnimationFrames(&loadingFrames, &loadingFrameWidths, &loadingFrameHeights,
                                         &loadingFrameNumbers);
    }
    if (loadGameState)
    {
        // Try to load lsave640.pcx as background for the load game screen
        void* loadBg = nullptr;
        int loadBgW = 0, loadBgH = 0;
        if (loadPcxTexture({"lsave640.pcx", "LSave640.pcx", "LSAVE640.PCX"}, "LoadGame", loadBg,
                           loadBgW, loadBgH))
        {
            loadGameState->setBackground(loadBg, loadBgW, loadBgH);
        }
    }
    if (inGameState)
    {
        inGameState->setInventoryBackground(inventoryBackground, inventoryBackgroundWidth,
                                            inventoryBackgroundHeight);

        inGameState->setTextureLookup(
            [this](const std::string& name, int& w, int& h) -> void*
            {
                void* tex = nullptr;
                if (loadPcxTexture(
                        {name, name + ".pcx", name + ".PCX", name + "01", name + "01.pcx"}, "Item",
                        tex, w, h))
                {
                    return tex;
                }
                return nullptr;
            });
    }

    uiAssetsLoaded = true;
    return true;
}

void Application::unloadUiAssets()
{
    if (renderer)
    {
        renderer->destroyTexture(titleBackground);
        renderer->destroyTexture(createBackground);
        renderer->destroyTexture(loadingBackground);
        for (int i = 0; i < kTitleButtonCount; i++)
        {
            renderer->destroyTexture(titleButtonHoverTextures[i]);
        }
        for (int i = 0; i < kPortraitCount; i++)
        {
            renderer->destroyTexture(portraitTextures[i]);
        }
        for (auto* tex : loadingFrames)
        {
            renderer->destroyTexture(tex);
        }
        renderer->destroyTexture(ccFaceMask_.tex);
        renderer->destroyTexture(ccSkyHeader_.tex);
        renderer->destroyTexture(ccTitleHeader_.tex);
        renderer->destroyTexture(ccOkButton_.tex);
        renderer->destroyTexture(ccClearButton_.tex);
        renderer->destroyTexture(ccMinusButton_.tex);
        renderer->destroyTexture(ccPlusButton_.tex);
        renderer->destroyTexture(ccLeftArrow_.tex);
        renderer->destroyTexture(ccRightArrow_.tex);
        for (int i = 0; i < kClassIconCount; i++)
        {
            renderer->destroyTexture(ccClassIcons_[i].tex);
        }
    }

    titleBackground = nullptr;
    createBackground = nullptr;
    loadingBackground = nullptr;
    titleBackgroundWidth = 0;
    titleBackgroundHeight = 0;
    createBackgroundWidth = 0;
    createBackgroundHeight = 0;
    loadingBackgroundWidth = 0;
    loadingBackgroundHeight = 0;
    for (int i = 0; i < kTitleButtonCount; i++)
    {
        titleButtonHoverTextures[i] = nullptr;
        titleButtonHoverWidths[i] = 0;
        titleButtonHoverHeights[i] = 0;
    }
    for (int i = 0; i < kPortraitCount; i++)
    {
        portraitTextures[i] = nullptr;
        portraitWidths[i] = 0;
        portraitHeights[i] = 0;
    }
    loadingFrames.clear();
    loadingFrameWidths.clear();
    loadingFrameHeights.clear();
    loadingFrameNumbers.clear();
    ccFaceMask_ = {};
    ccSkyHeader_ = {};
    ccTitleHeader_ = {};
    ccOkButton_ = {};
    ccClearButton_ = {};
    ccMinusButton_ = {};
    ccPlusButton_ = {};
    ccLeftArrow_ = {};
    ccRightArrow_ = {};
    for (int i = 0; i < kClassIconCount; i++)
    {
        ccClassIcons_[i] = {};
    }
    createFont_.reset();
    ccharFont_.reset();
    arrusFont_.reset();
    smallnumFont_.reset();
    fontPalRGB_.clear();
    uiAssetsLoaded = false;
}

bool Application::loadPcxTexture(const std::vector<std::string>& candidates,
                                 const std::string& label, void*& textureHandle, int& width,
                                 int& height)
{
    if (!renderer || !vfs)
    {
        return false;
    }

    for (const auto& name : candidates)
    {
        auto data = vfs->readFile(name);
        if (!data.has_value())
        {
            continue;
        }

        auto pcx = formats::decodePCX(*data, logger);
        if (pcx.has_value())
        {
            // Successfully decoded as PCX
            {
                auto imageResult =
                    pcx->is24Bit()
                        ? graphics::Image::fromRGBAData(pcx->rgbaPixels, pcx->width, pcx->height)
                        : graphics::Image::fromPalettedData(pcx->indices, pcx->width, pcx->height,
                                                            pcx->palette);
                if (imageResult.has_value() && *imageResult)
                {
                    void* tex = renderer->createTexture(**imageResult);
                    if (tex)
                    {
                        renderer->destroyTexture(textureHandle);
                        textureHandle = tex;
                        width = static_cast<int>(pcx->width);
                        height = static_cast<int>(pcx->height);
                        logger.info(std::format("Loaded UI texture '{}': {} ({}x{})", label, name,
                                                width, height));
                        return true;
                    }
                }
                else if (!imageResult.has_value())
                {
                    logger.warning(std::format("Failed to convert PCX '{}': {}", name,
                                               imageResult.error().message));
                }
            }
            continue;
        }

        // Try loading as sprite
        logger.debug(std::format("PCX decode failed for '{}', trying sprite", name));
        formats::SpriteParser spriteParser(logger);
        formats::Sprite sprite = spriteParser.parse(*data);

        if (sprite.frames.empty())
        {
            logger.debug(std::format("Sprite parsing failed for '{}'", name));
            continue;
        }

        auto& frame = sprite.frames[0];
        auto imageResult = graphics::Image::fromPalettedData(frame.data, frame.width, frame.height,
                                                             sprite.palette);
        if (imageResult.has_value() && *imageResult)
        {
            void* tex = renderer->createTexture(**imageResult);
            if (tex)
            {
                renderer->destroyTexture(textureHandle);
                textureHandle = tex;
                width = static_cast<int>(frame.width);
                height = static_cast<int>(frame.height);
                logger.info(std::format("Loaded UI texture '{}' as sprite: {} ({}x{})", label, name,
                                        width, height));
                return true;
            }
        }
        else if (!imageResult.has_value())
        {
            logger.warning(std::format("Failed to convert sprite frame to image '{}': {}", name,
                                       imageResult.error().message));
        }
    }

    // Third fallback: try raw paletted data from image archives
    for (const auto& name : candidates)
    {
        auto imgInfo = vfs->getImageInfo(name);
        if (!imgInfo.has_value() || imgInfo->width == 0 || imgInfo->height == 0)
        {
            continue;
        }

        auto pixelData = vfs->readFile(name);
        if (!pixelData.has_value())
        {
            continue;
        }

        // Load palette: prefer embedded palette from the LOD entry itself,
        // then try screen palette, then PAL### from BITMAPS.LOD.
        std::optional<std::vector<uint8_t>> palData = vfs->getImagePalette(name);
        if (!palData.has_value() && imgInfo->paletteId == 0 && !screenPaletteRGB.empty())
        {
            palData = screenPaletteRGB;
        }
        if (!palData.has_value())
        {
            int palId = imgInfo->paletteId > 0 ? imgInfo->paletteId : 1;
            std::string palName = std::format("PAL{:03d}", palId);
            palData = vfs->readFile(palName);
            if (!palData.has_value() && palId != 1)
            {
                palData = vfs->readFile("PAL001");
            }
        }
        if (!palData.has_value())
        {
            logger.warning(
                std::format("No palette found for '{}' (paletteId={})", name, imgInfo->paletteId));
            continue;
        }

        auto paletteResult = graphics::Palette::fromRGBData(*palData);
        if (!paletteResult.has_value())
        {
            logger.warning(std::format("Failed to convert paletted image '{}': {}", name,
                                       paletteResult.error().message));
            continue;
        }
        auto palette = *paletteResult;
        // Index 0 is transparent
        palette.setColor(0, graphics::Palette::Color(0, 0, 0, 0));

        auto imageResult =
            graphics::Image::fromPalettedData(*pixelData, imgInfo->width, imgInfo->height, palette);
        if (imageResult.has_value() && *imageResult)
        {
            void* tex = renderer->createTexture(**imageResult);
            if (tex)
            {
                renderer->destroyTexture(textureHandle);
                textureHandle = tex;
                width = static_cast<int>(imgInfo->width);
                height = static_cast<int>(imgInfo->height);
                logger.info(std::format("Loaded UI texture '{}' as paletted: {} ({}x{})", label,
                                        name, width, height));
                return true;
            }
        }
    }

    logger.warning(std::format("Failed to load texture '{}' from any candidate", label));
    return false;
}

bool Application::loadPcxSequence(const std::string& prefix, std::vector<void*>& textures,
                                  std::vector<int>& widths, std::vector<int>& heights,
                                  std::vector<int>* frameNumbers)
{
    textures.clear();
    widths.clear();
    heights.clear();
    if (frameNumbers)
    {
        frameNumbers->clear();
    }

    if (!renderer || !vfs)
    {
        return false;
    }

    struct FrameEntry
    {
        int index = 0;
        std::string name;
    };

    std::vector<FrameEntry> entries;
    const auto allFiles = vfs->listAllFiles();
    const std::string prefixLower = toLower(prefix);

    for (const auto& file : allFiles)
    {
        std::string fileLower = toLower(file);
        const size_t dot = fileLower.find_last_of('.');
        if (dot == std::string::npos)
        {
            continue;
        }
        if (fileLower.substr(dot) != ".pcx")
        {
            continue;
        }
        if (fileLower.rfind(prefixLower, 0) != 0)
        {
            continue;
        }

        std::string suffix = fileLower.substr(prefixLower.size(), dot - prefixLower.size());
        if (suffix.empty())
        {
            continue;
        }

        int value = 0;
        bool numeric = true;
        for (char c : suffix)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                numeric = false;
                break;
            }
            value = value * 10 + (c - '0');
        }
        if (!numeric)
        {
            continue;
        }

        entries.push_back({value, file});
    }

    if (entries.empty())
    {
        return false;
    }

    std::sort(entries.begin(), entries.end(),
              [](const FrameEntry& a, const FrameEntry& b)
              {
                  if (a.index != b.index)
                  {
                      return a.index < b.index;
                  }
                  return a.name < b.name;
              });

    for (const auto& entry : entries)
    {
        auto data = vfs->readFile(entry.name);
        if (!data.has_value())
        {
            continue;
        }

        auto pcx = formats::decodePCX(*data, logger);
        if (!pcx.has_value())
        {
            continue;
        }

        {
            auto imageResult =
                pcx->is24Bit()
                    ? graphics::Image::fromRGBAData(pcx->rgbaPixels, pcx->width, pcx->height)
                    : graphics::Image::fromPalettedData(pcx->indices, pcx->width, pcx->height,
                                                        pcx->palette);
            if (!imageResult.has_value())
            {
                logger.warning(std::format("Failed to convert PCX '{}': {}", entry.name,
                                           imageResult.error().message));
                continue;
            }
            if (!*imageResult)
            {
                continue;
            }

            void* tex = renderer->createTexture(**imageResult);
            if (!tex)
            {
                continue;
            }

            textures.push_back(tex);
            widths.push_back(static_cast<int>(pcx->width));
            heights.push_back(static_cast<int>(pcx->height));
            if (frameNumbers)
            {
                frameNumbers->push_back(entry.index);
            }
        }
    }

    if (!textures.empty())
    {
        logger.info(std::format("Loaded {} '{}' loading frames", textures.size(), prefix));
    }

    return !textures.empty();
}

void Application::updateLoadProgress(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    float current = loadProgress.load();
    while (value > current && !loadProgress.compare_exchange_weak(current, value))
    {
    }
    loadProgressActive.store(true);
}

void Application::startLoadingTask()
{
    logger.info(std::format("startLoadingTask: map='{}', preferOutdoor={}", startupMapName,
                            startupPreferOutdoor));

    if (loadingTaskActive.load())
    {
        logger.warning("startLoadingTask: already active, skipping");
        return;
    }

    LoadRequest request;
    request.mapName = startupMapName;
    request.preferOutdoor = startupPreferOutdoor;

    loadProgress.store(0.0f);
    loadProgressActive.store(true);
    loadingTaskDone.store(false);
    loadingTaskActive.store(true);

    {
        std::lock_guard<std::mutex> lock(loadingTaskMutex);
        loadingTaskScene.reset();
        loadingTaskError.clear();
        loadingTaskSuccess = false;
    }

    if (loadingThread.joinable())
    {
        loadingThread.join();
    }

    loadingThread = std::thread([this, request]() mutable { runLoadingTask(std::move(request)); });
}

void Application::finalizeLoadingTask()
{
    if (!loadingTaskActive.load())
    {
        return;
    }

    if (loadingThread.joinable())
    {
        loadingThread.join();
    }

    loadingTaskActive.store(false);
    loadingTaskDone.store(false);

    std::unique_ptr<MapScene> loadedScene;
    bool success = false;
    std::string error;

    {
        std::lock_guard<std::mutex> lock(loadingTaskMutex);
        loadedScene = std::move(loadingTaskScene);
        success = loadingTaskSuccess;
        error = loadingTaskError;
    }

    if (success && loadedScene)
    {
        logger.info("finalizeLoadingTask: success, transitioning to InGame");
        mapScene = std::move(loadedScene);
        // Keep state context in sync before the first InGame render to avoid stale pointers.
        updateStateContext();
        mapLoaded = true;
        wireUpMapTextures();
        setGameState(GameState::InGame);

        // Sync loaded map name to GameWorld
        if (gameWorld_ && mapScene && !mapScene->getName().empty())
        {
            gameWorld_->setCurrentMap(mapScene->getName());
            const auto& runtimeConfig = gameWorld_->runtimeConfig();

            if (runtimeConfig.noDecorations)
            {
                mapScene->mutableBLVData().decorations.clear();
                mapScene->mutableODMData().buildings.clear();
            }

            const std::string& mapName = mapScene->getName();
            restoreCurrentMapState(mapName);
            loadMapEventScripts(mapName);
            if (eventEngine_)
            {
                // Restore runtime state (fired one-shot set + timers) BEFORE
                // onMapLoaded(), so already-fired OnMapLoad/OnMapEnter events
                // are skipped instead of re-granting their rewards.
                if (sharedData && sharedData->hasPendingEventRuntimeState)
                {
                    if (!eventEngine_->deserializeRuntimeState(
                            sharedData->pendingEventRuntimeState))
                    {
                        logger.warning("Failed to restore event runtime state from save payload");
                    }
                    sharedData->hasPendingEventRuntimeState = false;
                    sharedData->pendingEventRuntimeState.clear();
                }
                eventEngine_->onMapLoaded();
            }

            bool shouldGenerateContent = false;
            if (!gameWorld_->hasGeneratedContent(mapName))
            {
                shouldGenerateContent = true;
            }
            else if (const auto* generated = gameWorld_->getGeneratedContent(mapName);
                     generated != nullptr)
            {
                const int respawnDays = std::max(0, generated->respawnDays);
                if (respawnDays > 0)
                {
                    const int64_t nowTicks = gameWorld_->calendar().totalTicks;
                    const int64_t elapsedTicks =
                        std::max<int64_t>(0, nowTicks - generated->generatedAtTicks);
                    const int64_t respawnTicks =
                        static_cast<int64_t>(respawnDays) * game::GameCalendar::kTicksPerDay;
                    if (elapsedTicks >= respawnTicks)
                    {
                        shouldGenerateContent = true;
                    }
                }
            }

            if (shouldGenerateContent && contentGenerator_)
            {
                game::GenerationConfig generationConfig;
                generationConfig.mapDifficulty =
                    resolveMapDifficulty(mapName, startupPreferOutdoor ? 4 : 5);
                generationConfig.maxMonsters =
                    runtimeConfig.noMonsters ? 0 : (startupPreferOutdoor ? 96 : 64);
                generationConfig.maxChests = startupPreferOutdoor ? 24 : 18;

                game::GeneratedMapContent generated = contentGenerator_->generateForMap(
                    mapName, mapScene->getBLVData().spawns, mapScene->getODMData().spawns,
                    generationConfig);
                generated.generatedAtTicks = gameWorld_->calendar().totalTicks;
                generated.respawnDays = resolveMapRespawnDays(mapName, 0);
                gameWorld_->setGeneratedContent(mapName, std::move(generated));
            }

            if (combatSystem_)
            {
                combatSystem_->clearMonsters();
                if (const auto* generated = gameWorld_->getGeneratedContent(mapName);
                    generated != nullptr)
                {
                    for (const auto& monster : generated->monsters)
                    {
                        combatSystem_->spawnMonster(monster.monsterId, monster.x, monster.y,
                                                    monster.z, monster.group);
                    }
                }
                combatSystem_->setInCombat(false);
            }

            applyMapEntryPoint();
            configureCameraForMap();
            pendingTransition_.active = false;
            if (sharedData)
            {
                sharedData->loadingScreenIndex = 0;
            }
        }
    }
    else
    {
        logger.error(std::format("finalizeLoadingTask: failed, error='{}', hasScene={}", error,
                                 loadedScene != nullptr));
        pendingTransition_ = {};
        if (sharedData)
        {
            sharedData->loadingScreenIndex = 0;
        }
        setGameState(GameState::TitleScreen);
    }
}

void Application::runLoadingTask(LoadRequest request)
{
    logger.info(std::format("runLoadingTask: map='{}', preferOutdoor={}", request.mapName,
                            request.preferOutdoor));

    std::unique_ptr<MapScene> scene;
    std::string error;
    bool loaded = false;

    updateLoadProgress(0.05f);

    auto tryLoad = [&](const std::string& mapName, bool outdoor) -> bool
    { return loadMapIntoScene(mapName, outdoor, scene, error); };

    if (!request.mapName.empty())
    {
        // If the name already has an extension, only try the matching format
        std::string ext = getLowerExtension(request.mapName);
        bool hasOdm = (ext == ".odm");
        bool hasBlv = (ext == ".blv");

        if (hasOdm)
        {
            loaded = tryLoad(request.mapName, true);
        }
        else if (hasBlv)
        {
            loaded = tryLoad(request.mapName, false);
        }
        else if (request.preferOutdoor)
        {
            loaded = tryLoad(request.mapName, true);
            if (!loaded)
            {
                loaded = tryLoad(request.mapName, false);
            }
        }
        else
        {
            loaded = tryLoad(request.mapName, false);
            if (!loaded)
            {
                loaded = tryLoad(request.mapName, true);
            }
        }
    }
    else
    {
        if (request.preferOutdoor)
        {
            if (auto name = pickFirstMap(true))
            {
                loaded = tryLoad(*name, true);
            }
            if (!loaded)
            {
                if (auto name = pickFirstMap(false))
                {
                    loaded = tryLoad(*name, false);
                }
            }
        }
        else
        {
            if (auto name = pickFirstMap(false))
            {
                loaded = tryLoad(*name, false);
            }
            if (!loaded)
            {
                if (auto name = pickFirstMap(true))
                {
                    loaded = tryLoad(*name, true);
                }
            }
        }
    }

    if (loaded)
    {
        updateLoadProgress(1.0f);
    }

    logger.info(std::format("runLoadingTask: finished, loaded={}, error='{}'", loaded, error));

    {
        std::lock_guard<std::mutex> lock(loadingTaskMutex);
        loadingTaskScene = std::move(scene);
        loadingTaskSuccess = loaded;
        loadingTaskError = loaded ? "" : error;
    }

    loadingTaskDone.store(true);
}

bool Application::loadMapIntoScene(const std::string& mapName, bool outdoor,
                                   std::unique_ptr<MapScene>& outScene, std::string& error)
{
    if (!gameDataLoaded || !vfs)
    {
        error = "Game data not loaded";
        return false;
    }

    if (mapName.empty())
    {
        error = "Empty map name";
        return false;
    }

    const std::string expectedExt = outdoor ? ".odm" : ".blv";
    std::string resolvedName = mapName;
    std::string ext = getLowerExtension(mapName);
    if (!ext.empty() && ext != expectedExt)
    {
        error = "Unsupported map extension";
        return false;
    }

    if (ext.empty())
    {
        resolvedName = mapName + expectedExt;
    }

    if (!vfs->fileExists(resolvedName))
    {
        error = "Map not found";
        return false;
    }

    auto data = vfs->readFile(resolvedName);
    if (!data.has_value())
    {
        error = "Failed to read map data";
        return false;
    }

    updateLoadProgress(0.1f);

    auto scene = std::make_unique<MapScene>(logger);
    scene->setTileTable(&tileTable_);
    auto progress = [this](float value) { updateLoadProgress(0.1f + value * 0.85f); };
    bool ok = false;
    if (outdoor)
    {
        ok = scene->loadODM(resolvedName, *data, progress);
    }
    else
    {
        ok = scene->loadBLV(resolvedName, *data, progress);
    }

    if (!ok)
    {
        error = "Failed to parse map";
        return false;
    }

    outScene = std::move(scene);
    return true;
}

std::optional<std::string> Application::pickFirstMap(bool outdoor)
{
    if (!gameDataLoaded || !vfs)
    {
        return std::nullopt;
    }

    const std::string expectedExt = outdoor ? ".odm" : ".blv";
    std::vector<std::string> files;
    auto allFiles = vfs->listAllFiles();

    for (const auto& name : allFiles)
    {
        if (getLowerExtension(name) == expectedExt)
        {
            files.push_back(name);
        }
    }

    if (files.empty())
    {
        return std::nullopt;
    }

    std::sort(files.begin(), files.end());
    std::string preferred = outdoor ? "out01.odm" : "d01.blv";
    for (const auto& name : files)
    {
        if (toLower(name) == preferred)
        {
            return name;
        }
    }

    return files.front();
}

void Application::pollKeyboardState()
{
    keyState = SDL_GetKeyboardState(&keyCount);
    if (!keyState || keyCount <= 0)
    {
        keyState = nullptr;
        keyCount = 0;
        return;
    }

    if (previousKeyCount != keyCount)
    {
        previousKeyState.assign(static_cast<size_t>(keyCount), 0);
        previousKeyCount = keyCount;
    }
}

void Application::commitKeyboardState()
{
    if (!keyState || previousKeyState.empty())
    {
        return;
    }

    for (int i = 0; i < keyCount; i++)
    {
        previousKeyState[static_cast<size_t>(i)] = keyState[i] ? 1 : 0;
    }
}

void Application::initDefaultParty()
{
    party.resize(4);

    party[0].name = "Zoltan";
    party[0].faceId = 17;
    party[0].charClass = CharacterClass::Knight;
    updateCharacterForFace(party[0]);
    updateSkillsForClass(party[0]);
    party[0].skills.push_back("Mace");
    game::syncSkillLevelsFromDisplaySkills(party[0]);

    party[1].name = "Roderick";
    party[1].faceId = 3;
    party[1].charClass = CharacterClass::Monk;
    updateCharacterForFace(party[1]);
    updateSkillsForClass(party[1]);

    party[2].name = "Serena";
    party[2].faceId = 14;
    party[2].charClass = CharacterClass::Cleric;
    updateCharacterForFace(party[2]);
    updateSkillsForClass(party[2]);

    party[3].name = "Alexis";
    party[3].faceId = 10;
    party[3].charClass = CharacterClass::Sorcerer;

    updateCharacterForFace(party[3]);
    updateSkillsForClass(party[3]);
}

void Application::updateCharacterForFace(Character& ch)
{
    int groupIdx = faceGroupFromId(ch.faceId);
    for (int i = 0; i < 7; i++)
    {
        ch.baseStats.byIndex(i) = kFaceBaseStats[groupIdx][i];
    }
    ch.stats = ch.baseStats;
}

void Application::updateSkillsForClass(Character& ch)
{
    int classIdx = baseClassIndex(ch.charClass);
    ch.skills.clear();
    ch.skills.push_back(kClassStartingSkills[classIdx].skill1);
    ch.skills.push_back(kClassStartingSkills[classIdx].skill2);
    game::syncSkillLevelsFromDisplaySkills(ch);
}

void Application::commitPartyToGameWorld()
{
    if (!gameWorld_)
    {
        return;
    }
    // Copies each finalized member from the Application-side `party` vector into
    // the GameWorld's party. This assigns only per-member state; it does NOT
    // overwrite party-level fields (gold/food/world position/arrival override)
    // or inventory — those are set by CharacterCreationState's OK handler before
    // this runs, and member assignment leaves them untouched. Re-running
    // finalize here is idempotent with the OK handler's finalize loop.
    auto& gp = gameWorld_->party();
    for (int i = 0; i < game::kPartySize && i < static_cast<int>(party.size()); i++)
    {
        Character ch = party[static_cast<size_t>(i)];
        ch.baseStats = ch.stats;
        ch.recalculateDerived();
        ch.hitPoints = ch.maxHitPoints;
        ch.spellPoints = ch.maxSpellPoints;
        game::syncSkillLevelsFromDisplaySkills(ch);
        gp.member(i) = ch;
    }
    logger.info("Committed party data to GameWorld");
}

void Application::configureGameplayCallbacks()
{
    if (eventEngine_)
    {
        game::EventCallbacks callbacks;
        callbacks.onShowText = [this](const std::string& text)
        {
            if (sharedData)
            {
                if (sharedData->awaitingNpcDialogText)
                {
                    sharedData->npcDialogueText = text;
                    sharedData->openNpcDialogue = true;
                    sharedData->awaitingNpcDialogText = false;
                }
                else
                {
                    sharedData->statusMessage = text;
                }
            }
            logger.info(std::format("Event: {}", text));
        };
        callbacks.onNpcDialog = [this](int dialogTextId)
        {
            if (sharedData)
            {
                std::string speaker = std::format("NPC #{}", dialogTextId);
                if (auto ownerIt = npcDialogOwnerById_.find(dialogTextId);
                    ownerIt != npcDialogOwnerById_.end() && !ownerIt->second.empty())
                {
                    speaker = ownerIt->second;
                }
                else if (auto npcNameIt = npcNameById_.find(dialogTextId);
                         npcNameIt != npcNameById_.end() && !npcNameIt->second.empty())
                {
                    speaker = npcNameIt->second;
                }
                else if (!npcNamePool_.empty())
                {
                    const size_t idx =
                        static_cast<size_t>(std::max(0, dialogTextId)) % npcNamePool_.size();
                    speaker = npcNamePool_[idx];
                }

                sharedData->pendingNpcDialogId = dialogTextId;
                sharedData->awaitingNpcDialogText = true;
                sharedData->openNpcDialogue = true;
                if (auto textIt = npcDialogTextById_.find(dialogTextId);
                    textIt != npcDialogTextById_.end())
                {
                    sharedData->npcDialogueText = textIt->second;
                }
                else
                {
                    sharedData->npcDialogueText.clear();
                }

                // Check if NPC has a greeting via npcdata
                if (auto greetIdIt = npcGreetingIdByNpcId_.find(dialogTextId);
                    greetIdIt != npcGreetingIdByNpcId_.end())
                {
                    int greetId = greetIdIt->second;
                    if (auto greetEntryIt = npcGreetingById_.find(greetId);
                        greetEntryIt != npcGreetingById_.end())
                    {
                        // Use greeting 1 for now
                        if (!greetEntryIt->second.greeting1.empty())
                        {
                            sharedData->npcDialogueText = greetEntryIt->second.greeting1;
                        }
                    }
                }

                // Add profession information if they have one
                if (auto profIdIt = npcProfessionIdByNpcId_.find(dialogTextId);
                    profIdIt != npcProfessionIdByNpcId_.end())
                {
                    int profId = profIdIt->second;
                    if (auto profEntryIt = npcProfessionById_.find(profId);
                        profEntryIt != npcProfessionById_.end())
                    {
                        speaker += " (" + profEntryIt->second.name + ")";
                    }
                }

                sharedData->npcDialogueSpeaker = std::move(speaker);
                sharedData->npcDialogueChoiceIds.clear();
                sharedData->npcDialogueChoiceTexts.clear();

                std::vector<int> topicIds;
                if (auto byNpcIt = npcTopicIdsByNpcId_.find(dialogTextId);
                    byNpcIt != npcTopicIdsByNpcId_.end())
                {
                    topicIds = byNpcIt->second;
                }
                else if (auto byTextIt = npcTopicIdsByTextId_.find(dialogTextId);
                         byTextIt != npcTopicIdsByTextId_.end())
                {
                    topicIds = byTextIt->second;
                }

                for (int topicId : topicIds)
                {
                    if (topicId <= 0)
                    {
                        continue;
                    }
                    std::string topicLabel = std::format("Topic #{}", topicId);
                    if (auto topicIt = npcTopicNameById_.find(topicId);
                        topicIt != npcTopicNameById_.end() && !topicIt->second.empty())
                    {
                        topicLabel = topicIt->second;
                    }

                    sharedData->npcDialogueChoiceIds.push_back(topicId);
                    sharedData->npcDialogueChoiceTexts.push_back(std::move(topicLabel));
                    if (sharedData->npcDialogueChoiceIds.size() >= 4)
                    {
                        break;
                    }
                }
            }
        };
        callbacks.onShowBuilding = [this](int buildingId)
        {
            if (!sharedData)
                return;

            // If this building is a shop/service with a known UI family, hand
            // off to the shop window (EVT_SHOW_BUILDING -> Window Type 10).
            auto it = buildingEntryById_.find(buildingId);
            if (it != buildingEntryById_.end() && game::hasShopUI(it->second.buildingType))
            {
                sharedData->pendingShopBuildingId = buildingId;
                sharedData->pendingShopBuilding = it->second;
                sharedData->openShop = true;
                return;
            }

            // Otherwise surface the building name as a status message (guilds,
            // houses, and any building without a dedicated shop UI).
            const std::string name = resolveBuildingDisplayName(buildingId);
            sharedData->statusMessage =
                name.empty()
                    ? std::format("Building interaction opened (id #{})", buildingId)
                    : std::format("Building interaction opened: {} (id #{})", name, buildingId);
        };
        callbacks.onPlaySound = [this](int soundId) { playEventSound(soundId); };
        callbacks.onTeleport = [this](const std::string& map, float x, float y, float z, float yaw)
        {
            const std::string trimmedMap = toLower(map);
            const bool intraMapTeleport =
                map.empty() || (!trimmedMap.empty() && trimmedMap[0] == '0');

            if (gameWorld_)
            {
                gameWorld_->party().setWorldPosition(x, y, z);
                groundPartyToOutdoorTerrain(gameWorld_->party(), mapScene.get(), &logger,
                                            "teleport");
                gameWorld_->party().setOrientation(yaw, gameWorld_->party().pitch());
            }

            if (intraMapTeleport)
            {
                return;
            }

            preserveCurrentMapState();
            startupMapName = normalizeMapName(map, false);
            startupPreferOutdoor = (getLowerExtension(startupMapName) == ".odm");
            autoLoadMap = true;
            pendingEntryDirection_ = 0;
            pendingArrivalOverride_.active = true;
            pendingArrivalOverride_.x = x;
            pendingArrivalOverride_.y = y;
            pendingArrivalOverride_.z = z;
            pendingArrivalOverride_.yaw = yaw;
            pendingTransition_.active = true;
            pendingTransition_.sourceMap = gameWorld_ ? gameWorld_->currentMap() : "";
            pendingTransition_.targetMap = startupMapName;
            pendingTransition_.exitDirection = 0;
            pendingTransition_.transitionParam = 0;
            pendingTransition_.hasArrivalOverride = true;
            pendingTransition_.arrivalX = x;
            pendingTransition_.arrivalY = y;
            pendingTransition_.arrivalZ = z;
            pendingTransition_.arrivalYaw = yaw;

            if (sharedData)
            {
                sharedData->startupMapName = startupMapName;
                sharedData->startupPreferOutdoor = startupPreferOutdoor;
                sharedData->autoLoadMap = true;
                sharedData->loadingScreenIndex = 0;
                sharedData->statusMessage = this->buildTransitionText(startupMapName, 0);
            }

            setGameState(GameState::Loading);
            transitionTo(GameStateId::Loading);
        };
        callbacks.onChangeMap =
            [this](const std::string& map, int exitDirection, int transitionParam)
        {
            const std::string lower = toLower(map);
            if (lower == "arbiter good")
            {
                if (gameWorld_)
                {
                    gameWorld_->party().setAlignment(game::Alignment::Good);
                }
                if (sharedData)
                {
                    sharedData->statusMessage = "Party alignment changed to Good";
                }
                logger.info("Event: party alignment set to Good");
                return;
            }
            if (lower == "arbiter evil")
            {
                if (gameWorld_)
                {
                    gameWorld_->party().setAlignment(game::Alignment::Evil);
                }
                if (sharedData)
                {
                    sharedData->statusMessage = "Party alignment changed to Evil";
                }
                logger.info("Event: party alignment set to Evil");
                return;
            }

            std::string targetMap = map;
            if (lower == "pcout01")
            {
                targetMap = "out01.odm";
            }

            preserveCurrentMapState();
            startupMapName = normalizeMapName(targetMap, false);
            startupPreferOutdoor = (getLowerExtension(startupMapName) == ".odm");
            autoLoadMap = true;
            pendingEntryDirection_ = resolveSpawnIndexFromDirection(exitDirection);
            const game::MapTransition* transitionMeta =
                resolveInteractionTransition(gameWorld_.get(), startupMapName);
            const bool useArrivalOverride = transitionMeta && transitionMeta->hasArrivalOverride;
            pendingArrivalOverride_.active = false;
            if (useArrivalOverride)
            {
                pendingArrivalOverride_.active = true;
                pendingArrivalOverride_.x = transitionMeta->targetX;
                pendingArrivalOverride_.y = transitionMeta->targetY;
                pendingArrivalOverride_.z = transitionMeta->targetZ;
                pendingArrivalOverride_.yaw = transitionMeta->targetYaw;
                pendingEntryDirection_ = 0;
            }
            pendingTransition_.active = true;
            pendingTransition_.sourceMap = gameWorld_ ? gameWorld_->currentMap() : "";
            pendingTransition_.targetMap = startupMapName;
            pendingTransition_.exitDirection = pendingEntryDirection_;
            pendingTransition_.transitionParam = transitionParam;
            pendingTransition_.hasArrivalOverride = useArrivalOverride;
            pendingTransition_.arrivalX = 0.0f;
            pendingTransition_.arrivalY = 0.0f;
            pendingTransition_.arrivalZ = 0.0f;
            pendingTransition_.arrivalYaw = 0.0f;
            if (useArrivalOverride)
            {
                pendingTransition_.arrivalX = transitionMeta->targetX;
                pendingTransition_.arrivalY = transitionMeta->targetY;
                pendingTransition_.arrivalZ = transitionMeta->targetZ;
                pendingTransition_.arrivalYaw = transitionMeta->targetYaw;
            }

            if (sharedData)
            {
                sharedData->startupMapName = startupMapName;
                sharedData->startupPreferOutdoor = startupPreferOutdoor;
                sharedData->autoLoadMap = true;
                sharedData->loadingScreenIndex = std::max(0, transitionParam);
                sharedData->statusMessage =
                    this->buildTransitionText(startupMapName, pendingEntryDirection_);
            }

            setGameState(GameState::Loading);
            transitionTo(GameStateId::Loading);
        };
        callbacks.onGiveItem = [this](int itemId)
        {
            if (!inventory_ || itemId <= 0)
            {
                return;
            }
            game::Item item;
            item.itemId = itemId;
            (void)inventory_->giveItem(item);
        };
        callbacks.onRemoveItem = [this](int itemId) -> bool
        {
            if (!inventory_ || itemId <= 0)
            {
                return false;
            }
            return inventory_->removeItem(itemId);
        };
        callbacks.onMapCommand = [this](const game::EventCommand& cmd)
        {
            if (!mapScene)
            {
                return;
            }

            auto& blv = mapScene->mutableBLVData();
            auto& odm = mapScene->mutableODMData();

            const int param1 = cmd.param1;
            const int param2 = cmd.param2;
            [[maybe_unused]] const int param3 = cmd.param3; // used by some opcode branches
            const auto interaction =
                gameWorld_ ? gameWorld_->lastEventInteraction() : game::EventInteractionContext{};
            constexpr uint32_t kInvisible =
                static_cast<uint32_t>(formats::FaceAttribute::Invisible);

            auto resolveIndoorFaceIndex = [&]() -> int
            {
                if (param1 >= 0 && static_cast<size_t>(param1) < blv.faces.size())
                {
                    return param1;
                }
                if (interaction.type == game::EventInteractionType::IndoorFace &&
                    interaction.objectIndex >= 0 &&
                    static_cast<size_t>(interaction.objectIndex) < blv.faces.size())
                {
                    return interaction.objectIndex;
                }
                return -1;
            };

            auto resolveIndoorDecorationIndex = [&]() -> int
            {
                if (interaction.type == game::EventInteractionType::IndoorDecoration &&
                    interaction.objectIndex >= 0 &&
                    static_cast<size_t>(interaction.objectIndex) < blv.decorations.size())
                {
                    return interaction.objectIndex;
                }
                if (param1 >= 0 && static_cast<size_t>(param1) < blv.decorations.size())
                {
                    return param1;
                }
                return -1;
            };

            struct OutdoorFaceTarget
            {
                int buildingIndex = -1;
                int faceIndex = -1; // -1 means "all faces in building"
            };

            auto decodeOutdoorFaceTarget = [&](int packedFaceIndex) -> OutdoorFaceTarget
            {
                OutdoorFaceTarget target;
                if (packedFaceIndex < 0)
                {
                    return target;
                }

                const int buildingIndex = (packedFaceIndex >> 16) & 0xFFFF;
                const int faceIndex = packedFaceIndex & 0xFFFF;
                if (buildingIndex < 0 || static_cast<size_t>(buildingIndex) >= odm.buildings.size())
                {
                    return target;
                }

                target.buildingIndex = buildingIndex;
                const auto& building = odm.buildings[static_cast<size_t>(buildingIndex)];
                if (faceIndex >= 0 && static_cast<size_t>(faceIndex) < building.faces.size())
                {
                    target.faceIndex = faceIndex;
                }
                return target;
            };

            auto resolveOutdoorFaceTarget = [&]() -> OutdoorFaceTarget
            {
                if (interaction.type == game::EventInteractionType::OutdoorBuildingFace &&
                    interaction.objectIndex >= 0)
                {
                    return decodeOutdoorFaceTarget(interaction.objectIndex);
                }

                if (param1 >= 0)
                {
                    if (static_cast<size_t>(param1) < odm.buildings.size())
                    {
                        OutdoorFaceTarget target;
                        target.buildingIndex = param1;
                        return target;
                    }

                    if (((param1 >> 16) & 0xFFFF) != 0)
                    {
                        return decodeOutdoorFaceTarget(param1);
                    }
                }

                return {};
            };

            auto applyFaceVisibilityAction =
                [&](uint32_t& attributes, int action, bool zeroMeansHide)
            {
                const bool currentlyHidden = (attributes & kInvisible) != 0;
                bool hide = currentlyHidden;

                if (action == 2)
                {
                    hide = !currentlyHidden;
                }
                else if (zeroMeansHide)
                {
                    hide = (action == 0);
                }
                else
                {
                    hide = (action != 0);
                }

                if (hide)
                {
                    attributes |= kInvisible;
                }
                else
                {
                    attributes &= ~kInvisible;
                }
            };

            switch (cmd.opcode)
            {
            case game::EventOpcode::DoorControl:
                if (const int faceIndex = resolveIndoorFaceIndex(); faceIndex >= 0)
                {
                    auto& face = blv.faces[static_cast<size_t>(faceIndex)];
                    applyFaceVisibilityAction(face.attributes, param2, true);
                }
                break;

            case game::EventOpcode::ModifyObject:
                if (const int faceIndex = resolveIndoorFaceIndex(); faceIndex >= 0)
                {
                    auto& face = blv.faces[static_cast<size_t>(faceIndex)];
                    applyFaceVisibilityAction(face.attributes, param2, false);
                }
                break;

            case game::EventOpcode::ModifyDecoration:
                if (const int decorIndex = resolveIndoorDecorationIndex(); decorIndex >= 0)
                {
                    auto& decoration = blv.decorations[static_cast<size_t>(decorIndex)];
                    if (param2 == 2)
                    {
                        decoration.hidden = !decoration.hidden;
                    }
                    else
                    {
                        decoration.hidden = (param2 == 0);
                    }
                    break;
                }

                if (const auto target = resolveOutdoorFaceTarget(); target.buildingIndex >= 0)
                {
                    auto& building = odm.buildings[static_cast<size_t>(target.buildingIndex)];
                    if (target.faceIndex >= 0 &&
                        static_cast<size_t>(target.faceIndex) < building.faces.size())
                    {
                        applyFaceVisibilityAction(
                            building.faces[static_cast<size_t>(target.faceIndex)].attributes,
                            param2, true);
                        break;
                    }

                    for (auto& face : building.faces)
                    {
                        applyFaceVisibilityAction(face.attributes, param2, true);
                    }
                }
                break;

            case game::EventOpcode::SpawnItem:
                if (gameWorld_)
                {
                    const game::GameVarId marker = static_cast<game::GameVarId>(0x7600);
                    gameWorld_->setVar(marker, gameWorld_->getVar(marker) + std::max(1, param2));
                }
                break;

            default:
                // Routed opcodes the host doesn't yet implement (e.g. ModifyNpc,
                // ModifyNpcEx, ShowEffect, PlayAnimation) land here. Log once
                // per opcode so the gap is visible rather than a silent no-op.
                logger.debug(std::format("onMapCommand: opcode {} not yet implemented",
                                         static_cast<int>(cmd.opcode)));
                break;
            }
        };
        callbacks.onSetGlobalVar = [this](int varIndex, int field, int value)
        {
            // Bridge quest-bit writes into the QuestLog. RE: setting a quest
            // bit (field 0) to a nonzero value acquires the quest; the
            // 0->nonzero transition is the "New Quest!" moment. startQuest
            // itself dedupes (only fires the journal entry on Unknown->Active).
            if (!questLog_ || !gameWorld_)
            {
                return;
            }
            if (field == 0 && value != 0)
            {
                const uint64_t gameTime =
                    static_cast<uint64_t>(std::max<int64_t>(0, gameWorld_->calendar().totalTicks));
                questLog_->startQuest(varIndex, gameTime);
            }
        };
        eventEngine_->setCallbacks(callbacks);
    }

    if (combatSystem_)
    {
        game::CombatCallbacks callbacks;
        callbacks.onCharacterAttack = [this](int characterIndex,
                                             const game::MonsterInstance& target,
                                             const game::AttackResult& result)
        {
            logger.debug(std::format("Combat: party[{}] -> {} :: {}", characterIndex, target.name,
                                     result.description));
        };
        callbacks.onMonsterAttack = [this](const game::MonsterInstance& attacker,
                                           int characterIndex, const game::AttackResult& result)
        {
            logger.debug(std::format("Combat: {} -> party[{}] :: {}", attacker.name, characterIndex,
                                     result.description));
        };
        callbacks.onMonsterKilled = [this](const game::MonsterInstance& monster, int xpReward)
        { logger.info(std::format("Combat: {} defeated (+{} xp)", monster.name, xpReward)); };
        callbacks.onCharacterDowned = [this](int characterIndex)
        { logger.warning(std::format("Combat: party member {} downed", characterIndex)); };
        combatSystem_->setCallbacks(callbacks);
    }

    if (spellSystem_)
    {
        game::SpellCallbacks callbacks;
        callbacks.onSpellCast =
            [this](int spellId, int casterIndex, const game::SpellResult& result)
        {
            logger.debug(std::format("Spell #{} by party[{}]: {}", spellId, casterIndex,
                                     result.description));
        };
        callbacks.onSpellFailed = [this](int spellId, const std::string& reason)
        { logger.debug(std::format("Spell #{} failed: {}", spellId, reason)); };
        // Route spell kills through CombatSystem so they award XP and fire the
        // death UI callback, matching melee kills.
        callbacks.onMonsterKilled = [this](game::MonsterInstance& monster, int /*xp*/)
        {
            if (combatSystem_)
            {
                combatSystem_->awardMonsterKill(monster);
            }
        };
        spellSystem_->setCallbacks(callbacks);
    }
}

void Application::loadDataTables()
{
    if (!vfs)
    {
        return;
    }

    auto readFirstExisting =
        [this](const std::vector<std::string>& names) -> std::optional<std::vector<uint8_t>>
    {
        for (const auto& name : names)
        {
            if (auto data = vfs->readFile(name); data.has_value())
            {
                return data;
            }
        }
        return std::nullopt;
    };

    if (combatSystem_)
    {
        std::vector<formats::MonsterEntry> parsedMonsters;

        if (auto monstersData = readFirstExisting({"monsters.txt", "MONSTERS.TXT", "Monsters.txt"});
            monstersData.has_value())
        {
            formats::MonstersParser parser(logger);
            if (parser.parse(*monstersData))
            {
                parsedMonsters = parser.getMonsters();
                combatSystem_->loadMonsterData(parsedMonsters);
            }
        }

        if (!parsedMonsters.empty())
        {
            if (auto hostileData = readFirstExisting({"hostile.txt", "HOSTILE.TXT", "Hostile.txt"});
                hostileData.has_value())
            {
                formats::HostileParser parser(logger);
                if (parser.parse(*hostileData))
                {
                    std::unordered_map<int, bool> hostilityByMonsterId;
                    hostilityByMonsterId.reserve(parsedMonsters.size());

                    const auto& matrix = parser.getHostileMatrix();
                    for (const auto& monster : parsedMonsters)
                    {
                        auto hostileValue = matrix.getHostilityInsensitive(monster.name, "Party");
                        if (!hostileValue.has_value())
                        {
                            hostileValue = matrix.getHostilityInsensitive("Party", monster.name);
                        }
                        if (hostileValue.has_value())
                        {
                            hostilityByMonsterId[monster.id] = (*hostileValue != 0);
                        }
                    }

                    combatSystem_->setPartyHostilityByMonsterId(std::move(hostilityByMonsterId));
                }
            }
        }
    }

    if (spellSystem_)
    {
        if (auto spellsData = readFirstExisting({"spells.txt", "SPELLS.TXT", "Spells.txt"});
            spellsData.has_value())
        {
            formats::SpellsParser parser(logger);
            if (parser.parse(*spellsData))
            {
                spellSystem_->loadSpellData(parser.getSpells());
            }
        }
    }

    if (inventory_)
    {
        if (auto itemsData = readFirstExisting({"items.txt", "ITEMS.TXT", "Items.txt"});
            itemsData.has_value())
        {
            formats::ItemsParser parser(logger);
            if (parser.parse(*itemsData))
            {
                inventory_->loadItemData(parser.getItems());
            }
        }
    }

    // Quest catalog + journal (RE: quests.txt is <id>\t"<text>" per line; the
    // quest index IS the array position, so QBit N -> quest row N).
    if (questLog_)
    {
        if (auto questsData = readFirstExisting({"quests.txt", "QUESTS.TXT", "Quests.txt"});
            questsData.has_value())
        {
            formats::QuestsParser parser(logger);
            if (parser.parse(*questsData))
            {
                questLog_->loadQuestData(parser.getQuests());
                logger.info(std::format("Loaded {} quest definitions", parser.getQuests().size()));
            }
        }
    }

    // Autonote + award catalogs (for the journal's autonotes/awards tabs).
    if (auto autonoteData = readFirstExisting({"autonote.txt", "AUTONOTE.TXT", "Autonote.txt"});
        autonoteData.has_value())
    {
        formats::AutonoteParser parser(logger);
        if (parser.parse(*autonoteData))
        {
            autonoteEntries_ = parser.getAutonoteEntries();
            logger.info(std::format("Loaded {} autonote entries", autonoteEntries_.size()));
        }
    }
    if (auto awardsData = readFirstExisting({"awards.txt", "AWARDS.TXT", "Awards.txt"});
        awardsData.has_value())
    {
        formats::AwardsParser parser(logger);
        if (parser.parse(*awardsData))
        {
            awardEntries_ = parser.getAwards();
            logger.info(std::format("Loaded {} award entries", awardEntries_.size()));
        }
    }

    if (contentGenerator_)
    {
        if (auto placeMonData = readFirstExisting({"placemon.txt", "PLACEMON.TXT", "PlaceMon.txt"});
            placeMonData.has_value())
        {
            formats::PlacemonParser parser(logger);
            if (parser.parse(*placeMonData))
            {
                std::vector<game::MonsterPlacementRule> rules;
                std::unordered_map<std::string, size_t> ruleIndex;
                rules.reserve(parser.getEntries().size());

                for (const auto& entry : parser.getEntries())
                {
                    const std::string key = toLower(entry.mapName) + "|" +
                                            std::to_string(entry.minDifficulty) + "|" +
                                            std::to_string(entry.maxDifficulty);

                    size_t idx = 0;
                    if (auto it = ruleIndex.find(key); it != ruleIndex.end())
                    {
                        idx = it->second;
                    }
                    else
                    {
                        idx = rules.size();
                        ruleIndex[key] = idx;
                        game::MonsterPlacementRule rule;
                        rule.mapName = toLower(entry.mapName);
                        rule.minDifficulty = entry.minDifficulty;
                        rule.maxDifficulty = entry.maxDifficulty;
                        rules.push_back(std::move(rule));
                    }

                    rules[idx].entries.push_back({entry.monsterId, entry.weight});
                }

                contentGenerator_->setMonsterPlacementRules(std::move(rules));
            }
        }

        if (auto rndItemsData = readFirstExisting({"rnditems.txt", "RNDITEMS.TXT", "RndItems.txt"});
            rndItemsData.has_value())
        {
            formats::RndItemsParser parser(logger);
            if (parser.parse(*rndItemsData))
            {
                std::vector<game::TreasureItemWeight> weights;
                weights.reserve(parser.getEntries().size());
                for (const auto& entry : parser.getEntries())
                {
                    game::TreasureItemWeight weight;
                    weight.itemId = entry.itemId;
                    weight.baseLevel = entry.baseLevel;
                    weight.levelWeights = entry.levelWeights;
                    weights.push_back(weight);
                }
                contentGenerator_->setTreasureItemWeights(std::move(weights));
            }
        }
    }

    buildingDisplayNameById_.clear();
    if (auto twoDEventsData = readFirstExisting({"2dEvents.txt", "2DEVENTS.TXT", "2DEvents.txt"});
        twoDEventsData.has_value())
    {
        formats::TwoDEventsParser parser(logger);
        if (parser.parse(*twoDEventsData))
        {
            for (const auto& entry : parser.getEntries())
            {
                if (!entry.displayName.empty())
                {
                    buildingDisplayNameById_[entry.id] = entry.displayName;
                }
                // Keep the full typed entry (used by the shop window for name,
                // proprietor, buy multiplier, and building type).
                buildingEntryById_[entry.id] = entry;
            }
            logger.info(std::format("Loaded {} 2dEvents building definitions",
                                    buildingDisplayNameById_.size()));
        }
    }

    npcDialogTextById_.clear();
    npcDialogOwnerById_.clear();
    npcNameById_.clear();
    npcTopicIdsByNpcId_.clear();
    npcTopicIdsByTextId_.clear();
    npcTopicNameById_.clear();
    if (auto npcTextData = readFirstExisting({"npctext.txt", "NPCTEXT.TXT", "NPCText.txt"});
        npcTextData.has_value())
    {
        formats::NPCTextParser parser(logger);
        if (parser.parse(*npcTextData))
        {
            for (const auto& entry : parser.getNPCTextEntries())
            {
                if (entry.id <= 0)
                {
                    continue;
                }
                if (!entry.text.empty())
                {
                    npcDialogTextById_[entry.id] = entry.text;
                }
                if (!entry.owner.empty())
                {
                    npcDialogOwnerById_[entry.id] = entry.owner;
                }
            }
            logger.info(std::format("Loaded {} npc text entries", npcDialogTextById_.size()));
        }
    }

    if (auto npcTopicData = readFirstExisting({"npctopic.txt", "NPCTOPIC.TXT", "NPCTOPIC.txt"});
        npcTopicData.has_value())
    {
        formats::NPCTopicParser parser(logger);
        if (parser.parse(*npcTopicData))
        {
            for (const auto& entry : parser.getEntries())
            {
                if (entry.id <= 0)
                {
                    continue;
                }
                if (!entry.topic.empty())
                {
                    npcTopicNameById_[entry.id] = entry.topic;
                }
                for (int textId : entry.textIds)
                {
                    if (textId <= 0)
                    {
                        continue;
                    }
                    auto& topics = npcTopicIdsByTextId_[textId];
                    if (std::find(topics.begin(), topics.end(), entry.id) == topics.end())
                    {
                        topics.push_back(entry.id);
                    }
                }
            }

            logger.info(std::format("Loaded {} npc topic labels and {} text->topic maps",
                                    npcTopicNameById_.size(), npcTopicIdsByTextId_.size()));
        }
    }

    if (auto npcData = readFirstExisting({"npcdata.txt", "NPCDATA.TXT", "NPCData.txt"});
        npcData.has_value())
    {
        formats::NPCDataParser parser(logger);
        if (parser.parse(*npcData))
        {
            for (const auto& entry : parser.getEntries())
            {
                if (entry.id <= 0)
                {
                    continue;
                }

                if (!entry.name.empty())
                {
                    npcNameById_[entry.id] = entry.name;
                }

                std::vector<int> topicIds;
                topicIds.reserve(entry.actionEventIds.size());
                for (int topicId : entry.actionEventIds)
                {
                    if (topicId <= 0)
                    {
                        continue;
                    }
                    if (std::find(topicIds.begin(), topicIds.end(), topicId) == topicIds.end())
                    {
                        topicIds.push_back(topicId);
                    }
                }
                if (!topicIds.empty())
                {
                    npcTopicIdsByNpcId_[entry.id] = std::move(topicIds);
                }

                npcProfessionIdByNpcId_[entry.id] = entry.professionId;
                npcGreetingIdByNpcId_[entry.id] = entry.greetingId;
            }

            logger.info(std::format("Loaded {} npc profiles with {} dialogue topic sets",
                                    npcNameById_.size(), npcTopicIdsByNpcId_.size()));
        }
    }

    npcNamePool_.clear();
    if (auto npcNamesData = readFirstExisting({"npcnames.txt", "NPCNAMES.TXT", "NPCNames.txt"});
        npcNamesData.has_value())
    {
        formats::NPCNamesParser parser(logger);
        if (parser.parse(*npcNamesData))
        {
            const auto& names = parser.getNPCNames();
            npcNamePool_.reserve(names.maleNames.size() + names.femaleNames.size());
            for (const auto& name : names.maleNames)
            {
                if (!name.empty())
                {
                    npcNamePool_.push_back(name);
                }
            }
            for (const auto& name : names.femaleNames)
            {
                if (!name.empty())
                {
                    npcNamePool_.push_back(name);
                }
            }
            logger.info(std::format("Loaded {} npc fallback names", npcNamePool_.size()));
        }
    }

    npcProfessionById_.clear();
    if (auto profData = readFirstExisting({"npcprof.txt", "NPCPROF.TXT", "NPCProf.txt"});
        profData.has_value())
    {
        formats::NPCProfessionParser parser(logger);
        if (parser.parse(*profData))
        {
            for (const auto& entry : parser.getEntries())
            {
                if (entry.id >= 0)
                {
                    npcProfessionById_[entry.id] = entry;
                }
            }
            logger.info(std::format("Loaded {} npc professions", npcProfessionById_.size()));
        }
    }

    npcGreetingById_.clear();
    if (auto greetData = readFirstExisting({"npcgreet.txt", "NPCGREET.TXT", "NPCGreet.txt"});
        greetData.has_value())
    {
        formats::NPCGreetingParser parser(logger);
        if (parser.parse(*greetData))
        {
            for (const auto& entry : parser.getEntries())
            {
                if (entry.id >= 0)
                {
                    npcGreetingById_[entry.id] = entry;
                }
            }
            logger.info(std::format("Loaded {} npc greetings", npcGreetingById_.size()));
        }
    }

    soundNameById_.clear();
    if (soundList_)
    {
        if (auto dsoundsData = readFirstExisting({"dsounds.bin", "DSOUNDS.BIN"});
            dsoundsData.has_value() && soundList_->parse(*dsoundsData))
        {
            for (const auto& [soundId, event] : soundList_->getAllSounds())
            {
                if (!event.name.empty())
                {
                    soundNameById_[soundId] = event.name;
                }
            }
            logger.info(
                std::format("Loaded {} sound ID mappings from dsounds.bin", soundNameById_.size()));
        }
    }
}

void Application::loadGlobalEventScripts()
{
    if (!vfs || !evtParser_ || !eventEngine_)
    {
        return;
    }

    std::optional<std::vector<uint8_t>> evtData;
    for (const auto& name : {"global.evt", "GLOBAL.EVT", "Global.evt"})
    {
        evtData = vfs->readFile(name);
        if (evtData.has_value())
        {
            break;
        }
    }

    if (!evtData.has_value())
    {
        globalEventScripts_.clear();
        eventEngine_->clear();
        logger.warning("Global event script not found (global.evt)");
        return;
    }

    std::vector<std::string> strings = {""};
    for (const auto& name : {"global.str", "GLOBAL.STR", "Global.str"})
    {
        if (auto strData = vfs->readFile(name); strData.has_value())
        {
            strings = evtParser_->parseStringTable(*strData);
            break;
        }
    }

    globalEventScripts_ = evtParser_->parseEventData(*evtData, strings);
    eventEngine_->clear();
    eventEngine_->loadEvents(globalEventScripts_);
    eventEngine_->setMapScopedEvents({});
    logger.info(std::format("Loaded {} global event scripts", globalEventScripts_.size()));
}

void Application::loadMapEventScripts(const std::string& mapName)
{
    if (!vfs || !evtParser_ || !eventEngine_)
    {
        return;
    }

    eventEngine_->clear();
    if (!globalEventScripts_.empty())
    {
        eventEngine_->loadEvents(globalEventScripts_);
    }

    if (mapName.empty())
    {
        eventEngine_->setMapScopedEvents({});
        if (gameWorld_)
        {
            gameWorld_->clearTransitions();
        }
        return;
    }

    std::string baseName = mapName;
    const size_t dot = baseName.find_last_of('.');
    if (dot != std::string::npos)
    {
        baseName = baseName.substr(0, dot);
    }

    std::optional<std::vector<uint8_t>> evtData;
    const std::array<std::string, 3> evtCandidates = {baseName + ".evt", toLower(baseName) + ".evt",
                                                      toLower(baseName) + ".EVT"};
    for (const auto& name : evtCandidates)
    {
        evtData = vfs->readFile(name);
        if (evtData.has_value())
        {
            break;
        }
    }

    if (!evtData.has_value())
    {
        eventEngine_->setMapScopedEvents({});
        if (gameWorld_)
        {
            gameWorld_->clearTransitions();
        }
        logger.debug(std::format("No map event script for {}", mapName));
        return;
    }

    std::vector<std::string> strings = {""};
    const std::array<std::string, 3> strCandidates = {baseName + ".str", toLower(baseName) + ".str",
                                                      toLower(baseName) + ".STR"};
    for (const auto& name : strCandidates)
    {
        if (auto strData = vfs->readFile(name); strData.has_value())
        {
            strings = evtParser_->parseStringTable(*strData);
            break;
        }
    }

    auto mapScripts = evtParser_->parseEventData(*evtData, strings);

    if (gameWorld_)
    {
        gameWorld_->clearTransitions();

        for (const auto& script : mapScripts)
        {
            game::MapTransition selectedTransition;
            bool hasSelectedTransition = false;

            for (const auto& cmd : script.commands)
            {
                game::MapTransition transition;
                bool hasTransition = false;

                if (cmd.opcode == game::EventOpcode::ChangeMap && !cmd.text.empty())
                {
                    std::string target = toLower(cmd.text);
                    if (target == "pcout01")
                    {
                        target = "out01.odm";
                    }
                    if (target != "arbiter good" && target != "arbiter evil")
                    {
                        const std::string normalized = normalizeMapName(target, false);
                        const std::string ext = getLowerExtension(normalized);
                        if (ext == ".blv" || ext == ".odm")
                        {
                            transition.targetMap = normalized;
                            const auto yaw = directionToEntryYaw(cmd.param1);
                            transition.targetYaw = yaw.has_value() ? *yaw : 0.0f;
                            transition.hasArrivalOverride = false;
                            hasTransition = true;
                        }
                    }
                }
                else if (cmd.opcode == game::EventOpcode::Teleport && !cmd.text.empty() &&
                         cmd.text.front() != '0')
                {
                    const std::string normalized = normalizeMapName(cmd.text, false);
                    const std::string ext = getLowerExtension(normalized);
                    if (ext == ".blv" || ext == ".odm")
                    {
                        transition.targetMap = normalized;
                        transition.targetX = cmd.fparam;
                        transition.targetY = cmd.fparam2;
                        transition.targetZ = cmd.fparam3;
                        transition.targetYaw = static_cast<float>(cmd.param1);
                        transition.hasArrivalOverride = true;
                        hasTransition = true;
                    }
                }

                if (!hasTransition)
                {
                    continue;
                }

                if (!hasSelectedTransition ||
                    (!selectedTransition.hasArrivalOverride && transition.hasArrivalOverride))
                {
                    selectedTransition = std::move(transition);
                    hasSelectedTransition = true;
                }
            }

            if (hasSelectedTransition)
            {
                selectedTransition.targetDisplayName =
                    resolveMapDisplayName(selectedTransition.targetMap);
                gameWorld_->addTransition(script.eventId, selectedTransition);
            }
        }
    }

    eventEngine_->setMapScopedEvents(mapScripts);
    logger.info(std::format("Loaded {} map event scripts for {}", mapScripts.size(), mapName));
}

void Application::preserveCurrentMapState()
{
    if (!gameWorld_ || !mapScene || !mapScene->isLoaded())
    {
        return;
    }

    const std::string mapName = mapScene->getName();
    if (mapName.empty())
    {
        return;
    }

    game::SavedMapState state;

    const auto& blv = mapScene->getBLVData();
    if (!blv.faces.empty())
    {
        state.indoorFaceAttributes.reserve(blv.faces.size());
        for (const auto& face : blv.faces)
        {
            state.indoorFaceAttributes.push_back(face.attributes);
        }
    }
    if (!blv.decorations.empty())
    {
        state.indoorDecorationHidden.reserve(blv.decorations.size());
        for (const auto& decoration : blv.decorations)
        {
            state.indoorDecorationHidden.push_back(decoration.hidden ? 1u : 0u);
        }
    }

    const auto& odm = mapScene->getODMData();
    if (!odm.buildings.empty())
    {
        state.outdoorBuildingFaceAttributes.resize(odm.buildings.size());
        for (size_t bi = 0; bi < odm.buildings.size(); bi++)
        {
            const auto& building = odm.buildings[bi];
            auto& attrs = state.outdoorBuildingFaceAttributes[bi];
            attrs.reserve(building.faces.size());
            for (const auto& face : building.faces)
            {
                attrs.push_back(face.attributes);
            }
        }
    }

    gameWorld_->setSavedMapState(mapName, std::move(state));
}

void Application::restoreCurrentMapState(const std::string& mapName)
{
    if (!gameWorld_ || !mapScene || !mapScene->isLoaded() || mapName.empty())
    {
        return;
    }

    const game::SavedMapState* state = gameWorld_->getSavedMapState(mapName);
    if (!state)
    {
        return;
    }

    auto& blv = mapScene->mutableBLVData();
    if (!blv.faces.empty() && !state->indoorFaceAttributes.empty())
    {
        const size_t count = std::min(blv.faces.size(), state->indoorFaceAttributes.size());
        for (size_t i = 0; i < count; i++)
        {
            blv.faces[i].attributes = state->indoorFaceAttributes[i];
        }
    }
    if (!blv.decorations.empty() && !state->indoorDecorationHidden.empty())
    {
        const size_t count = std::min(blv.decorations.size(), state->indoorDecorationHidden.size());
        for (size_t i = 0; i < count; i++)
        {
            blv.decorations[i].hidden = state->indoorDecorationHidden[i] != 0;
        }
    }

    auto& odm = mapScene->mutableODMData();
    if (!odm.buildings.empty() && !state->outdoorBuildingFaceAttributes.empty())
    {
        const size_t buildingCount =
            std::min(odm.buildings.size(), state->outdoorBuildingFaceAttributes.size());
        for (size_t bi = 0; bi < buildingCount; bi++)
        {
            auto& building = odm.buildings[bi];
            const auto& savedFaces = state->outdoorBuildingFaceAttributes[bi];
            const size_t faceCount = std::min(building.faces.size(), savedFaces.size());
            for (size_t fi = 0; fi < faceCount; fi++)
            {
                building.faces[fi].attributes = savedFaces[fi];
            }
        }
    }
}

void Application::applyMapEntryPoint()
{
    fprintf(stderr,
            "[APPLY-ENTRY] pendingOverride=%d sharedOverride=%d entryDir=%d "
            "partyPos=(%.1f,%.1f,%.1f) yaw=%.1f indoorSpawns=%zu outdoorSpawns=%zu\n",
            pendingArrivalOverride_.active ? 1 : 0,
            (sharedData && sharedData->arrivalOverrideActive) ? 1 : 0, pendingEntryDirection_,
            gameWorld_ ? gameWorld_->party().worldX() : 0.0f,
            gameWorld_ ? gameWorld_->party().worldY() : 0.0f,
            gameWorld_ ? gameWorld_->party().worldZ() : 0.0f,
            gameWorld_ ? gameWorld_->party().yaw() : 0.0f,
            (mapScene && mapScene->isLoaded()) ? mapScene->getBLVData().spawns.size() : 0u,
            (mapScene && mapScene->isLoaded()) ? mapScene->getODMData().spawns.size() : 0u);

    if (!gameWorld_ || !mapScene || !mapScene->isLoaded())
    {
        pendingArrivalOverride_.active = false;
        pendingEntryDirection_ = 0;
        if (sharedData)
            sharedData->arrivalOverrideActive = false;
        return;
    }

    if (pendingArrivalOverride_.active || (sharedData && sharedData->arrivalOverrideActive))
    {
        float x = pendingArrivalOverride_.active ? pendingArrivalOverride_.x : sharedData->arrivalX;
        float y = pendingArrivalOverride_.active ? pendingArrivalOverride_.y : sharedData->arrivalY;
        float z = pendingArrivalOverride_.active ? pendingArrivalOverride_.z : sharedData->arrivalZ;
        float yaw =
            pendingArrivalOverride_.active ? pendingArrivalOverride_.yaw : sharedData->arrivalYaw;

        gameWorld_->party().setWorldPosition(x, y, z);
        groundPartyToOutdoorTerrain(gameWorld_->party(), mapScene.get(), &logger,
                                    "arrival-override");
        gameWorld_->party().setOrientation(yaw, gameWorld_->party().pitch());

        pendingArrivalOverride_.active = false;
        if (sharedData)
            sharedData->arrivalOverrideActive = false;
        pendingEntryDirection_ = 0;
        return;
    }

    const int spawnIndex = resolveSpawnIndexFromDirection(pendingEntryDirection_);
    const auto entryYaw = directionToEntryYaw(pendingEntryDirection_);
    pendingEntryDirection_ = 0;

    auto applyEntryOrientation = [this, entryYaw]()
    {
        if (!gameWorld_ || !entryYaw.has_value())
        {
            return;
        }
        gameWorld_->party().setOrientation(*entryYaw, gameWorld_->party().pitch());
    };

    const auto& indoorSpawns = mapScene->getBLVData().spawns;
    if (!indoorSpawns.empty())
    {
        size_t selected = 0;
        if (spawnIndex > 0)
        {
            bool matched = false;
            for (size_t i = 0; i < indoorSpawns.size(); i++)
            {
                const auto& spawn = indoorSpawns[i];
                if (spawn.group == spawnIndex || static_cast<int>(spawn.objectIndex) == spawnIndex)
                {
                    selected = i;
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                const size_t fallback = static_cast<size_t>(spawnIndex - 1);
                if (fallback < indoorSpawns.size())
                {
                    selected = fallback;
                }
            }
        }

        const auto& spawn = indoorSpawns[selected];
        gameWorld_->party().setWorldPosition(
            static_cast<float>(spawn.x), static_cast<float>(spawn.y), static_cast<float>(spawn.z));
        applyEntryOrientation();
        return;
    }

    // Outdoor maps mark the default arrival point with a "Party Start" decoration.
    // ODM spawn points are monster generators, so falling through to them drops the
    // party wherever the first generator happens to sit (usually far offshore).
    const auto& outdoorDecorations = mapScene->getODMData().decorations;
    for (const auto& decoration : outdoorDecorations)
    {
        if (toLower(decoration.name) != "party start")
        {
            continue;
        }

        gameWorld_->party().setWorldPosition(static_cast<float>(decoration.x),
                                             static_cast<float>(decoration.y),
                                             static_cast<float>(decoration.z));
        groundPartyToOutdoorTerrain(gameWorld_->party(), mapScene.get(), &logger, "party-start");
        applyEntryOrientation();
        return;
    }

    const auto& outdoorSpawns = mapScene->getODMData().spawns;
    if (!outdoorSpawns.empty())
    {
        size_t selected = 0;
        if (spawnIndex > 0)
        {
            bool matched = false;
            for (size_t i = 0; i < outdoorSpawns.size(); i++)
            {
                const auto& spawn = outdoorSpawns[i];
                if (spawn.group == spawnIndex || static_cast<int>(spawn.objectIndex) == spawnIndex)
                {
                    selected = i;
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                const size_t fallback = static_cast<size_t>(spawnIndex - 1);
                if (fallback < outdoorSpawns.size())
                {
                    selected = fallback;
                }
            }
        }

        const auto& spawn = outdoorSpawns[selected];
        gameWorld_->party().setWorldPosition(
            static_cast<float>(spawn.x), static_cast<float>(spawn.y), static_cast<float>(spawn.z));
        groundPartyToOutdoorTerrain(gameWorld_->party(), mapScene.get(), &logger, "outdoor-spawn");
        applyEntryOrientation();
    }
}

void Application::loadMapStatsTable()
{
    if (!vfs)
    {
        return;
    }

    std::optional<std::vector<uint8_t>> mapStatsData = vfs->readFile("MAPSTATS.TXT");
    if (!mapStatsData.has_value())
    {
        mapStatsData = vfs->readFile("MapStats.txt");
    }
    if (!mapStatsData.has_value())
    {
        mapStatsData = vfs->readFile("mapstats.txt");
    }

    if (!mapStatsData.has_value())
    {
        logger.warning("MapStats table not found; using fallback map generation difficulty");
        mapDifficultyByFileName_.clear();
        mapRespawnDaysByFileName_.clear();
        mapDisplayNameByFileName_.clear();
        return;
    }

    formats::MapStatsParser parser(logger);
    if (!parser.parse(*mapStatsData))
    {
        logger.warning("Failed to parse MAPSTATS.TXT; using fallback map generation difficulty");
        mapDifficultyByFileName_.clear();
        mapRespawnDaysByFileName_.clear();
        mapDisplayNameByFileName_.clear();
        return;
    }

    mapDifficultyByFileName_.clear();
    mapRespawnDaysByFileName_.clear();
    mapDisplayNameByFileName_.clear();
    for (const auto& entry : parser.getMapStats())
    {
        if (entry.fileName.empty())
        {
            continue;
        }
        const std::string key = toLower(entry.fileName);
        mapDifficultyByFileName_[key] =
            game::ContentGenerator::estimateDifficultyFromMapStats(entry);
        mapRespawnDaysByFileName_[key] = std::max(0, entry.refillDays);
        if (!entry.name.empty())
        {
            mapDisplayNameByFileName_[key] = entry.name;
        }
    }

    logger.info(std::format("Loaded {} map difficulty entries from MAPSTATS.TXT",
                            mapDifficultyByFileName_.size()));
}

int Application::resolveMapDifficulty(const std::string& mapName, int fallback) const
{
    if (mapName.empty() || mapDifficultyByFileName_.empty())
    {
        return std::clamp(fallback, 1, 10);
    }

    const std::string key = toLower(mapName);
    auto it = mapDifficultyByFileName_.find(key);
    if (it != mapDifficultyByFileName_.end())
    {
        return std::clamp(it->second, 1, 10);
    }

    return std::clamp(fallback, 1, 10);
}

int Application::resolveMapRespawnDays(const std::string& mapName, int fallback) const
{
    if (mapName.empty() || mapRespawnDaysByFileName_.empty())
    {
        return std::max(0, fallback);
    }

    const std::string key = toLower(mapName);
    auto it = mapRespawnDaysByFileName_.find(key);
    if (it != mapRespawnDaysByFileName_.end())
    {
        return std::max(0, it->second);
    }

    return std::max(0, fallback);
}

std::string Application::resolveMapDisplayName(const std::string& mapName) const
{
    if (mapName.empty() || mapDisplayNameByFileName_.empty())
    {
        return mapName;
    }

    const std::string key = toLower(mapName);
    if (auto it = mapDisplayNameByFileName_.find(key); it != mapDisplayNameByFileName_.end())
    {
        return it->second;
    }

    return mapName;
}

std::string Application::resolveSoundNameById(int soundId) const
{
    if (soundId <= 0)
    {
        return {};
    }

    if (auto it = soundNameById_.find(static_cast<uint32_t>(soundId)); it != soundNameById_.end())
    {
        return it->second;
    }
    return {};
}

void Application::playEventSound(int soundId)
{
    if (soundId <= 0)
    {
        return;
    }

    if (bootConfig_.noSound || !audioSystem_ || !audioSystem_->isInitialized())
    {
        logger.debug(std::format("Event PlaySound {} skipped (audio disabled)", soundId));
        return;
    }

    std::string soundName = resolveSoundNameById(soundId);
    if (soundName.empty())
    {
        logger.debug(std::format("Event PlaySound {} skipped (unknown sound id)", soundId));
        return;
    }

    const std::string lowerName = toLower(soundName);
    if (bootConfig_.noWalkSound && (lowerName.find("walk") != std::string::npos ||
                                    lowerName.find("step") != std::string::npos))
    {
        return;
    }

    // Lazy-load WAV payload from Audio.snd on first use.
    if (!loadedSounds_.contains(lowerName))
    {
        if (!sndArchive_ || !sndArchive_->isOpen())
        {
            logger.debug(
                std::format("Event PlaySound {} skipped (Audio.snd not mounted)", soundId));
            return;
        }

        std::vector<std::string> candidates;
        if (soundName.find('.') != std::string::npos)
        {
            candidates.push_back(soundName);
        }
        else
        {
            candidates.push_back(soundName + ".wav");
            candidates.push_back(soundName + ".WAV");
            candidates.push_back(soundName);
        }

        std::optional<std::vector<uint8_t>> wavData;
        for (const auto& candidate : candidates)
        {
            wavData = sndArchive_->extractFile(candidate);
            if (wavData.has_value())
            {
                break;
            }
        }

        if (!wavData.has_value() || wavData->empty())
        {
            logger.debug(std::format("Event PlaySound {} skipped (missing WAV for '{}')", soundId,
                                     soundName));
            return;
        }

        if (!audioSystem_->loadSound(lowerName, *wavData))
        {
            logger.debug(std::format("Event PlaySound {} skipped (failed to decode '{}')", soundId,
                                     soundName));
            return;
        }
        loadedSounds_.insert(lowerName);
    }

    if (audioSystem_->playSound(lowerName) < 0)
    {
        logger.debug(std::format("Event PlaySound {} failed to start '{}'", soundId, soundName));
    }
}

void Application::playUiSound(const std::string& soundName)
{
    if (bootConfig_.noSound || !audioSystem_ || !audioSystem_->isInitialized() || soundName.empty())
    {
        return;
    }

    const std::string lowerName = toLower(soundName);

    // Lazy-load WAV payload from Audio.snd on first use.
    if (!loadedSounds_.contains(lowerName))
    {
        if (!sndArchive_ || !sndArchive_->isOpen())
        {
            logger.debug(std::format("UI PlaySound skipped (Audio.snd not mounted)"));
            return;
        }

        std::vector<std::string> candidates;
        if (soundName.find('.') != std::string::npos)
        {
            candidates.push_back(soundName);
        }
        else
        {
            candidates.push_back(soundName + ".wav");
            candidates.push_back(soundName + ".WAV");
            candidates.push_back(soundName);
        }

        std::optional<std::vector<uint8_t>> wavData;
        for (const auto& candidate : candidates)
        {
            wavData = sndArchive_->extractFile(candidate);
            if (wavData.has_value())
            {
                break;
            }
        }

        if (!wavData.has_value() || wavData->empty())
        {
            logger.debug(std::format("UI PlaySound skipped (missing WAV for '{}')", soundName));
            return;
        }

        if (!audioSystem_->loadSound(lowerName, *wavData))
        {
            logger.debug(std::format("UI PlaySound skipped (failed to decode '{}')", soundName));
            return;
        }

        loadedSounds_.insert(lowerName);
    }

    if (audioSystem_->playSound(lowerName) < 0)
    {
        logger.debug(std::format("UI PlaySound failed to start '{}'", soundName));
    }
}

std::string Application::resolveBuildingDisplayName(int buildingId) const
{
    if (auto it = buildingDisplayNameById_.find(buildingId); it != buildingDisplayNameById_.end())
    {
        return it->second;
    }
    return {};
}

std::string Application::buildTransitionText(const std::string& targetMap, int direction) const
{
    if (targetMap.empty())
    {
        return {};
    }

    const std::string displayName = resolveMapDisplayName(targetMap);
    if (!mapDisplayNameByFileName_.empty() && displayName == targetMap)
    {
        logger.warning(std::format("No transition text found! target map='{}'", targetMap));
    }
    if (resolveSpawnIndexFromDirection(direction) == 0)
    {
        if (displayName == targetMap)
        {
            return std::format("Transition to {}", targetMap);
        }
        return std::format("Transition to {} ({})", displayName, targetMap);
    }

    if (displayName == targetMap)
    {
        return std::format("Transition to {} [{}]", targetMap, directionName(direction));
    }
    return std::format("Transition to {} ({}) [{}]", displayName, targetMap,
                       directionName(direction));
}

void Application::setDefaultStartMap(const std::string& mapName)
{
    if (mapName.empty())
    {
        return;
    }

    std::string resolvedName = mapName;
    std::string ext = getLowerExtension(resolvedName);
    if (ext.empty())
    {
        resolvedName += ".odm";
        ext = ".odm";
    }

    if (ext != ".odm" && ext != ".blv")
    {
        logger.warning(std::format("Ignoring invalid start map extension in '{}'", mapName));
        return;
    }

    defaultStartMapName_ = resolvedName;
    if (sharedData)
    {
        sharedData->newGameStartMapName = defaultStartMapName_;
    }
}

void Application::setBootConfig(const BootConfig& config)
{
    bool noLogoChanged = (bootConfig_.noLogo != config.noLogo);
    bootConfig_ = config;

    if (gameWorld_)
    {
        game::RuntimeConfig runtimeConfig = gameWorld_->runtimeConfig();
        runtimeConfig.noMonsters = config.noMonster;
        runtimeConfig.noDamage = config.noDamage;
        runtimeConfig.noDecorations = config.noDecoration;
        runtimeConfig.noSky = config.noSky;
        runtimeConfig.noWavyWater = config.noWavyWater;
        runtimeConfig.noMist = config.noMist;
        runtimeConfig.walkSpeed = std::max(1, config.walkSpeed);
        runtimeConfig.partyHeight = std::max(1, config.partyHeight);
        runtimeConfig.partyEyeLevel = std::max(0, config.partyEyeLevel);
        runtimeConfig.gridBand1 = std::max(1, config.gridBand1);
        runtimeConfig.gridBand2 = std::max(runtimeConfig.gridBand1, config.gridBand2);
        runtimeConfig.gridBand3 = std::max(runtimeConfig.gridBand2, config.gridBand3);
        runtimeConfig.terrainGamma = config.terrainGamma;
        runtimeConfig.buildingGamma = config.buildingGamma;
        runtimeConfig.distShade = std::max(0, config.distShade);
        runtimeConfig.distShadeMist = std::max(runtimeConfig.distShade, config.distShadeMist);
        runtimeConfig.distMist = std::max(runtimeConfig.distShadeMist, config.distMist);
        runtimeConfig.skyDayTop = config.skyDayTop;
        runtimeConfig.skyDayBottom = config.skyDayBottom;
        runtimeConfig.skyNightTop = config.skyNightTop;
        runtimeConfig.skyNightBottom = config.skyNightBottom;
        gameWorld_->setRuntimeConfig(runtimeConfig);
    }

    if (sharedData)
    {
        constexpr int kDefaultViewportX = 8;
        constexpr int kDefaultViewportY = 8;
        constexpr int kDefaultViewportWidth = 468;
        constexpr int kDefaultViewportHeight = 351;
        constexpr int kMinSafeViewportWidth = 64;
        constexpr int kMinSafeViewportHeight = 64;

        int worldVpX = std::clamp(config.viewportX, 0, kGameWidth - 1);
        int worldVpY = std::clamp(config.viewportY, 0, kGameHeight - 1);
        int worldVpW = std::clamp(config.viewportWidth, 1, kGameWidth);
        int worldVpH = std::clamp(config.viewportHeight, 1, kGameHeight);

        worldVpW = std::min(worldVpW, kGameWidth - worldVpX);
        worldVpH = std::min(worldVpH, kGameHeight - worldVpY);
        if (worldVpW < kMinSafeViewportWidth || worldVpH < kMinSafeViewportHeight)
        {
            logger.warning(std::format("Boot viewport {}x{} at ({},{}) is too small; restoring "
                                       "{}x{} at ({},{})",
                                       worldVpW, worldVpH, worldVpX, worldVpY,
                                       kDefaultViewportWidth, kDefaultViewportHeight,
                                       kDefaultViewportX, kDefaultViewportY));
            worldVpX = kDefaultViewportX;
            worldVpY = kDefaultViewportY;
            worldVpW = kDefaultViewportWidth;
            worldVpH = kDefaultViewportHeight;
        }

        sharedData->showFrameRate = config.showFr;
        sharedData->worldViewportX = worldVpX;
        sharedData->worldViewportY = worldVpY;
        sharedData->worldViewportWidth = worldVpW;
        sharedData->worldViewportHeight = worldVpH;
    }

    graphics::TerrainLOD::configureFromGridBands(config.gridBand1, config.gridBand2,
                                                 config.gridBand3);

    if (audioSystem_)
    {
        audioSystem_->setMaxChannels(config.mixerChannels);

        if (config.noSound)
        {
            audioSystem_->stopAll();
        }
        else if (!audioSystem_->isInitialized())
        {
            if (!audioSystem_->initialize())
            {
                logger.warning("Failed to initialize audio subsystem from boot config");
            }
        }
    }

    if (videoPlayer)
    {
        videoPlayer->setAudioEnabled(!config.noSound);
    }

    if (config.noSound)
    {
        logger.info("Boot config: audio disabled");
    }
    if (config.noWalkSound)
    {
        logger.info("Boot config: walk-step sounds disabled");
    }

    if (config.noAnim)
    {
        if (renderer)
        {
            for (void* tex : loadingFrames)
            {
                renderer->destroyTexture(tex);
            }
        }
        loadingFrames.clear();
        loadingFrameWidths.clear();
        loadingFrameHeights.clear();
        loadingFrameNumbers.clear();
        if (loadingState)
        {
            loadingState->setAnimationFrames(&loadingFrames, &loadingFrameWidths,
                                             &loadingFrameHeights, &loadingFrameNumbers);
        }
        logger.info("Boot config: loading animations disabled");
    }

    if (noLogoChanged && config.noLogo && !config.noIntro && !gameRoot.empty())
    {
        refreshIntroPlaylist();
        logger.info("Boot config: logo videos disabled");
    }

    if ((config.noIntro || config.noAnim) && activeStateId == GameStateId::IntroVideo)
    {
        logger.info("Boot config: skipping intro videos");
        transitionTo(GameStateId::TitleScreen);
    }

    if (config.bootCharCreate)
    {
        logger.info("Boot config: jumping to Create Party screen");
        transitionTo(GameStateId::CharacterCreation);
    }
}

void Application::shutdown()
{
    logger.info("Shutting down...");

    if (loadingThread.joinable())
    {
        loadingThread.join();
    }

    clearMapTextureCache();
    unloadUiAssets();
    if (videoPlayer)
    {
        videoPlayer->stop();
        videoPlayer.reset();
    }
    if (audioSystem_)
    {
        audioSystem_->shutdown();
    }
    if (sndArchive_)
    {
        sndArchive_->close();
    }
    loadedSounds_.clear();
    vfs->unmountAll();
    worldRenderer.reset();
    lineRenderer.reset();
    renderer.reset();
    window.shutdown();
    initialized = false;
    logger.info("Shutdown complete");
}
} // namespace runeharbor::engine
