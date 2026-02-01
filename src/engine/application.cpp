// SPDX-License-Identifier: MIT
#include "application.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <format>
#include <vector>

#include "../formats/pcx_image.hpp"
#include "../graphics/image.hpp"
#include "../graphics/line_renderer.hpp"
#include "../graphics/sdl_renderer.hpp"
#include "../platform/iwindow.hpp"
#include "../util/ilogger.hpp"
#include "virtual_filesystem.hpp"

namespace runeharbor::engine
{

Application::Application(util::ILogger& logger, platform::IWindow& window)
    : logger(logger), window(window), vfs(std::make_unique<VirtualFileSystem>(logger))
{
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

const std::vector<std::string> kTitleMenuItems = {
    "NEW GAME",
    "LOAD GAME",
    "QUIT",
};

const std::vector<std::string> kCharacterMenuItems = {
    "QUICK START",
    "BACK",
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

    lineRenderer = std::make_unique<graphics::LineRenderer>(renderer->getSDLRenderer(), logger);
    debugText = std::make_unique<graphics::DebugText>();
    mapScene = std::make_unique<MapScene>(logger);
    videoPlayer = std::make_unique<media::VideoPlayer>();
    updateViewport();

    logger.info("Press ESC or close window to exit");

    setGameState(GameState::IntroVideo);
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
        if (mapLoaded && mapScene && lineRenderer)
        {
            lineRenderer->clear();
            lineRenderer->setViewProjection(camera.getViewProjectionMatrix());

            mapScene->renderWireframe(*lineRenderer, mapRenderOptions);

            const auto& bounds = mapScene->getBounds();
            float baseSize = bounds.valid ? std::max(bounds.radius(), 5000.0f) : 5000.0f;

            if (showGrid)
            {
                float gridSpacing = baseSize / 10.0f;
                lineRenderer->drawGrid(baseSize, gridSpacing, 60, 60, 60);
            }

            if (showAxes)
            {
                float axisSize = std::max(baseSize * 0.1f, 200.0f);
                lineRenderer->drawAxes(graphics::Vec3(0, 0, 0), axisSize);
            }

            lineRenderer->render();
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
        if (isKeyPressed(SDL_SCANCODE_UP))
        {
            titleMenuIndex = (titleMenuIndex + static_cast<int>(kTitleMenuItems.size()) - 1) %
                             static_cast<int>(kTitleMenuItems.size());
        }
        if (isKeyPressed(SDL_SCANCODE_DOWN))
        {
            titleMenuIndex = (titleMenuIndex + 1) % static_cast<int>(kTitleMenuItems.size());
        }

        if (isKeyPressed(SDL_SCANCODE_RETURN))
        {
            if (titleMenuIndex == 0)
            {
                quickStartReady = false;
                setGameState(GameState::CharacterCreation);
            }
            else if (titleMenuIndex == 1)
            {
                stateMessage = "Load game not implemented yet";
            }
            else if (titleMenuIndex == 2)
            {
                stateMessage = "Use ESC or window close to exit";
            }
        }

        if (isKeyPressed(SDL_SCANCODE_N))
        {
            quickStartReady = false;
            setGameState(GameState::CharacterCreation);
        }
        else if (isKeyPressed(SDL_SCANCODE_L))
        {
            stateMessage = "Load game not implemented yet";
        }
        else if (isKeyPressed(SDL_SCANCODE_Q))
        {
            stateMessage = "Use ESC or window close to exit";
        }
        break;
    case GameState::CharacterCreation:
        if (isKeyPressed(SDL_SCANCODE_UP))
        {
            characterMenuIndex =
                (characterMenuIndex + static_cast<int>(kCharacterMenuItems.size()) - 1) %
                static_cast<int>(kCharacterMenuItems.size());
        }
        if (isKeyPressed(SDL_SCANCODE_DOWN))
        {
            characterMenuIndex =
                (characterMenuIndex + 1) % static_cast<int>(kCharacterMenuItems.size());
        }

        if (isKeyPressed(SDL_SCANCODE_RETURN))
        {
            if (characterMenuIndex == 0)
            {
                quickStartReady = true;
                setGameState(GameState::Loading);
            }
            else
            {
                setGameState(GameState::TitleScreen);
            }
        }

        if (isKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            setGameState(GameState::TitleScreen);
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

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    int scale = 3;
    int menuX = 60;
    int menuY = viewportHeight > 0 ? viewportHeight / 2 - 20 : 260;

    renderMenu(kTitleMenuItems, titleMenuIndex, menuX, menuY, scale);

    if (!stateMessage.empty())
    {
        debugText->drawText(sdlRenderer, menuX, menuY + 120, 2, 255, 200, 120, stateMessage);
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

    int scale = 2;
    int menuX = 60;
    int menuY = viewportHeight > 0 ? viewportHeight / 2 - 20 : 260;

    debugText->drawText(sdlRenderer, menuX, menuY - 60, 2, 255, 230, 180,
                        "CHARACTER CREATION");
    renderMenu(kCharacterMenuItems, characterMenuIndex, menuX, menuY, scale);
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
            frameIndex = static_cast<size_t>(progress * static_cast<float>(loadingFrames.size() - 1));
        }
        else if (loadingFrameDurationMs > 0)
        {
            uint64_t now = SDL_GetTicks();
            frameIndex =
                static_cast<size_t>((now - stateStartTicks) / loadingFrameDurationMs) %
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
    SDL_FRect bg = {static_cast<float>(x), static_cast<float>(barY),
                    static_cast<float>(barWidth), static_cast<float>(barHeight)};
    SDL_SetRenderDrawColor(sdlRenderer, 20, 20, 20, 200);
    SDL_RenderFillRect(sdlRenderer, &bg);

    float fill = std::clamp(loadProgress.load(), 0.0f, 1.0f);
    SDL_FRect fg = {static_cast<float>(x) + 1.0f, static_cast<float>(barY) + 1.0f,
                    static_cast<float>((barWidth - 2) * fill),
                    static_cast<float>(barHeight - 2)};
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
                lines.push_back(std::format("TERRAIN: {}x{}  BUILDINGS: {}",
                                            data.heightmap.size() > 0
                                                ? formats::ODMMapData::TERRAIN_SIZE
                                                : 0,
                                            data.heightmap.size() > 0
                                                ? formats::ODMMapData::TERRAIN_SIZE
                                                : 0,
                                            data.buildings.size()));
            }
        }
        else
        {
            lines.push_back("MAP: (NONE)");
        }

        lines.push_back("ARROWS ORBIT  Q/E ZOOM  WASD PAN  R RESET");
        lines.push_back(std::format("F FLOORS:{}  V WALLS:{}  C CEIL:{}  P PORTAL:{}",
                                    onOff(mapRenderOptions.showFloors),
                                    onOff(mapRenderOptions.showWalls),
                                    onOff(mapRenderOptions.showCeilings),
                                    onOff(mapRenderOptions.showPortals)));
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

    SDL_FRect panel = {10.0f, 10.0f, static_cast<float>(boxWidth),
                       static_cast<float>(boxHeight)};
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
    bool loaded = false;
    if (std::filesystem::exists(magicVid) && manifest.load(magicVid))
    {
        loaded = true;
    }
    else if (std::filesystem::exists(mightVid) && manifest.load(mightVid))
    {
        loaded = true;
    }

    if (!loaded)
    {
        introPlaylist.push_back({"Intro", 2500});
        return;
    }

    // Build a filtered intro list (logos + intro)
    const std::vector<std::string> preferred = {
        "3DOLOGO.SMK",
        "JVC.BIK",
        "NEW WORLD LOGO.BIK",
        "INTRO.BIK",
        "INTRO POST.BIK",
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
    loadPcxSequence("loading", loadingFrames, loadingFrameWidths, loadingFrameHeights);
    loadPcxTexture({"loading.pcx", "Loading.pcx", "LOADING.PCX"}, "Loading", loadingBackground,
                   loadingBackgroundWidth, loadingBackgroundHeight);

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

    const auto allFiles = vfs->listAllFiles();

    for (const auto& name : candidates)
    {
        std::optional<std::string> resolvedName;
        std::string targetLower = toLower(name);
        std::string targetStem = targetLower;
        size_t dot = targetStem.find_last_of('.');
        if (dot != std::string::npos)
        {
            targetStem = targetStem.substr(0, dot);
        }

        for (const auto& file : allFiles)
        {
            std::string fileLower = toLower(file);
            if (fileLower == targetLower || fileLower == targetStem)
            {
                resolvedName = file;
                break;
            }
            size_t fileDot = fileLower.find_last_of('.');
            if (fileDot != std::string::npos &&
                fileLower.substr(0, fileDot) == targetStem)
            {
                resolvedName = file;
                break;
            }
        }

        if (!resolvedName.has_value())
        {
            continue;
        }

        auto data = vfs->readFile(*resolvedName);
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
            auto image =
                graphics::Image::fromPalettedData(pcx->indices, pcx->width, pcx->height,
                                                  pcx->palette);
            if (!image)
            {
                continue;
            }

            void* tex = renderer->createTexture(*image);
            if (!tex)
            {
                continue;
            }

            renderer->destroyTexture(textureHandle);
            textureHandle = tex;
            width = static_cast<int>(pcx->width);
            height = static_cast<int>(pcx->height);

            logger.info(std::format("Loaded UI texture '{}': {} ({}x{})", label, *resolvedName,
                                    width, height));
            return true;
        }
        catch (const std::exception& ex)
        {
            logger.warning(
                std::format("Failed to convert PCX '{}': {}", *resolvedName, ex.what()));
        }
    }

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
            auto image =
                graphics::Image::fromPalettedData(pcx->indices, pcx->width, pcx->height,
                                                  pcx->palette);
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
            logger.warning(
                std::format("Failed to convert PCX '{}': {}", entry.name, ex.what()));
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

    SDL_FRect panel = {static_cast<float>(x - panelPadding),
                       static_cast<float>(y - panelPadding),
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

    auto tryLoad = [&](const std::string& mapName, bool outdoor) -> bool {
        return loadMapIntoScene(mapName, outdoor, scene, error);
    };

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
    bool before = previousKeyState.empty() ? false : previousKeyState[static_cast<size_t>(code)] != 0;
    return now && !before;
}

void Application::shutdown()
{
    if (!initialized)
    {
        return;
    }

    logger.info("Shutting down...");

    if (loadingTaskActive.load())
    {
        if (loadingThread.joinable())
        {
            loadingThread.join();
        }
        loadingTaskActive.store(false);
        loadingTaskDone.store(false);
    }

    unloadUiAssets();

    // Unmount all game data
    if (gameDataLoaded && vfs)
    {
        vfs->unmountAll();
        gameDataLoaded = false;
    }

    window.shutdown();
    logger.info("Shutdown complete");

    initialized = false;
}

} // namespace runeharbor::engine
