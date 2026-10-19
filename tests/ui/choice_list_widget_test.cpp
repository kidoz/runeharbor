// SPDX-License-Identifier: MIT
//
// Navigation/selection tests for the modal choice menu used by Town Portal and
// Lloyd's Beacon. Rendering is not exercised; selections are queued via
// takeSelection().
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/ui/choice_list_widget.hpp"

using namespace runeharbor;
using namespace runeharbor::ui;

namespace
{
UIEvent keyEvent(SDL_Scancode scancode)
{
    return {UIEventType::KeyDown, 0, 0, platform::MouseButton::Left, scancode};
}
} // namespace

TEST_CASE("ChoiceListWidget reports the selected option", "[ui][choice]")
{
    ChoiceListWidget widget;
    widget.setBounds(0, 0, 640, 480);
    widget.open("Choose", {{.label = "First", .id = 10}, {.label = "Second", .id = 20}});

    REQUIRE(widget.visible());
    CHECK(widget.hasChoices());
    CHECK(widget.selectedIndex() == 0);
    CHECK(widget.selectedId() == 10);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_DOWN)));
    CHECK(widget.selectedIndex() == 1);
    CHECK(widget.selectedId() == 20);

    // Wraps.
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_DOWN)));
    CHECK(widget.selectedIndex() == 0);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_UP)));
    CHECK(widget.selectedIndex() == 1);

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    CHECK_FALSE(widget.visible());
    CHECK(widget.takeSelection() == 20);

    // One-shot.
    CHECK(widget.takeSelection() == -1);
}

TEST_CASE("ChoiceListWidget cancel and empty list are no-ops", "[ui][choice]")
{
    ChoiceListWidget widget;
    widget.setBounds(0, 0, 640, 480);
    widget.open("Choose", {{.label = "Only", .id = 1}});

    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_ESCAPE)));
    CHECK_FALSE(widget.visible());
    CHECK(widget.takeSelection() == -1);

    widget.open("Empty", {});
    CHECK_FALSE(widget.hasChoices());
    REQUIRE(widget.handleEvent(keyEvent(SDL_SCANCODE_RETURN)));
    CHECK(widget.takeSelection() == -1);
}
