// SPDX-License-Identifier: MIT
#include "application.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <vector>

#include <cctype>

#include "../formats/image_lod_archive.hpp"
#include "../formats/pcx_image.hpp"
#include "../formats/sprite_parser.hpp"
#include "../graphics/image.hpp"
#include "../graphics/line_renderer.hpp"
#include "../graphics/palette.hpp"
#include "../graphics/sdl_renderer.hpp"
#include "../graphics/world_renderer.hpp"
#include "../media/vid_archive.hpp"
#include "../media/vid_manifest.hpp"
#include "../media/video_player.hpp"
#include "../platform/iwindow.hpp"
#include "../util/ilogger.hpp"
#include "states/character_creation_state.hpp"
#include "states/ingame_state.hpp"
#include "states/intro_state.hpp"
#include "states/loading_state.hpp"
#include "states/state_context.hpp"
#include "states/title_state.hpp"
#include "virtual_filesystem.hpp"

namespace runeharbor::engine
{

Application::Application(util::ILogger& logger, platform::IWindow& window)
    : logger(logger), window(window), vfs(std::make_unique<VirtualFileSystem>(logger))
{
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

// Race base stats: [race][stat] order: Might, Intellect, Personality, Endurance, Speed, Accuracy,
// Luck
constexpr int kRaceBaseStats[4][7] = {
    {11, 11, 11, 9, 11, 11, 9}, // Human
    {7, 14, 11, 7, 11, 14, 9},  // Elf
    {14, 11, 11, 14, 7, 7, 9},  // Dwarf
    {14, 7, 7, 11, 14, 11, 9},  // Goblin
};

// Face-to-race: 0-7=Human, 8-11=Elf, 12-15=Dwarf, 16-19=Goblin
Race raceFromFace(int faceId)
{
    if (faceId < 8)
        return Race::Human;
    if (faceId < 12)
        return Race::Elf;
    if (faceId < 16)
        return Race::Dwarf;
    return Race::Goblin;
}

// Starting skills per class (indexed by CharacterClass enum order)
struct ClassSkills
{
    const char* skill1;
    const char* skill2;
};

constexpr ClassSkills kClassStartingSkills[] = {
    {"Sword", "Leather Armor"}, // Knight
    {"Mace", "Spirit Magic"},   // Paladin
    {"Bow", "Air Magic"},       // Archer
    {"Mace", "Body Magic"},     // Cleric
    {"Staff", "Fire Magic"},    // Sorcerer
    {"Dagger", "Stealing"},     // Thief
    {"Dodging", "Unarmed"},     // Monk
    {"Axe", "Perception"},      // Ranger
    {"Dagger", "Earth Magic"},  // Druid
};
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
    videoPlayer = std::make_unique<media::VideoPlayer>();
    updateViewport();

    logger.info("Press ESC or close window to exit");

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
    case GameStateId::Quit:
    {
        SDL_Event quitEvent = {};
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
        return;
    }
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

    // Text/data archives
    const std::vector<std::string> textArchives = {
        "Events.lod",
    };

    // Image archives (use different format)
    const std::vector<std::string> imageArchives = {
        "BITMAPS.LOD",
        "ICONS.LOD",
        "SPRITES.LOD",
    };

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

    gameDataLoaded = true;
    buildIntroPlaylist();
    if (introState)
    {
        introState->setPlaylist(introPlaylist);
    }
    loadUiAssets();
    if (gameState == GameState::IntroVideo && videoPlayer && !introPlaylist.empty())
    {
        videoPlayer->setPlaylist(introPlaylist);
        videoPlayer->start(SDL_GetTicks());
    }
    return true;
}

void Application::configureBootFlow(const std::string& mapName, bool preferOutdoor, bool autoLoad)
{
    startupMapName = mapName;
    startupPreferOutdoor = preferOutdoor;
    autoLoadMap = autoLoad;

    // Sync to shared data
    if (sharedData)
    {
        sharedData->startupMapName = mapName;
        sharedData->startupPreferOutdoor = preferOutdoor;
        sharedData->autoLoadMap = autoLoad;
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
    logger.info("Camera controls: Arrow keys orbit, Q/E zoom, Shift to speed up");
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

    auto progress = [this](float value) { updateLoadProgress(0.1f + value * 0.85f); };
    if (!mapScene->loadODM(resolvedName, *data, progress))
    {
        logger.error(std::format("Failed to parse ODM map: {}", resolvedName));
        return false;
    }

    mapLoaded = true;
    configureCameraForMap();

    logger.info(std::format("Loaded ODM map: {}", resolvedName));
    logger.info("Camera controls: Arrow keys orbit, Q/E zoom, WASD pan, Shift to speed up");
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

    renderer->present();
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
    if (!mapScene || !mapScene->isLoaded())
    {
        return;
    }

    const auto& bounds = mapScene->getBounds();
    if (!bounds.valid)
    {
        return;
    }

    float distance = std::max(bounds.radius() * 2.5f, 1000.0f);
    camera.lookAt(bounds.center(), distance);
}

void Application::wireUpMapTextures()
{
    if (!worldRenderer || !vfs || !renderer)
    {
        return;
    }

    clearMapTextureCache();

    worldRenderer->setTextureLookup(
        [this](const std::string& name) -> SDL_Texture*
        {
            // Check cache first
            auto it = mapTextureCache.find(name);
            if (it != mapTextureCache.end())
            {
                return static_cast<SDL_Texture*>(it->second);
            }

            // Get image info (dimensions, palette ID)
            auto info = vfs->getImageInfo(name);
            if (!info)
            {
                return nullptr;
            }

            // Get raw indexed pixel data
            auto data = vfs->readFile(name);
            if (!data)
            {
                return nullptr;
            }

            // Load palette
            graphics::Palette palette;
            int palId = info->paletteId;
            if (palId == 0)
            {
                palId = 1; // PAL000 doesn't exist; use PAL001 as fallback
            }
            std::string palName = std::format("pal{:03d}", palId);
            auto palData = vfs->readFile(palName);
            if (palData && palData->size() >= 768)
            {
                std::vector<uint8_t> rgb;
                if (palData->size() > 768)
                {
                    rgb.assign(palData->end() - 768, palData->end());
                }
                else
                {
                    rgb = *palData;
                }
                palette = graphics::Palette::fromRGBData(rgb);
            }
            else
            {
                palette = graphics::Palette::createDefaultPalette();
            }

            // Convert indexed data to RGBA image
            auto image =
                graphics::Image::fromPalettedData(*data, info->width, info->height, palette);
            if (!image)
            {
                return nullptr;
            }

            // Create GPU texture
            void* tex = renderer->createTexture(*image);
            if (tex)
            {
                mapTextureCache[name] = tex;
            }
            return static_cast<SDL_Texture*>(tex);
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
        introPlaylist.push_back({"Intro", 2500});
        return;
    }

    // Load the VID archive into the video player
    if (videoPlayer && !vidPath.empty())
    {
        videoPlayer->loadArchive(vidPath);
    }

    // Build a filtered intro list (logos + intro)
    const std::vector<std::string> preferred = {
        "3DOLOGO.SMK", "JVC.BIK", "NEW WORLD LOGO.BIK", "INTRO.BIK", "INTRO POST.BIK",
    };

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

    for (const auto& want : preferred)
    {
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
            introPlaylist.push_back({clip.name, 2500});
            if (introPlaylist.size() >= 3)
            {
                break;
            }
        }
    }
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

    // Extract palette from MAKEME.PCX for use with paletted textures that have paletteId=0
    // In the original VGA engine, the background PCX set the shared screen palette
    for (const auto& pcxName : {"makeme.pcx", "MAKEME.PCX", "Create.pcx", "CREATE.PCX"})
    {
        auto pcxData = vfs->readFile(pcxName);
        if (!pcxData.has_value())
        {
            continue;
        }
        auto pcx = formats::decodePCX(*pcxData, logger);
        if (pcx.has_value() && !pcx->is24Bit())
        {
            screenPaletteRGB.resize(768);
            for (int i = 0; i < 256; i++)
            {
                auto c = pcx->palette.getColor(static_cast<uint8_t>(i));
                screenPaletteRGB[i * 3] = c.r;
                screenPaletteRGB[i * 3 + 1] = c.g;
                screenPaletteRGB[i * 3 + 2] = c.b;
            }
            logger.info(std::format("Extracted screen palette from {} ({} colors)", pcxName, 256));
            break;
        }
    }

    loadPcxSequence("loading", loadingFrames, loadingFrameWidths, loadingFrameHeights);
    loadPcxTexture({"loading.pcx", "Loading.pcx", "LOADING.PCX"}, "Loading", loadingBackground,
                   loadingBackgroundWidth, loadingBackgroundHeight);

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
    }
    if (loadingState)
    {
        loadingState->setBackground(loadingBackground, loadingBackgroundWidth,
                                    loadingBackgroundHeight);
        loadingState->setFallbackBackground(titleBackground, titleBackgroundWidth,
                                            titleBackgroundHeight);
        loadingState->setAnimationFrames(&loadingFrames, &loadingFrameWidths, &loadingFrameHeights);
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
            try
            {
                std::unique_ptr<graphics::Image> image;
                if (pcx->is24Bit())
                {
                    image = graphics::Image::fromRGBAData(pcx->rgbaPixels, pcx->width, pcx->height);
                }
                else
                {
                    image = graphics::Image::fromPalettedData(pcx->indices, pcx->width, pcx->height,
                                                              pcx->palette);
                }
                if (image)
                {
                    void* tex = renderer->createTexture(*image);
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
            }
            catch (const std::exception& ex)
            {
                logger.warning(std::format("Failed to convert PCX '{}': {}", name, ex.what()));
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
        try
        {
            auto image = graphics::Image::fromPalettedData(frame.data, frame.width, frame.height,
                                                           sprite.palette);
            if (image)
            {
                void* tex = renderer->createTexture(*image);
                if (tex)
                {
                    renderer->destroyTexture(textureHandle);
                    textureHandle = tex;
                    width = static_cast<int>(frame.width);
                    height = static_cast<int>(frame.height);
                    logger.info(std::format("Loaded UI texture '{}' as sprite: {} ({}x{})", label,
                                            name, width, height));
                    return true;
                }
            }
        }
        catch (const std::exception& e)
        {
            logger.warning(
                std::format("Failed to convert sprite frame to image'{}': {}", name, e.what()));
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

        // Load palette
        // paletteId=0 means "use the active screen palette" (VGA-era concept)
        std::optional<std::vector<uint8_t>> palData;
        if (imgInfo->paletteId == 0 && !screenPaletteRGB.empty())
        {
            palData = screenPaletteRGB;
        }
        else
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

        try
        {
            auto palette = graphics::Palette::fromRGBData(*palData);
            // Index 0 is transparent
            palette.setColor(0, graphics::Palette::Color(0, 0, 0, 0));

            auto image = graphics::Image::fromPalettedData(*pixelData, imgInfo->width,
                                                           imgInfo->height, palette);
            if (image)
            {
                void* tex = renderer->createTexture(*image);
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
        catch (const std::exception& ex)
        {
            logger.warning(
                std::format("Failed to convert paletted image '{}': {}", name, ex.what()));
        }
    }

    logger.warning(std::format("Failed to load texture '{}' from any candidate", label));
    return false;
}

bool Application::loadPcxSequence(const std::string& prefix, std::vector<void*>& textures,
                                  std::vector<int>& widths, std::vector<int>& heights)
{
    textures.clear();
    widths.clear();
    heights.clear();

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

        try
        {
            std::unique_ptr<graphics::Image> image;
            if (pcx->is24Bit())
            {
                image = graphics::Image::fromRGBAData(pcx->rgbaPixels, pcx->width, pcx->height);
            }
            else
            {
                image = graphics::Image::fromPalettedData(pcx->indices, pcx->width, pcx->height,
                                                          pcx->palette);
            }
            if (!image)
            {
                continue;
            }

            void* tex = renderer->createTexture(*image);
            if (!tex)
            {
                continue;
            }

            textures.push_back(tex);
            widths.push_back(static_cast<int>(pcx->width));
            heights.push_back(static_cast<int>(pcx->height));
        }
        catch (const std::exception& ex)
        {
            logger.warning(std::format("Failed to convert PCX '{}': {}", entry.name, ex.what()));
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
        mapLoaded = true;
        configureCameraForMap();
        wireUpMapTextures();
        setGameState(GameState::InGame);
    }
    else
    {
        logger.error(std::format("finalizeLoadingTask: failed, error='{}', hasScene={}",
                                 error, loadedScene != nullptr));
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
        if (request.preferOutdoor)
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

    party[1].name = "Roderick";
    party[1].faceId = 3;
    party[1].charClass = CharacterClass::Thief;
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
    Race race = raceFromFace(ch.faceId);
    int raceIdx = static_cast<int>(race);
    for (int i = 0; i < 7; i++)
    {
        ch.baseStats.byIndex(i) = kRaceBaseStats[raceIdx][i];
    }
    ch.stats = ch.baseStats;
}

void Application::updateSkillsForClass(Character& ch)
{
    int classIdx = static_cast<int>(ch.charClass);
    ch.skills.clear();
    ch.skills.push_back(kClassStartingSkills[classIdx].skill1);
    ch.skills.push_back(kClassStartingSkills[classIdx].skill2);
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
    vfs->unmountAll();
    renderer.reset();
    window.shutdown();
    initialized = false;
    logger.info("Shutdown complete");
}
} // namespace runeharbor::engine