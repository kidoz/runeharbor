// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "../../media/video_player.hpp"
#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class IntroState : public IGameState
{
  public:
    explicit IntroState(StateContext& ctx);
    ~IntroState() override;

    void setPlaylist(std::vector<media::VideoClip> playlist);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    StateContext& ctx;
    std::vector<media::VideoClip> playlist;
    bool hasVideo = false;
};

} // namespace runeharbor::engine
