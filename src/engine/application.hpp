// SPDX-License-Identifier: MIT
#ifndef RUNEHARBOR_APPLICATION_HPP
#define RUNEHARBOR_APPLICATION_HPP

#include <SDL3/SDL.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../game/character.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/debug_text.hpp"
#include "../media/video_player.hpp"
#include "map_scene.hpp"
#include "states/igame_state.hpp"

// Forward declarations
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
class LineRenderer;
class DebugText;
class WorldRenderer;
class SDLRenderer;
} // namespace runeharbor::graphics
namespace runeharbor::media
{
class VideoPlayer;
}
namespace runeharbor::engine
{
class VirtualFileSystem;
class IGameState;
struct StateContext;
struct SharedGameData;
class IntroState;
class TitleState;
class CharacterCreationState;
class LoadingState;
class InGameState;
} // namespace runeharbor::engine

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

// Character types from game:: namespace, re-exported into engine:: for backward compatibility
using game::CharacterClass;
using game::Gender;
using game::Stats;
using game::baseClassIndex;
using Character = game::Character;

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

    // State machine
    std::unique_ptr<StateContext> stateCtx;
    std::unique_ptr<SharedGameData> sharedData;
    IGameState* activeState = nullptr;
    GameStateId activeStateId = GameStateId::IntroVideo;
    std::unique_ptr<IntroState> introState;
    std::unique_ptr<TitleState> titleState;
    std::unique_ptr<CharacterCreationState> charCreationState;
    std::unique_ptr<LoadingState> loadingState;
    std::unique_ptr<InGameState> inGameState;

    // Game state management
    GameState gameState = GameState::IntroVideo;

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
    static constexpr int kTitleButtonCount = 4;
    void* titleBackground = nullptr;
    void* titleButtonHoverTextures[kTitleButtonCount] = {nullptr};
    int titleButtonHoverWidths[kTitleButtonCount] = {0};
    int titleButtonHoverHeights[kTitleButtonCount] = {0};

    static constexpr int kPortraitCount = 20;
    void* portraitTextures[kPortraitCount] = {};
    int portraitWidths[kPortraitCount] = {};
    int portraitHeights[kPortraitCount] = {};

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

    // Map texture cache (face texture name -> GPU texture handle)
    std::unordered_map<std::string, void*> mapTextureCache;

    // Methods
    void setGameState(GameState state);
    void transitionTo(GameStateId id);
    void initStates();
    void updateStateContext();
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
    void wireUpMapTextures();
    void clearMapTextureCache();

    // Input handling
    void pollKeyboardState();
    void commitKeyboardState();

    // Asset loading
    bool loadUiAssets();
    void unloadUiAssets();
    bool loadPcxTexture(const std::vector<std::string>& candidates, const std::string& label,
                        void*& textureHandle, int& width, int& height);
    bool loadPcxSequence(const std::string& prefix, std::vector<void*>& textures,
                         std::vector<int>& widths, std::vector<int>& heights);

    // Character helpers (used by initDefaultParty)
    void initDefaultParty();
    void updateCharacterForFace(Character& ch);
    void updateSkillsForClass(Character& ch);
};

} // namespace runeharbor::engine

#endif // RUNEHARBOR_APPLICATION_HPP
