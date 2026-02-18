// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "../../graphics/primitives.hpp"
#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class TitleState : public IGameState
{
  public:
    explicit TitleState(StateContext& ctx);
    ~TitleState() override;

    // UI textures (owned externally by Application; set before use)
    void setBackground(void* tex, int w, int h);
    void setButtonTextures(int index, void* tex, int w, int h);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    void layoutButtons();
    void updateHover();

    StateContext& ctx;

    struct MenuButton
    {
        std::string id;
        graphics::Rect bounds;
        void* hoverTexture = nullptr;
        int textureWidth = 0;
        int textureHeight = 0;
        bool isHovered = false;
    };

    std::vector<MenuButton> buttons;
    int selectedIndex = 0;
    std::string statusMessage;

    // Background texture (non-owning)
    void* background = nullptr;
    int backgroundWidth = 0;
    int backgroundHeight = 0;

    // Button hover textures (non-owning)
    static constexpr int kButtonCount = 4;
    void* buttonTextures[kButtonCount] = {};
    int buttonWidths[kButtonCount] = {};
    int buttonHeights[kButtonCount] = {};

    static const std::vector<std::string> kMenuItems;
};

} // namespace runeharbor::engine
