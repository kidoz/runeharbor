// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

/// Loading state: shows a progress screen while a map loads asynchronously.
/// The actual loading is managed by Application; this state provides the UI.
class LoadingState : public IGameState
{
  public:
    explicit LoadingState(StateContext& ctx);
    ~LoadingState() override;

    // Callbacks (set by Application to drive loading)
    using StartLoadingFn = std::function<void()>;
    using IsLoadingDoneFn = std::function<bool()>;
    using FinalizeLoadingFn = std::function<bool()>; // returns true if success

    void setCallbacks(StartLoadingFn start, IsLoadingDoneFn isDone, FinalizeLoadingFn finalize);

    // Loading progress (thread-safe)
    void setProgress(std::atomic<float>* progress);

    // Textures (non-owning)
    void setBackground(void* tex, int w, int h);
    void setFallbackBackground(void* tex, int w, int h);
    void setAnimationFrames(std::vector<void*>* frames, std::vector<int>* widths,
                            std::vector<int>* heights);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    StateContext& ctx;
    bool loadingStarted = false;
    uint64_t enterTicks = 0;

    StartLoadingFn onStartLoading;
    StartLoadingFn savedStartLoading;
    IsLoadingDoneFn onIsLoadingDone;
    FinalizeLoadingFn onFinalizeLoading;
    std::atomic<float>* loadProgress = nullptr;

    // Textures (non-owning)
    void* background = nullptr;
    int backgroundWidth = 0;
    int backgroundHeight = 0;
    void* fallbackBackground = nullptr;
    int fallbackWidth = 0;
    int fallbackHeight = 0;
    std::vector<void*>* animFrames = nullptr;
    std::vector<int>* animWidths = nullptr;
    std::vector<int>* animHeights = nullptr;
};

} // namespace runeharbor::engine
