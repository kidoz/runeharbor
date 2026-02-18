// SPDX-License-Identifier: MIT
#pragma once

#include "../map_scene.hpp"
#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class InGameState : public IGameState
{
  public:
    explicit InGameState(StateContext& ctx);
    ~InGameState() override;

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    void updateCameraInput();
    void renderOverlay();

    StateContext& ctx;

    MapRenderOptions renderOptions;
    bool showGrid = false;
    bool showAxes = true;
    bool showHelpOverlay = true;
};

} // namespace runeharbor::engine
