// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

namespace runeharbor::engine
{

enum class GameStateId
{
    IntroVideo,
    TitleScreen,
    CharacterCreation,
    Loading,
    InGame,
    Quit,
};

/// Interface for game state implementations.
/// Each state handles its own update logic and rendering.
class IGameState
{
  public:
    virtual ~IGameState() = default;

    /// Called when this state becomes active
    virtual void enter() = 0;

    /// Called when this state is being replaced
    virtual void exit() = 0;

    /// Update logic. Returns a state ID to transition to, or nullopt to stay.
    virtual std::optional<GameStateId> update() = 0;

    /// Render the state's visuals
    virtual void render() = 0;
};

} // namespace runeharbor::engine
