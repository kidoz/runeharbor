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
#include "../platform/iwindow.hpp"
#include "../util/ilogger.hpp"
#include "../media/vid_archive.hpp"
#include "../media/vid_manifest.hpp"
#include "../media/video_player.hpp"
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

std::string toUpper(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
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

const char* onOff(bool value)
{
    return value ? "ON" : "OFF";
}

constexpr int kGameWidth = 640;
constexpr int kGameHeight = 480;

const std::vector<std::string> kTitleMenuItems = {
    "NEW",
    "LOAD",
    "CREDITS",
    "EXIT",
};

const std::vector<std::string> kRaceNames = {
    "Human", "Elf", "Dwarf", "Goblin"
};

const std::vector<std::string> kGenderNames = {
    "Male", "Female"
};

const std::vector<std::string> kClassNames = {
    "Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Thief", "Monk", "Ranger", "Druid"
};

const std::vector<std::string> kStatNames = {
    "Might", "Intellect", "Personality", "Endurance", "Speed", "Accuracy", "Luck"
};

// Race base stats: [race][stat] order: Might, Intellect, Personality, Endurance, Speed, Accuracy, Luck
constexpr int kRaceBaseStats[4][7] = {
    {11, 11, 11,  9, 11, 11, 9}, // Human
    { 7, 14, 11,  7, 11, 14, 9}, // Elf
    {14, 11, 11, 14,  7,  7, 9}, // Dwarf
    {14,  7,  7, 11, 14, 11, 9}, // Goblin
};

// Race stat maximums
constexpr int kRaceStatMax[4][7] = {
    {25, 25, 25, 25, 25, 25, 25}, // Human
    {15, 30, 25, 15, 25, 30, 20}, // Elf
    {30, 25, 25, 30, 15, 15, 20}, // Dwarf
    {30, 15, 15, 25, 30, 25, 20}, // Goblin
};

// Face-to-race: 0-7=Human, 8-11=Elf, 12-15=Dwarf, 16-19=Goblin
Race raceFromFace(int faceId)
{
    if (faceId < 8) return Race::Human;
    if (faceId < 12) return Race::Elf;
    if (faceId < 16) return Race::Dwarf;
    return Race::Goblin;
}

// Face-to-gender: first half of each race group is male, second half female
Gender genderFromFace(int faceId)
{
    int raceStart = 0;
    int raceCount = 8;
    if (faceId >= 16) { raceStart = 16; raceCount = 4; }
    else if (faceId >= 12) { raceStart = 12; raceCount = 4; }
    else if (faceId >= 8) { raceStart = 8; raceCount = 4; }
    int offset = faceId - raceStart;
    return offset < raceCount / 2 ? Gender::Male : Gender::Female;
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

    // Skip IntroVideo for now as decoders need more work
    setGameState(GameState::TitleScreen);
    initialized = true;
    return true;
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

    // If map name is specified via CLI, skip menu and go directly to loading
    if (!mapName.empty() && autoLoad && (gameState == GameState::TitleScreen || gameState == GameState::IntroVideo))
    {
        quickStartReady = true;
        setGameState(GameState::Loading);
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

    switch (gameState)
    {
    case GameState::IntroVideo:
        renderIntroVideo();
        break;
    case GameState::TitleScreen:
        renderTitleScreen();
        break;
    case GameState::CharacterCreation:
        renderCharacterCreation();
        break;
    case GameState::Loading:
        renderLoadingScreen();
        break;
    case GameState::InGame:
        if (mapLoaded && mapScene && worldRenderer)
        {
            worldRenderer->render(*mapScene, camera);
        }
        break;
    }

    renderOverlay();

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

        // Re-layout menu buttons on viewport change
        if (!titleMenuUI.buttons.empty())
        {
            layoutTitleMenuButtons();
        }
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

void Application::updateCameraInput()
{
    if (!mapLoaded || gameState != GameState::InGame)
    {
        return;
    }

    float orbitSpeed = 0.015f;
    float zoomSpeed = 50.0f;
    float panSpeed = 8.0f;

    if (isKeyDown(SDL_SCANCODE_LSHIFT) || isKeyDown(SDL_SCANCODE_RSHIFT))
    {
        orbitSpeed *= 2.0f;
        zoomSpeed *= 2.0f;
        panSpeed *= 2.0f;
    }

    if (isKeyDown(SDL_SCANCODE_LEFT))
    {
        camera.orbit(-orbitSpeed, 0.0f);
    }
    if (isKeyDown(SDL_SCANCODE_RIGHT))
    {
        camera.orbit(orbitSpeed, 0.0f);
    }
    if (isKeyDown(SDL_SCANCODE_UP))
    {
        camera.orbit(0.0f, orbitSpeed);
    }
    if (isKeyDown(SDL_SCANCODE_DOWN))
    {
        camera.orbit(0.0f, -orbitSpeed);
    }

    if (isKeyDown(SDL_SCANCODE_Q))
    {
        camera.zoom(zoomSpeed);
    }
    if (isKeyDown(SDL_SCANCODE_E))
    {
        camera.zoom(-zoomSpeed);
    }

    if (isKeyDown(SDL_SCANCODE_A))
    {
        camera.pan(-panSpeed, 0.0f);
    }
    if (isKeyDown(SDL_SCANCODE_D))
    {
        camera.pan(panSpeed, 0.0f);
    }
    if (isKeyDown(SDL_SCANCODE_W))
    {
        camera.pan(0.0f, panSpeed);
    }
    if (isKeyDown(SDL_SCANCODE_S))
    {
        camera.pan(0.0f, -panSpeed);
    }
}

void Application::setGameState(GameState state)
{
    gameState = state;
    stateStartTicks = SDL_GetTicks();
    loadingStarted = false;
    if (state == GameState::TitleScreen)
    {
        titleMenuIndex = 0;
        stateMessage.clear();
        startupMapName.clear();
        startupPreferOutdoor = false;
        titleMenuUI.buttons.clear(); // Re-layout on next frame
    }
    else if (state == GameState::CharacterCreation)
    {
        characterMenuIndex = 0;
        stateMessage.clear();
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

    if (gameState == GameState::IntroVideo && videoPlayer && !introPlaylist.empty())
    {
        videoPlayer->setPlaylist(introPlaylist);
        videoPlayer->start(stateStartTicks);
    }
}

void Application::updateStateMachine()
{
    pollKeyboardState();

    const uint64_t now = SDL_GetTicks();
    switch (gameState)
    {
    case GameState::IntroVideo:
        if (videoPlayer)
        {
            videoPlayer->update(now);
            if (videoPlayer->isFinished())
            {
                setGameState(GameState::TitleScreen);
                break;
            }
        }

        if (isKeyPressed(SDL_SCANCODE_RETURN) || isKeyPressed(SDL_SCANCODE_SPACE))
        {
            setGameState(GameState::TitleScreen);
        }
        break;
    case GameState::TitleScreen:
        // Ensure buttons are laid out
        if (titleMenuUI.buttons.empty())
        {
            layoutTitleMenuButtons();
        }

        // Update hover state from mouse position
        updateTitleMenuHover();

        // Keyboard navigation
        if (isKeyPressed(SDL_SCANCODE_UP))
        {
            titleMenuIndex = (titleMenuIndex + static_cast<int>(kTitleMenuItems.size()) - 1) %
                             static_cast<int>(kTitleMenuItems.size());
            titleMenuUI.selectedIndex = titleMenuIndex;
        }
        if (isKeyPressed(SDL_SCANCODE_DOWN))
        {
            titleMenuIndex = (titleMenuIndex + 1) % static_cast<int>(kTitleMenuItems.size());
            titleMenuUI.selectedIndex = titleMenuIndex;
        }

        // Handle selection (keyboard Enter or mouse click)
        {
            bool activated = isKeyPressed(SDL_SCANCODE_RETURN);

            // Check for mouse click on any button
            if (!activated && window.wasMousePressed(platform::MouseButton::Left))
            {
                for (size_t i = 0; i < titleMenuUI.buttons.size(); i++)
                {
                    if (titleMenuUI.buttons[i].isHovered)
                    {
                        titleMenuIndex = static_cast<int>(i);
                        titleMenuUI.selectedIndex = titleMenuIndex;
                        activated = true;
                        break;
                    }
                }
            }

            if (activated)
            {
                if (titleMenuIndex == 0)
                {
                    // NEW GAME
                    quickStartReady = false;
                    setGameState(GameState::CharacterCreation);
                }
                else if (titleMenuIndex == 1)
                {
                    // LOAD GAME
                    stateMessage = "Load game not implemented yet";
                }
                else if (titleMenuIndex == 2)
                {
                    // CREDITS
                    stateMessage = "Credits not implemented yet";
                }
                else if (titleMenuIndex == 3)
                {
                    // EXIT GAME
                    SDL_Event quitEvent = {};
                    quitEvent.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quitEvent);
                }
            }
        }

        // Keyboard shortcuts
        if (isKeyPressed(SDL_SCANCODE_N))
        {
            quickStartReady = false;
            setGameState(GameState::CharacterCreation);
        }
        else if (isKeyPressed(SDL_SCANCODE_L))
        {
            stateMessage = "Load game not implemented yet";
        }
        else if (isKeyPressed(SDL_SCANCODE_C))
        {
            stateMessage = "Credits not implemented yet";
        }
        else if (isKeyPressed(SDL_SCANCODE_Q) || isKeyPressed(SDL_SCANCODE_E))
        {
            SDL_Event quitEvent = {};
            quitEvent.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quitEvent);
        }
        break;
    case GameState::CharacterCreation:
        {
            // Select active character with 1-4
            if (isKeyPressed(SDL_SCANCODE_1)) activeCharacterIndex = 0;
            if (isKeyPressed(SDL_SCANCODE_2)) activeCharacterIndex = 1;
            if (isKeyPressed(SDL_SCANCODE_3)) activeCharacterIndex = 2;
            if (isKeyPressed(SDL_SCANCODE_4)) activeCharacterIndex = 3;

            // Mouse click: column selection + bottom buttons
            if (window.wasMousePressed(platform::MouseButton::Left))
            {
                auto mouseState = window.getMouseState();
                int gameX = unscaleX(mouseState.x);
                int gameY = unscaleY(mouseState.y);

                // Column click detection (4 columns)
                constexpr int colX[] = {10, 168, 326, 484};
                constexpr int colWidth = 155;
                for (int i = 0; i < 4; i++)
                {
                    if (gameX >= colX[i] && gameX < colX[i] + colWidth &&
                        gameY >= 30 && gameY < 420)
                    {
                        activeCharacterIndex = i;
                        break;
                    }
                }

                // OK button (game coords ~560-610, 440-460)
                if (gameX >= 560 && gameX <= 620 && gameY >= 440 && gameY <= 465)
                {
                    quickStartReady = true;
                    startupMapName = "out01.odm";
                    startupPreferOutdoor = true;
                    setGameState(GameState::Loading);
                }
                // CLEAR button (game coords ~490-550, 440-460)
                else if (gameX >= 490 && gameX <= 555 && gameY >= 440 && gameY <= 465)
                {
                    Character& ch = party[activeCharacterIndex];
                    ch.stats = ch.baseStats;
                }
            }

            Character& activeChar = party[activeCharacterIndex];

            // Naming mode: capture text input
            if (isNaming)
            {
                if (isKeyPressed(SDL_SCANCODE_BACKSPACE) && !activeChar.name.empty())
                {
                    activeChar.name.pop_back();
                }
                else if (isKeyPressed(SDL_SCANCODE_RETURN) || isKeyPressed(SDL_SCANCODE_ESCAPE))
                {
                    isNaming = false;
                }
                for (int i = SDL_SCANCODE_A; i <= SDL_SCANCODE_Z; i++)
                {
                    if (isKeyPressed(static_cast<SDL_Scancode>(i)))
                    {
                        if (activeChar.name.size() < 15)
                        {
                            char c = 'A' + (i - SDL_SCANCODE_A);
                            activeChar.name += c;
                        }
                    }
                }
                break;
            }

            // Row navigation: UP/DOWN through 10 rows (NAME, FACE, CLASS, 7 stats)
            if (isKeyPressed(SDL_SCANCODE_UP))
            {
                characterMenuIndex =
                    (characterMenuIndex + kCharCreationRowCount - 1) % kCharCreationRowCount;
            }
            if (isKeyPressed(SDL_SCANCODE_DOWN))
            {
                characterMenuIndex =
                    (characterMenuIndex + 1) % kCharCreationRowCount;
            }

            // Horizontal navigation
            int hDelta = 0;
            if (isKeyPressed(SDL_SCANCODE_LEFT)) hDelta = -1;
            if (isKeyPressed(SDL_SCANCODE_RIGHT)) hDelta = 1;

            if (hDelta != 0)
            {
                if (characterMenuIndex == 1) // FACE
                {
                    int oldRace = static_cast<int>(raceFromFace(activeChar.faceId));
                    activeChar.faceId = (activeChar.faceId + hDelta + 20) % 20;
                    int newRace = static_cast<int>(raceFromFace(activeChar.faceId));
                    if (oldRace != newRace)
                    {
                        updateCharacterForFace(activeChar);
                    }
                }
                else if (characterMenuIndex == 2) // CLASS
                {
                    int c = static_cast<int>(activeChar.charClass);
                    c = (c + hDelta + static_cast<int>(kClassNames.size())) %
                        static_cast<int>(kClassNames.size());
                    activeChar.charClass = static_cast<CharacterClass>(c);
                    updateSkillsForClass(activeChar);
                }
                else if (characterMenuIndex >= 3 && characterMenuIndex <= 9) // STATS
                {
                    int statIdx = characterMenuIndex - 3;
                    Race race = raceFromFace(activeChar.faceId);
                    int raceIdx = static_cast<int>(race);
                    int minVal = activeChar.baseStats.byIndex(statIdx) - 2;
                    int maxVal = kRaceStatMax[raceIdx][statIdx];

                    if (hDelta > 0 && calculateBonusPointsRemaining() <= 0)
                    {
                        // No bonus points left
                    }
                    else
                    {
                        int& stat = activeChar.stats.byIndex(statIdx);
                        stat = std::clamp(stat + hDelta, minVal, maxVal);
                    }
                }
            }

            // Enter key actions
            if (isKeyPressed(SDL_SCANCODE_RETURN))
            {
                if (characterMenuIndex == 0) // NAME
                {
                    isNaming = true;
                }
            }

            // ESC to go back
            if (isKeyPressed(SDL_SCANCODE_ESCAPE))
            {
                setGameState(GameState::TitleScreen);
            }
        }
        break;
    case GameState::Loading:
        if (!autoLoadMap && !loadingStarted)
        {
            if (isKeyPressed(SDL_SCANCODE_RETURN))
            {
                loadingStarted = true;
            }
            else
            {
                break;
            }
        }

        if (!loadingStarted)
        {
            loadingStarted = true;
        }

        if (loadingStarted)
        {
            if (!loadingTaskActive.load())
            {
                startLoadingTask();
            }
            else if (loadingTaskDone.load())
            {
                finalizeLoadingTask();
            }
        }
        break;
    case GameState::InGame:
        updateCameraInput();
        if (isKeyPressed(SDL_SCANCODE_F))
        {
            mapRenderOptions.showFloors = !mapRenderOptions.showFloors;
        }
        if (isKeyPressed(SDL_SCANCODE_V))
        {
            mapRenderOptions.showWalls = !mapRenderOptions.showWalls;
        }
        if (isKeyPressed(SDL_SCANCODE_C))
        {
            mapRenderOptions.showCeilings = !mapRenderOptions.showCeilings;
        }
        if (isKeyPressed(SDL_SCANCODE_P))
        {
            mapRenderOptions.showPortals = !mapRenderOptions.showPortals;
        }
        if (isKeyPressed(SDL_SCANCODE_L))
        {
            mapRenderOptions.showLights = !mapRenderOptions.showLights;
        }
        if (isKeyPressed(SDL_SCANCODE_G))
        {
            showGrid = !showGrid;
        }
        if (isKeyPressed(SDL_SCANCODE_X))
        {
            showAxes = !showAxes;
        }
        if (isKeyPressed(SDL_SCANCODE_H))
        {
            showHelpOverlay = !showHelpOverlay;
        }
        if (isKeyPressed(SDL_SCANCODE_R))
        {
            configureCameraForMap();
        }
        break;
    }

    commitKeyboardState();
}

void Application::renderIntroVideo()
{
    if (!videoPlayer || !renderer)
    {
        return;
    }

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    if (!sdlRenderer)
    {
        return;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        return;
    }

    videoPlayer->render(sdlRenderer, debugText.get(), viewportWidth, viewportHeight);
}



void Application::renderTitleScreen()
{
    if (!renderer)
    {
        return;
    }

    if (titleBackground)
    {
        renderFullscreenTexture(titleBackground, titleBackgroundWidth, titleBackgroundHeight);
    }

    if (!debugText || !renderer->getSDLRenderer())
    {
        return;
    }

    // Render hover textures for buttons
    for (const auto& button : titleMenuUI.buttons)
    {
        if (button.isHovered && button.hoverTexture)
        {
            renderer->renderTexture(button.hoverTexture, button.bounds.x, button.bounds.y,
                                    button.bounds.width, button.bounds.height);
        }
    }

    // Render any state message
    if (!stateMessage.empty())
    {
        int scale = 2;
        int x = 40;
        int y = viewportHeight > 0 ? viewportHeight - 60 : 520;
        debugText->drawText(renderer->getSDLRenderer(), x, y, scale, 255, 220, 80, stateMessage);
    }
}

void Application::renderCharacterCreation()
{
    if (!renderer)
    {
        return;
    }

    if (createBackground)
    {
        renderFullscreenTexture(createBackground, createBackgroundWidth, createBackgroundHeight);
    }
    else if (titleBackground)
    {
        renderFullscreenTexture(titleBackground, titleBackgroundWidth, titleBackgroundHeight);
    }

    if (!debugText || !renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    // Compute text scale from viewport
    float gameScale = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    int textScale = std::max(1, static_cast<int>(gameScale));

    // 4-column layout (game coords)
    constexpr int colX[] = {10, 168, 326, 484};
    constexpr int colWidth = 155;

    for (int c = 0; c < 4; c++)
    {
        const Character& ch = party[c];
        bool isActive = (c == activeCharacterIndex);

        int sx = scaleX(colX[c]);

        // Highlight active column
        if (isActive)
        {
            SDL_FRect highlight = {
                static_cast<float>(sx),
                static_cast<float>(scaleY(30)),
                static_cast<float>(scaleW(colWidth)),
                static_cast<float>(scaleH(420)),
            };
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 100, 30);
            SDL_RenderFillRect(sdlRenderer, &highlight);
        }

        // Portrait
        int portraitY = scaleY(35);
        if (ch.faceId >= 0 && ch.faceId < kPortraitCount && portraitTextures[ch.faceId])
        {
            int pw = scaleW(std::min(portraitWidths[ch.faceId], colWidth - 10));
            int ph = scaleH(portraitHeights[ch.faceId]);
            int px = sx + scaleW(colWidth / 2) - pw / 2;
            renderer->renderTexture(portraitTextures[ch.faceId], px, portraitY, pw, ph);
        }
        else
        {
            debugText->drawText(sdlRenderer, sx + scaleW(20), portraitY + scaleH(30),
                                textScale, 100, 100, 100,
                                std::format("[Face {}]", ch.faceId + 1));
        }

        // Face navigation arrows (active character, FACE row selected)
        if (isActive && characterMenuIndex == 1)
        {
            debugText->drawText(sdlRenderer, sx, portraitY, textScale, 255, 255, 0, "<");
            debugText->drawText(sdlRenderer, sx + scaleW(colWidth - 12), portraitY,
                                textScale, 255, 255, 0, ">");
        }

        // Name
        int nameY = scaleY(130);
        uint8_t nr = 200, ng = 200, nb = 200;
        if (isActive && characterMenuIndex == 0)
        {
            nr = 255; ng = 255; nb = 0;
        }
        std::string nameStr = ch.name;
        if (isActive && isNaming) nameStr += "_";
        debugText->drawText(sdlRenderer, sx, nameY, textScale, nr, ng, nb, nameStr);

        // Race + Gender
        Race race = raceFromFace(ch.faceId);
        Gender gender = genderFromFace(ch.faceId);
        std::string raceGender = kRaceNames[static_cast<int>(race)] + " " +
                                 kGenderNames[static_cast<int>(gender)];
        debugText->drawText(sdlRenderer, sx, scaleY(148), textScale, 180, 180, 180, raceGender);

        // Class
        uint8_t cr = 200, cg = 200, cb = 200;
        if (isActive && characterMenuIndex == 2)
        {
            cr = 255; cg = 255; cb = 0;
        }
        std::string classStr = kClassNames[static_cast<int>(ch.charClass)];
        if (isActive && characterMenuIndex == 2) classStr = "< " + classStr + " >";
        debugText->drawText(sdlRenderer, sx, scaleY(166), textScale, cr, cg, cb, classStr);

        // Stats
        for (int s = 0; s < 7; s++)
        {
            int statY = scaleY(195 + s * 18);
            uint8_t sr = 180, sg = 180, sb = 180;
            if (isActive && characterMenuIndex == 3 + s)
            {
                sr = 255; sg = 255; sb = 0;
            }

            std::string marker = (isActive && characterMenuIndex == 3 + s) ? "> " : "  ";
            std::string statLine = std::format("{}{}:{}", marker, kStatNames[s].substr(0, 3),
                                               ch.stats.byIndex(s));
            debugText->drawText(sdlRenderer, sx, statY, textScale, sr, sg, sb, statLine);
        }

        // Skills
        int skillY = scaleY(325);
        for (const auto& skill : ch.skills)
        {
            debugText->drawText(sdlRenderer, sx, skillY, textScale, 150, 200, 150, skill);
            skillY += debugText->lineHeight(textScale);
        }
    }

    // Bottom controls
    int bonusPoints = calculateBonusPointsRemaining();
    uint8_t bonusR = bonusPoints > 0 ? 255 : 100;
    uint8_t bonusG = bonusPoints > 0 ? 230 : 255;
    uint8_t bonusB = 150;
    debugText->drawText(sdlRenderer, scaleX(20), scaleY(440), textScale,
                        bonusR, bonusG, bonusB,
                        std::format("BONUS: {}", bonusPoints));

    debugText->drawText(sdlRenderer, scaleX(560), scaleY(440), textScale,
                        200, 255, 200, "OK");

    debugText->drawText(sdlRenderer, scaleX(490), scaleY(440), textScale,
                        255, 200, 200, "CLEAR");

    // Help text
    debugText->drawText(sdlRenderer, scaleX(20), scaleY(460),
                        std::max(1, textScale - 1), 150, 150, 150,
                        "1-4:Select  Arrows:Navigate  Enter:Edit name  ESC:Back");
}

void Application::renderLoadingScreen()
{
    if (!renderer)
    {
        return;
    }

    if (!loadingFrames.empty())
    {
        size_t frameIndex = 0;
        if (loadProgressActive.load() && loadingFrames.size() > 1)
        {
            float progress = std::clamp(loadProgress.load(), 0.0f, 1.0f);
            frameIndex =
                static_cast<size_t>(progress * static_cast<float>(loadingFrames.size() - 1));
        }
        else if (loadingFrameDurationMs > 0)
        {
            uint64_t now = SDL_GetTicks();
            frameIndex = static_cast<size_t>((now - stateStartTicks) / loadingFrameDurationMs) %
                         loadingFrames.size();
        }
        renderFullscreenTexture(loadingFrames[frameIndex], loadingFrameWidths[frameIndex],
                                loadingFrameHeights[frameIndex]);
    }
    else if (loadingBackground)
    {
        renderFullscreenTexture(loadingBackground, loadingBackgroundWidth, loadingBackgroundHeight);
    }
    else if (titleBackground)
    {
        renderFullscreenTexture(titleBackground, titleBackgroundWidth, titleBackgroundHeight);
    }

    if (!debugText || !renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    std::string line = "LOADING...";
    if (!startupMapName.empty())
    {
        line = "LOADING " + toUpper(startupMapName);
    }
    int percent = static_cast<int>(std::clamp(loadProgress.load(), 0.0f, 1.0f) * 100.0f + 0.5f);
    line += " " + std::to_string(percent) + "%";

    int scale = 2;
    int x = 40;
    int y = viewportHeight > 0 ? viewportHeight - 60 : 520;
    debugText->drawText(sdlRenderer, x, y, scale, 255, 255, 255, line);

    int barWidth = 240;
    int barHeight = 10;
    int barY = y + debugText->lineHeight(scale) + 6;
    SDL_FRect bg = {static_cast<float>(x), static_cast<float>(barY), static_cast<float>(barWidth),
                    static_cast<float>(barHeight)};
    SDL_SetRenderDrawColor(sdlRenderer, 20, 20, 20, 200);
    SDL_RenderFillRect(sdlRenderer, &bg);

    float fill = std::clamp(loadProgress.load(), 0.0f, 1.0f);
    SDL_FRect fg = {static_cast<float>(x) + 1.0f, static_cast<float>(barY) + 1.0f,
                    static_cast<float>((barWidth - 2) * fill), static_cast<float>(barHeight - 2)};
    SDL_SetRenderDrawColor(sdlRenderer, 230, 200, 120, 220);
    SDL_RenderFillRect(sdlRenderer, &fg);
}

void Application::renderOverlay()
{
    if (!debugText || !renderer || !renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    std::vector<std::string> lines;

    switch (gameState)
    {
    case GameState::InGame:
        if (!showHelpOverlay)
        {
            return;
        }
        if (mapScene && mapScene->isLoaded())
        {
            if (mapScene->getODMData().heightmap.empty())
            {
                const auto& data = mapScene->getBLVData();
                lines.push_back("MAP: " + toUpper(mapScene->getName()));
                lines.push_back(std::format("VERTS: {}  FACES: {}  LIGHTS: {}",
                                            data.vertices.size(), data.faces.size(),
                                            data.lights.size()));
            }
            else
            {
                const auto& data = mapScene->getODMData();
                lines.push_back("MAP: " + toUpper(mapScene->getName()));
                lines.push_back(
                    std::format("TERRAIN: {}x{}  BUILDINGS: {}",
                                data.heightmap.size() > 0 ? formats::ODMMapData::TERRAIN_SIZE : 0,
                                data.heightmap.size() > 0 ? formats::ODMMapData::TERRAIN_SIZE : 0,
                                data.buildings.size()));
            }
        }
        else
        {
            lines.push_back("MAP: (NONE)");
        }

        lines.push_back("ARROWS ORBIT  Q/E ZOOM  WASD PAN  R RESET");
        lines.push_back(
            std::format("F FLOORS:{}  V WALLS:{}  C CEIL:{}  P PORTAL:{}",
                        onOff(mapRenderOptions.showFloors), onOff(mapRenderOptions.showWalls),
                        onOff(mapRenderOptions.showCeilings), onOff(mapRenderOptions.showPortals)));
        lines.push_back(std::format("L LIGHTS:{}  G GRID:{}  X AXES:{}  H HELP:{}",
                                    onOff(mapRenderOptions.showLights), onOff(showGrid),
                                    onOff(showAxes), onOff(showHelpOverlay)));
        break;
    default:
        return;
    }

    if (lines.empty())
    {
        return;
    }

    const int scale = 2;
    int maxLen = 0;
    for (const auto& line : lines)
    {
        maxLen = std::max(maxLen, static_cast<int>(line.size()));
    }

    const int padding = 8;
    int boxWidth = debugText->charWidth(scale) * maxLen + padding * 2;
    int boxHeight = debugText->lineHeight(scale) * static_cast<int>(lines.size()) + padding * 2;

    SDL_FRect panel = {10.0f, 10.0f, static_cast<float>(boxWidth), static_cast<float>(boxHeight)};
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 180);
    SDL_RenderFillRect(sdlRenderer, &panel);

    int cursorY = static_cast<int>(panel.y) + padding;
    for (const auto& line : lines)
    {
        debugText->drawText(sdlRenderer, static_cast<int>(panel.x) + padding, cursorY, scale, 230,
                            230, 230, line);
        cursorY += debugText->lineHeight(scale);
    }
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
        loadPcxTexture(portraitCandidates, std::format("Portrait {}", i + 1),
                       portraitTextures[i], portraitWidths[i], portraitHeights[i]);
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
                        logger.info(std::format("Loaded UI texture '{}': {} ({}x{})", label,
                                                name, width, height));
                        return true;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                logger.warning(
                    std::format("Failed to convert PCX '{}': {}", name, ex.what()));
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
            auto image = graphics::Image::fromPalettedData(frame.data, frame.width, frame.height, sprite.palette);
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
            logger.warning(std::format("No palette found for '{}' (paletteId={})", name,
                                        imgInfo->paletteId));
            continue;
        }

        try
        {
            auto palette = graphics::Palette::fromRGBData(*palData);
            // Index 0 is transparent
            palette.setColor(0, graphics::Palette::Color(0, 0, 0, 0));

            auto image = graphics::Image::fromPalettedData(
                *pixelData, imgInfo->width, imgInfo->height, palette);
            if (image)
            {
                void* tex = renderer->createTexture(*image);
                if (tex)
                {
                    renderer->destroyTexture(textureHandle);
                    textureHandle = tex;
                    width = static_cast<int>(imgInfo->width);
                    height = static_cast<int>(imgInfo->height);
                    logger.info(std::format("Loaded UI texture '{}' as paletted: {} ({}x{})",
                                            label, name, width, height));
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

void Application::renderFullscreenTexture(void* textureHandle, int texWidth, int texHeight)
{
    if (!renderer || !textureHandle || texWidth <= 0 || texHeight <= 0 || viewportWidth <= 0 ||
        viewportHeight <= 0)
    {
        return;
    }

    float scaleX = static_cast<float>(viewportWidth) / static_cast<float>(texWidth);
    float scaleY = static_cast<float>(viewportHeight) / static_cast<float>(texHeight);
    float scale = std::min(scaleX, scaleY);

    int drawWidth = static_cast<int>(texWidth * scale);
    int drawHeight = static_cast<int>(texHeight * scale);
    int drawX = (viewportWidth - drawWidth) / 2;
    int drawY = (viewportHeight - drawHeight) / 2;

    renderer->renderTexture(textureHandle, drawX, drawY, drawWidth, drawHeight);
}

void Application::renderMenu(const std::vector<std::string>& items, int selectedIndex, int x, int y,
                             int scale)
{
    if (!debugText || !renderer || !renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    int cursorY = y;
    int panelPadding = 6;
    int panelWidth = 0;
    int panelHeight = 0;

    for (const auto& item : items)
    {
        panelWidth =
            std::max(panelWidth, debugText->charWidth(scale) * static_cast<int>(item.size()));
    }
    panelHeight = debugText->lineHeight(scale) * static_cast<int>(items.size());

    SDL_FRect panel = {static_cast<float>(x - panelPadding), static_cast<float>(y - panelPadding),
                       static_cast<float>(panelWidth + panelPadding * 2),
                       static_cast<float>(panelHeight + panelPadding * 2)};
    SDL_SetRenderDrawColor(sdlRenderer, 10, 10, 10, 180);
    SDL_RenderFillRect(sdlRenderer, &panel);

    for (size_t i = 0; i < items.size(); i++)
    {
        bool selected = static_cast<int>(i) == selectedIndex;
        uint8_t r = selected ? 255 : 210;
        uint8_t g = selected ? 220 : 210;
        uint8_t b = selected ? 130 : 210;
        debugText->drawText(sdlRenderer, x, cursorY, scale, r, g, b, items[i]);
        cursorY += debugText->lineHeight(scale);
    }
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
    if (loadingTaskActive.load())
    {
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
        mapScene = std::move(loadedScene);
        mapLoaded = true;
        configureCameraForMap();
        setGameState(GameState::InGame);
    }
    else
    {
        stateMessage = error.empty() ? "Failed to load map" : error;
        setGameState(GameState::TitleScreen);
    }
}

void Application::runLoadingTask(LoadRequest request)
{
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

bool Application::isKeyDown(SDL_Scancode code) const
{
    if (!keyState || code >= keyCount)
    {
        return false;
    }
    return keyState[code];
}

bool Application::isKeyPressed(SDL_Scancode code) const
{
    if (!keyState || code >= keyCount)
    {
        return false;
    }
    bool now = keyState[code];
    bool before =
        previousKeyState.empty() ? false : previousKeyState[static_cast<size_t>(code)] != 0;
    return now && !before;
}

bool Application::isMouseOver(const graphics::Rect& rect) const
{
    auto mouseState = window.getMouseState();
    return rect.contains(mouseState.x, mouseState.y);
}

bool Application::wasMouseClickedIn(const graphics::Rect& rect) const
{
    if (!window.wasMousePressed(platform::MouseButton::Left))
    {
        return false;
    }

    return isMouseOver(rect);
}

int Application::scaleX(int gameX) const
{
    float s = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    float offsetX = (viewportWidth - kGameWidth * s) / 2.0f;
    return static_cast<int>(offsetX + gameX * s);
}

int Application::scaleY(int gameY) const
{
    float s = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    float offsetY = (viewportHeight - kGameHeight * s) / 2.0f;
    return static_cast<int>(offsetY + gameY * s);
}

int Application::scaleW(int gameW) const
{
    float s = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    return static_cast<int>(gameW * s);
}

int Application::scaleH(int gameH) const
{
    float s = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    return static_cast<int>(gameH * s);
}

int Application::unscaleX(int screenX) const
{
    float s = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    if (s <= 0.0f) return 0;
    float offsetX = (viewportWidth - kGameWidth * s) / 2.0f;
    return static_cast<int>((screenX - offsetX) / s);
}

int Application::unscaleY(int screenY) const
{
    float s = std::min(
        static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
        static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
    if (s <= 0.0f) return 0;
    float offsetY = (viewportHeight - kGameHeight * s) / 2.0f;
    return static_cast<int>((screenY - offsetY) / s);
}

int Application::calculateBonusPointsRemaining() const
{
    int totalSpent = 0;
    for (const auto& ch : party)
    {
        for (int i = 0; i < 7; i++)
        {
            totalSpent += ch.stats.byIndex(i) - ch.baseStats.byIndex(i);
        }
    }
    return 50 - totalSpent;
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

void Application::layoutTitleMenuButtons()
{
    if (viewportWidth == 0 || viewportHeight == 0)
    {
        return;
    }
    titleMenuUI.buttons.clear();

    // MM7-accurate button positions (640x480 game coords)
    constexpr int kButtonGameX = 495;
    constexpr int kButtonGameY[] = {172, 227, 282, 337};

    for (int i = 0; i < kTitleButtonCount; ++i)
    {
        // Use actual texture dimensions for hit area, with sensible defaults
        int btnW = titleButtonHoverWidths[i] > 0 ? titleButtonHoverWidths[i] : 85;
        int btnH = titleButtonHoverHeights[i] > 0 ? titleButtonHoverHeights[i] : 30;

        MenuButton button;
        button.id = kTitleMenuItems[static_cast<size_t>(i)];
        button.bounds = {
            scaleX(kButtonGameX),
            scaleY(kButtonGameY[i]),
            scaleW(btnW),
            scaleH(btnH),
        };
        button.hoverTexture = titleButtonHoverTextures[i];
        button.textureWidth = titleButtonHoverWidths[i];
        button.textureHeight = titleButtonHoverHeights[i];
        titleMenuUI.buttons.push_back(button);
    }
}

void Application::updateTitleMenuHover()
{
    for (size_t i = 0; i < titleMenuUI.buttons.size(); ++i)
    {
        auto& button = titleMenuUI.buttons[i];
        const bool mouseOver = isMouseOver(button.bounds);
        if (mouseOver)
        {
            titleMenuIndex = static_cast<int>(i);
            titleMenuUI.selectedIndex = titleMenuIndex;
        }
        button.isHovered = (mouseOver || (static_cast<int>(i) == titleMenuIndex));
    }
}

void Application::shutdown()
{
    logger.info("Shutting down...");

    if (loadingThread.joinable())
    {
        loadingThread.join();
    }

    unloadUiAssets();
    vfs->unmountAll();
    renderer.reset();
    window.shutdown();
    initialized = false;
    logger.info("Shutdown complete");
}
} // namespace runeharbor::engine