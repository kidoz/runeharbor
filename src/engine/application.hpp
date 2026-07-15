// SPDX-License-Identifier: MIT
#ifndef RUNEHARBOR_APPLICATION_HPP
#define RUNEHARBOR_APPLICATION_HPP

#include <SDL3/SDL.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../audio/audio_system.hpp"
#include "../formats/evt_script_parser.hpp"
#include "../formats/frame_tables.hpp"
#include "../formats/npcgreet_parser.hpp"
#include "../formats/npcprof_parser.hpp"
#include "../formats/snd_archive.hpp"
#include "../formats/sound_list.hpp"
#include "../game/character.hpp"
#include "../game/combat.hpp"
#include "../game/event_engine.hpp"
#include "../game/game_world.hpp"
#include "../game/inventory.hpp"
#include "../game/save_game.hpp"
#include "../game/spells.hpp"
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
class BitmapFont;
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
class CreditsState;
class LoadGameState;
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
using game::baseClassIndex;
using game::CharacterClass;
using game::Gender;
using game::Stats;
using Character = game::Character;

struct BootConfig
{
    bool noIntro = false;
    bool noLogo = false;
    bool noSound = false;
    bool noWalkSound = false;
    bool noAnim = false;
    int mixerChannels = 16;
    bool windowed = false;
    bool showFr = false;
    bool noMonster = false;
    bool noDamage = false;
    bool noDecoration = false;
    bool noSky = false;
    bool noWavyWater = false;
    bool noMist = false;
    int walkSpeed = 384;
    int partyHeight = 192;
    int partyEyeLevel = 160;
    int gridBand1 = 10;
    int gridBand2 = 15;
    int gridBand3 = 25;
    int terrainGamma = 0;
    int buildingGamma = 0;
    int distShade = 2048;
    int distShadeMist = 4096;
    int distMist = 8192;
    std::array<uint8_t, 3> skyDayTop = {81, 121, 236};
    std::array<uint8_t, 3> skyDayBottom = {153, 193, 237};
    std::array<uint8_t, 3> skyNightTop = {0, 0, 0};
    std::array<uint8_t, 3> skyNightBottom = {11, 41, 129};
    int viewportX = 8;
    int viewportY = 8;
    int viewportWidth = 468;
    int viewportHeight = 351;
};

class Application
{
  public:
    Application(util::ILogger& logger, platform::IWindow& window);
    ~Application();

    bool initialize(const platform::WindowConfig& windowConfig);
    bool loadGameData(const std::filesystem::path& dataPath);
    void setUseDefsMode(bool enabled) { useDefsMode_ = enabled; }
    void setPreferLowResSprites(bool enabled) { preferLowResSprites_ = enabled; }
    void configureBootFlow(const std::string& mapName, bool preferOutdoor, bool autoLoad);
    void setDefaultStartMap(const std::string& mapName);
    void setBootConfig(const BootConfig& config);
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
    std::unique_ptr<audio::AudioSystem> audioSystem_;

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
    std::unique_ptr<CreditsState> creditsState;
    std::unique_ptr<LoadGameState> loadGameState;

    BootConfig bootConfig_;

    // Frame tables
    formats::SpriteFrameTable spriteFrameTable_;
    formats::TextureFrameTable textureFrameTable_;

    // Game state management
    GameState gameState = GameState::IntroVideo;

    // Input state
    const bool* keyState = nullptr;
    std::vector<uint8_t> previousKeyState;
    int keyCount = 0;
    int previousKeyCount = 0;

    // Game data
    std::unique_ptr<VirtualFileSystem> vfs;
    std::unique_ptr<formats::EvtScriptParser> evtParser_;
    std::unique_ptr<game::ContentGenerator> contentGenerator_;
    std::unique_ptr<game::GameWorld> gameWorld_;
    std::unique_ptr<game::EventEngine> eventEngine_;
    std::unique_ptr<game::CombatSystem> combatSystem_;
    std::unique_ptr<game::SpellSystem> spellSystem_;
    std::unique_ptr<game::Inventory> inventory_;
    std::unique_ptr<game::SaveGame> saveGame_;
    std::unordered_map<std::string, int> mapDifficultyByFileName_;
    std::unordered_map<std::string, int> mapRespawnDaysByFileName_;
    std::unordered_map<std::string, std::string> mapDisplayNameByFileName_;
    std::unordered_map<int, std::string> buildingDisplayNameById_;
    std::unordered_map<int, std::string> npcDialogTextById_;
    std::unordered_map<int, std::string> npcDialogOwnerById_;
    std::unordered_map<int, std::string> npcNameById_;
    std::unordered_map<int, std::vector<int>> npcTopicIdsByNpcId_;
    std::unordered_map<int, std::vector<int>> npcTopicIdsByTextId_;
    std::unordered_map<int, std::string> npcTopicNameById_;
    std::unordered_map<int, int> npcProfessionIdByNpcId_;
    std::unordered_map<int, int> npcGreetingIdByNpcId_;
    std::unordered_map<int, formats::NPCProfessionEntry> npcProfessionById_;
    std::unordered_map<int, formats::NPCGreetingEntry> npcGreetingById_;
    std::vector<std::string> npcNamePool_;
    std::vector<game::EventScript> globalEventScripts_;
    std::unique_ptr<formats::SndArchive> sndArchive_;
    std::unique_ptr<formats::SoundList> soundList_;
    std::unordered_map<uint32_t, std::string> soundNameById_;
    std::unordered_set<std::string> loadedSounds_;
    std::filesystem::path dataRoot;
    std::filesystem::path gameRoot;
    bool gameDataLoaded = false;
    bool mapLoaded = false;
    bool useDefsMode_ = false;
    bool preferLowResSprites_ = false;

    std::string startupMapName;
    std::string defaultStartMapName_ = "out01.odm";
    bool startupPreferOutdoor = false;
    bool autoLoadMap = false;
    int pendingEntryDirection_ = 0;

    struct TransitionRequest
    {
        bool active = false;
        std::string sourceMap;
        std::string targetMap;
        int exitDirection = 0;
        int transitionParam = 0;
        bool hasArrivalOverride = false;
        float arrivalX = 0.0f;
        float arrivalY = 0.0f;
        float arrivalZ = 0.0f;
        float arrivalYaw = 0.0f;
    };
    TransitionRequest pendingTransition_;

    struct PendingArrivalOverride
    {
        bool active = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yaw = 0.0f;
    };
    PendingArrivalOverride pendingArrivalOverride_;

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

    // Inventory
    void* inventoryBackground = nullptr;
    int inventoryBackgroundWidth = 0;
    int inventoryBackgroundHeight = 0;
    std::vector<uint8_t> screenPaletteRGB; // 768-byte RGB palette from background PCX

    // Bitmap fonts
    std::vector<uint8_t> fontPalRGB_;                    // FONTPAL (768 bytes)
    std::unique_ptr<graphics::BitmapFont> createFont_;   // create.fnt
    std::unique_ptr<graphics::BitmapFont> ccharFont_;    // CCHAR.FNT
    std::unique_ptr<graphics::BitmapFont> arrusFont_;    // ARRUS.FNT
    std::unique_ptr<graphics::BitmapFont> smallnumFont_; // SMALLNUM.FNT

    // Character creation overlay textures
    struct TexRef
    {
        void* tex = nullptr;
        int w = 0, h = 0;
    };
    TexRef ccFaceMask_;
    TexRef ccSkyHeader_;
    TexRef ccTitleHeader_;
    TexRef ccOkButton_;
    TexRef ccClearButton_;
    TexRef ccMinusButton_;
    TexRef ccPlusButton_;
    TexRef ccLeftArrow_;
    TexRef ccRightArrow_;
    static constexpr int kClassIconCount = 9;
    TexRef ccClassIcons_[kClassIconCount] = {};
    std::vector<void*> loadingFrames;
    std::vector<int> loadingFrameWidths;
    std::vector<int> loadingFrameHeights;
    std::vector<int> loadingFrameNumbers;
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
                         std::vector<int>& widths, std::vector<int>& heights,
                         std::vector<int>* frameNumbers = nullptr);

    // Character helpers (used by initDefaultParty)
    void initDefaultParty();
    void commitPartyToGameWorld();
    void updateCharacterForFace(Character& ch);
    void updateSkillsForClass(Character& ch);
    void loadDataTables();
    void loadGlobalEventScripts();
    void loadMapEventScripts(const std::string& mapName);
    void configureGameplayCallbacks();
    void loadMapStatsTable();
    void playEventSound(int soundId);
    void playUiSound(const std::string& soundName);
    std::string resolveSoundNameById(int soundId) const;
    int resolveMapDifficulty(const std::string& mapName, int fallback) const;
    int resolveMapRespawnDays(const std::string& mapName, int fallback) const;
    std::string resolveMapDisplayName(const std::string& mapName) const;
    std::string resolveBuildingDisplayName(int buildingId) const;
    std::string buildTransitionText(const std::string& targetMap, int direction) const;
    void preserveCurrentMapState();
    void restoreCurrentMapState(const std::string& mapName);
    void applyMapEntryPoint();
};

} // namespace runeharbor::engine

#endif // RUNEHARBOR_APPLICATION_HPP
