// SPDX-License-Identifier: MIT
#ifndef RUNEHARBOR_APPLICATION_HPP
#define RUNEHARBOR_APPLICATION_HPP

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include <SDL3/SDL.h>

#include "map_scene.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/primitives.hpp"
#include "../graphics/debug_text.hpp"
#include "../media/video_player.hpp"

// Forward declarations
namespace runeharbor::platform
{
    class IWindow;
    struct WindowConfig;
}
namespace runeharbor::util
{
    class ILogger;
}
namespace runeharbor::graphics
{
    class IRenderer;
    class LineRenderer;
    class DebugText;
    class WorldRenderer;
    class SDLRenderer;
}
namespace runeharbor::media
{
    class VideoPlayer;
}
namespace runeharbor::engine
{
    class VirtualFileSystem;
}

namespace runeharbor::engine
{

enum class GameState
{
    IntroVideo,
    TitleScreen,
    CharacterCreation,
    Loading,
    InGame,
};

struct Character
{
    std::string name = "Conan";
    struct Stats
    {
        int might = 15;
        int intellect = 10;
        int personality = 10;
        int endurance = 15;
        int speed = 10;
        int accuracy = 10;
        int luck = 10;
    } stats;
};

class Application
{
  public:
    Application(util::ILogger& logger, platform::IWindow& window);
    ~Application();

    bool initialize(const platform::WindowConfig& windowConfig);
    bool loadGameData(const std::filesystem::path& dataPath);
    void configureBootFlow(const std::string& mapName, bool preferOutdoor, bool autoLoad);
    void run();
    void shutdown();
    std::vector<std::string> listMaps() const;
    std::vector<std::string> listOutdoorMaps() const;

  private:
    util::ILogger& logger;
    platform::IWindow& window;
    std::unique_ptr<graphics::IRenderer> renderer;
    std::unique_ptr<graphics::LineRenderer> lineRenderer;
    std::unique_ptr<graphics::WorldRenderer> worldRenderer;
    std::unique_ptr<graphics::DebugText> debugText;
    std::unique_ptr<media::VideoPlayer> videoPlayer;

    bool initialized = false;
    int viewportWidth = 0;
    int viewportHeight = 0;

    // Game state management
    GameState gameState = GameState::IntroVideo;
    uint64_t stateStartTicks = 0;
    std::string stateMessage;

    // Input state
    const bool* keyState = nullptr;
    std::vector<uint8_t> previousKeyState;
    int keyCount = 0;
    int previousKeyCount = 0;

    // Game data
    std::unique_ptr<VirtualFileSystem> vfs;
    std::filesystem::path dataRoot;
    std::filesystem::path gameRoot;
    bool gameDataLoaded = false;
    bool mapLoaded = false;

    std::string startupMapName;
    bool startupPreferOutdoor = false;
    bool autoLoadMap = false;

    // Camera
    graphics::Camera camera;

    // Map data
    std::unique_ptr<MapScene> mapScene;

    // Intro video
    std::vector<media::VideoClip> introPlaylist;

    // UI state
    struct MenuButton
    {
        std::string id;
        graphics::Rect bounds;
        void* hoverTexture = nullptr;
        int textureWidth = 0;
        int textureHeight = 0;
        bool isHovered = false;
    };

    struct TitleMenuUI
    {
        std::vector<MenuButton> buttons;
        int selectedIndex = 0;
    } titleMenuUI;
    static constexpr int kTitleButtonCount = 4;
    int titleMenuIndex = 0;
    void* titleBackground = nullptr;
    void* titleButtonHoverTextures[kTitleButtonCount] = {nullptr};
    int titleButtonHoverWidths[kTitleButtonCount] = {0};
    int titleButtonHoverHeights[kTitleButtonCount] = {0};

    int characterMenuIndex = 0;
    bool quickStartReady = false;
    Character current_character;

    // UI Assets
    bool uiAssetsLoaded = false;
    void* createBackground = nullptr;
    int createBackgroundWidth = 0;
    int createBackgroundHeight = 0;
    void* loadingBackground = nullptr;
    int loadingBackgroundWidth = 0;
    int loadingBackgroundHeight = 0;
    std::vector<void*> loadingFrames;
    std::vector<int> loadingFrameWidths;
    std::vector<int> loadingFrameHeights;
    uint32_t loadingFrameDurationMs = 100;
    int titleBackgroundWidth = 0;
    int titleBackgroundHeight = 0;

    // Loading state
    std::atomic<float> loadProgress{0.0f};
    std::atomic<bool> loadProgressActive{false};
    std::atomic<bool> loadingTaskActive{false};
    std::atomic<bool> loadingTaskDone{false};
    std::thread loadingThread;
    std::mutex loadingTaskMutex;
    struct LoadRequest
    {
        std::string mapName;
        bool preferOutdoor = false;
    };
    std::unique_ptr<MapScene> loadingTaskScene;
    std::string loadingTaskError;
    bool loadingTaskSuccess = false;
    bool loadingStarted = false;

    // In-game state
    MapRenderOptions mapRenderOptions;
    bool showGrid = false;
    bool showAxes = true;
    bool showHelpOverlay = true;

    // Methods
    void setGameState(GameState state);
    void updateStateMachine();
    void updateViewport();
    void renderFrame();

    void buildIntroPlaylist();

    // Map handling
    bool loadMap(const std::string& mapName);
    bool loadFirstMap();
    bool loadOutdoorMap(const std::string& mapName);
    bool loadFirstOutdoorMap();


    // Loading thread logic
    void startLoadingTask();
    void finalizeLoadingTask();
    void runLoadingTask(LoadRequest request);
    bool loadMapIntoScene(const std::string& mapName, bool outdoor,
                          std::unique_ptr<MapScene>& outScene, std::string& error);
    std::optional<std::string> pickFirstMap(bool outdoor);
    void updateLoadProgress(float value);

    // In-game rendering
    void configureCameraForMap();

    // Input handling
    void pollKeyboardState();
    void commitKeyboardState();
    void updateCameraInput();
    bool isKeyDown(SDL_Scancode code) const;
    bool isKeyPressed(SDL_Scancode code) const;

    // UI rendering
    void renderIntroVideo();
    void renderTitleScreen();
    void renderCharacterCreation();
    void renderLoadingScreen();
    void renderOverlay();
    void renderMenu(const std::vector<std::string>& items, int selectedIndex, int x, int y,
                    int scale);
    void renderFullscreenTexture(void* textureHandle, int texWidth, int texHeight);
    bool loadUiAssets();
    void unloadUiAssets();
    bool loadPcxTexture(const std::vector<std::string>& candidates, const std::string& label,
                        void*& textureHandle, int& width, int& height);
    bool loadPcxSequence(const std::string& prefix, std::vector<void*>& textures,
                         std::vector<int>& widths, std::vector<int>& heights);
    void layoutTitleMenuButtons();
    void updateTitleMenuHover();
    bool isMouseOver(const graphics::Rect& rect) const;
    bool wasMouseClickedIn(const graphics::Rect& rect) const;
};

}

#endif // RUNEHARBOR_APPLICATION_HPP
