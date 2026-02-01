// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "../graphics/camera.hpp"
#include "../graphics/debug_text.hpp"
#include "map_scene.hpp"
#include "../media/video_player.hpp"
#include "../media/vid_manifest.hpp"

namespace runeharbor::platform
{
class IWindow;
struct WindowConfig;
} // namespace runeharbor::platform

namespace runeharbor::util
{
class ILogger;
}

namespace runeharbor::graphics
{
class IRenderer;
}

namespace runeharbor::engine
{
class VirtualFileSystem;

class Application
{
  public:
    Application(util::ILogger& logger, platform::IWindow& window);
    ~Application();

    bool initialize(const platform::WindowConfig& windowConfig);

    /// Load game data from specified directory (mounts LOD archives)
    /// Returns true on success, false if no archives were mounted
    bool loadGameData(const std::filesystem::path& dataPath);

    /// Configure boot flow map selection and auto-load behavior
    void configureBootFlow(const std::string& mapName, bool preferOutdoor, bool autoLoad);

    /// Load a specific BLV map from mounted archives
    bool loadMap(const std::string& mapName);

    /// Load the first available BLV map from mounted archives
    bool loadFirstMap();

    /// Load a specific ODM map from mounted archives
    bool loadOutdoorMap(const std::string& mapName);

    /// Load the first available ODM map from mounted archives
    bool loadFirstOutdoorMap();

    /// List available BLV maps from mounted archives
    std::vector<std::string> listMaps() const;

    /// List available ODM maps from mounted archives
    std::vector<std::string> listOutdoorMaps() const;

    void run();
    void shutdown();

  private:
    enum class GameState
    {
        IntroVideo,
        TitleScreen,
        CharacterCreation,
        Loading,
        InGame
    };

    void setGameState(GameState state);
    void updateStateMachine();
    void renderOverlay();
    void renderIntroVideo();
    void renderTitleScreen();
    void renderCharacterCreation();
    void renderLoadingScreen();
    void buildIntroPlaylist();
    bool loadUiAssets();
    void unloadUiAssets();
    void pollKeyboardState();
    void commitKeyboardState();
    bool isKeyDown(SDL_Scancode code) const;
    bool isKeyPressed(SDL_Scancode code) const;

    void renderFrame();
    void updateViewport();
    void configureCameraForMap();
    void updateCameraInput();
    bool loadPcxTexture(const std::vector<std::string>& candidates, const std::string& label,
                        void*& textureHandle, int& width, int& height);
    bool loadPcxSequence(const std::string& prefix, std::vector<void*>& textures,
                         std::vector<int>& widths, std::vector<int>& heights);
    void renderFullscreenTexture(void* textureHandle, int texWidth, int texHeight);
    void renderMenu(const std::vector<std::string>& items, int selectedIndex, int x, int y,
                    int scale);
    void updateLoadProgress(float value);
    void startLoadingTask();
    void finalizeLoadingTask();

    struct LoadRequest
    {
        std::string mapName;
        bool preferOutdoor = true;
    };

    void runLoadingTask(LoadRequest request);
    bool loadMapIntoScene(const std::string& mapName, bool outdoor,
                          std::unique_ptr<MapScene>& outScene, std::string& error);
    std::optional<std::string> pickFirstMap(bool outdoor);

    util::ILogger& logger;
    platform::IWindow& window;
    std::unique_ptr<VirtualFileSystem> vfs;
    std::unique_ptr<graphics::IRenderer> renderer;
    std::unique_ptr<graphics::LineRenderer> lineRenderer;
    std::unique_ptr<graphics::DebugText> debugText;
    std::unique_ptr<MapScene> mapScene;
    graphics::Camera camera;
    MapRenderOptions mapRenderOptions;
    bool mapLoaded = false;
    bool showGrid = false;
    bool showAxes = true;
    bool showHelpOverlay = true;
    bool autoLoadMap = true;
    bool startupPreferOutdoor = true;
    bool loadingStarted = false;
    bool quickStartReady = false;
    std::string startupMapName;
    std::string stateMessage;
    GameState gameState = GameState::IntroVideo;
    uint64_t stateStartTicks = 0;
    const bool* keyState = nullptr;
    int keyCount = 0;
    std::unique_ptr<media::VideoPlayer> videoPlayer;
    std::vector<media::VideoClip> introPlaylist;
    std::filesystem::path dataRoot;
    std::filesystem::path gameRoot;
    std::vector<uint8_t> previousKeyState;
    int previousKeyCount = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    bool initialized = false;
    bool gameDataLoaded = false;
    void* titleBackground = nullptr;
    int titleBackgroundWidth = 0;
    int titleBackgroundHeight = 0;
    void* createBackground = nullptr;
    int createBackgroundWidth = 0;
    int createBackgroundHeight = 0;
    void* loadingBackground = nullptr;
    int loadingBackgroundWidth = 0;
    int loadingBackgroundHeight = 0;
    std::vector<void*> loadingFrames;
    std::vector<int> loadingFrameWidths;
    std::vector<int> loadingFrameHeights;
    int loadingFrameDurationMs = 150;
    std::atomic<float> loadProgress = 0.0f;
    std::atomic<bool> loadProgressActive = false;
    std::atomic<bool> loadingTaskActive = false;
    std::atomic<bool> loadingTaskDone = false;
    bool loadingTaskSuccess = false;
    std::string loadingTaskError;
    std::mutex loadingTaskMutex;
    std::unique_ptr<MapScene> loadingTaskScene;
    std::thread loadingThread;
    int titleMenuIndex = 0;
    int characterMenuIndex = 0;
    bool uiAssetsLoaded = false;
};

} // namespace runeharbor::engine
