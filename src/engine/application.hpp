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

enum class Race
{
    Human,
    Elf,
    Dwarf,
    Goblin
};

enum class Gender
{
    Male,
    Female
};

enum class CharacterClass
{
    Knight,
    Paladin,
    Archer,
    Cleric,
    Sorcerer,
    Thief,
    Monk,
    Ranger,
    Druid
};

struct Character
{
    std::string name = "New Hero";
    int faceId = 0;
    CharacterClass charClass = CharacterClass::Knight;

    struct Stats
    {
        int might = 11;
        int intellect = 11;
        int personality = 11;
        int endurance = 11;
        int speed = 11;
        int accuracy = 11;
        int luck = 11;

        int& byIndex(int i)
        {
            switch (i)
            {
            case 1: return intellect;
            case 2: return personality;
            case 3: return endurance;
            case 4: return speed;
            case 5: return accuracy;
            case 6: return luck;
            default: return might;
            }
        }

        int byIndex(int i) const
        {
            switch (i)
            {
            case 0: return might;
            case 1: return intellect;
            case 2: return personality;
            case 3: return endurance;
            case 4: return speed;
            case 5: return accuracy;
            case 6: return luck;
            default: return 0;
            }
        }
    } stats;

    Stats baseStats;
    std::vector<std::string> skills;
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

    static constexpr int kPortraitCount = 20;
    void* portraitTextures[kPortraitCount] = {};
    int portraitWidths[kPortraitCount] = {};
    int portraitHeights[kPortraitCount] = {};

    int characterMenuIndex = 0; // Row: 0=NAME, 1=FACE, 2=CLASS, 3-9=stats
    int activeCharacterIndex = 0;
    static constexpr int kCharCreationRowCount = 10;
    bool isNaming = false;
    bool quickStartReady = false;
    std::vector<Character> party;

    // UI Assets
    bool uiAssetsLoaded = false;
    void* createBackground = nullptr;
    int createBackgroundWidth = 0;
    int createBackgroundHeight = 0;
    void* loadingBackground = nullptr;
    int loadingBackgroundWidth = 0;
    int loadingBackgroundHeight = 0;
    std::vector<uint8_t> screenPaletteRGB; // 768-byte RGB palette from background PCX
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

    // Coordinate scaling (640x480 game space <-> viewport)
    int scaleX(int gameX) const;
    int scaleY(int gameY) const;
    int scaleW(int gameW) const;
    int scaleH(int gameH) const;
    int unscaleX(int screenX) const;
    int unscaleY(int screenY) const;

    // Character creation helpers
    int calculateBonusPointsRemaining() const;
    void initDefaultParty();
    void updateCharacterForFace(Character& ch);
    void updateSkillsForClass(Character& ch);
};

}

#endif // RUNEHARBOR_APPLICATION_HPP
