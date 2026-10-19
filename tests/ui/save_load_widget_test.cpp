// SPDX-License-Identifier: MIT
//
// Navigation and confirmation tests for the in-game save/load slot picker.
// Rendering is not exercised here; the widget exposes selection state and
// queued actions so the InGameState wiring stays thin.
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>

#include <catch2/catch_test_macros.hpp>

#include "../../src/game/game_world.hpp"
#include "../../src/game/save_game.hpp"
#include "../../src/ui/save_load_widget.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor;

namespace
{

class NullLogger : public util::ILogger
{
  public:
    void log(util::LogLevel, std::string_view) override {}
};

struct TempSaveDir
{
    std::filesystem::path path;

    TempSaveDir()
    {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               std::format("rh_saveload_widget_test_{}_{}", stamp, counter++);
        std::filesystem::create_directories(path);
    }
    ~TempSaveDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

ui::UIEvent keyEvent(SDL_Scancode scancode)
{
    return {ui::UIEventType::KeyDown, 0, 0, platform::MouseButton::Left, scancode};
}

} // namespace

TEST_CASE("SaveLoadWidget navigates slots and queues a save action", "[ui][save][widget]")
{
    NullLogger logger;
    TempSaveDir dir;

    game::SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());
    game::GameWorld world;
    REQUIRE(save.save(world, 0));

    ui::SaveLoadWidget widget;
    widget.setSaveGame(&save);
    widget.setBounds(0, 0, 640, 480);
    widget.open(ui::SaveLoadWidget::Mode::Save);

    REQUIRE(widget.visible());
    CHECK(widget.selectedSlot() == 0);
    CHECK(widget.slotExists(0));
    CHECK_FALSE(widget.slotExists(1));

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_DOWN)));
    CHECK(widget.selectedSlot() == 1);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_PAGEDOWN)));
    CHECK(widget.selectedSlot() == 11);

    // Number keys jump within the current page (page start 10, "3" -> 12).
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_3)));
    CHECK(widget.selectedSlot() == 12);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    auto action = widget.takeAction();
    CHECK(action.type == ui::SaveLoadWidget::ActionType::Save);
    CHECK(action.slotIndex == 12);

    // Actions are one-shot.
    CHECK(widget.takeAction().type == ui::SaveLoadWidget::ActionType::None);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_ESCAPE)));
    CHECK_FALSE(widget.visible());
}

TEST_CASE("SaveLoadWidget requires a second Enter to overwrite a slot", "[ui][save][widget]")
{
    NullLogger logger;
    TempSaveDir dir;

    game::SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());
    game::GameWorld world;
    REQUIRE(save.save(world, 0));

    ui::SaveLoadWidget widget;
    widget.setSaveGame(&save);
    widget.setBounds(0, 0, 640, 480);
    widget.open(ui::SaveLoadWidget::Mode::Save);
    CHECK(widget.slotExists(0));

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    CHECK(widget.takeAction().type == ui::SaveLoadWidget::ActionType::None);
    CHECK(widget.visible());

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    const auto action = widget.takeAction();
    CHECK(action.type == ui::SaveLoadWidget::ActionType::Save);
    CHECK(action.slotIndex == 0);

    // Moving to another slot disarms the overwrite confirmation.
    widget.open(ui::SaveLoadWidget::Mode::Save);
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    CHECK(widget.takeAction().type == ui::SaveLoadWidget::ActionType::None);
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_DOWN)));
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_UP)));
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    CHECK(widget.takeAction().type == ui::SaveLoadWidget::ActionType::None);
}

TEST_CASE("SaveLoadWidget refuses to load an empty slot", "[ui][save][widget]")
{
    NullLogger logger;
    TempSaveDir dir;

    game::SaveGame save(logger);
    save.setSaveDirectory(dir.path.string());
    game::GameWorld world;
    REQUIRE(save.save(world, 0));

    ui::SaveLoadWidget widget;
    widget.setSaveGame(&save);
    widget.setBounds(0, 0, 640, 480);
    widget.open(ui::SaveLoadWidget::Mode::Load);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_DOWN)));
    CHECK(widget.selectedSlot() == 1);
    CHECK_FALSE(widget.slotExists(1));

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    CHECK(widget.takeAction().type == ui::SaveLoadWidget::ActionType::None);
    CHECK(widget.visible());

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_UP)));
    CHECK(widget.selectedSlot() == 0);
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    const auto action = widget.takeAction();
    CHECK(action.type == ui::SaveLoadWidget::ActionType::Load);
    CHECK(action.slotIndex == 0);
}
