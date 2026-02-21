// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "../../formats/credits_parser.hpp"
#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class CreditsState : public IGameState
{
  public:
    explicit CreditsState(StateContext& ctx);
    ~CreditsState() override;

    void setCreditsSections(std::vector<formats::CreditsSection> sections);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    StateContext& ctx;
    std::vector<formats::CreditsSection> sections_;
    uint64_t enterTicks_ = 0;
    float scrollY_ = 0.0f;

    static constexpr float kScrollSpeed = 40.0f; // pixels per second (game coords)
};

} // namespace runeharbor::engine
