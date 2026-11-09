// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "deck_view.hpp"

#include <doctest/doctest.h>

#include <variant>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;

namespace
{
    constexpr api::DeckId deck{1};

    struct Fixture
    {
            api::CommandQueue       queue;
            api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = deck}};
            DeckView                view{deck, queue, snapshot};
            PointerRouter           router;

            Fixture()
            {
                view.layout({0, 0, 300, 600});
                view.addTo(router);
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            Point buttonCenter() const
            {
                const Rect &area = view.playButton().bounds();
                return {area.x + area.width / 2, area.y + area.height / 2};
            }
    };
} // namespace

TEST_CASE("DeckView: button, position and fader are laid out top to bottom inside the area")
{
    Fixture     fixture;
    const Rect &button = fixture.view.playButton().bounds();
    const Rect &fader  = fixture.view.fader().bounds();

    CHECK(button.width > 0);
    CHECK(button.height > 0);
    CHECK(fader.y > button.y + button.height);
    CHECK(fader.y + fader.height <= 600);
}

TEST_CASE("DeckView: tiny areas give empty widgets instead of negative sizes")
{
    Fixture fixture;
    fixture.view.layout({0, 0, 10, 10});

    CHECK(fixture.view.playButton().bounds().width >= 0);
    CHECK(fixture.view.fader().bounds().height >= 0);
}

TEST_CASE("DeckView: pressing play on a stopped deck sends DeckPlay")
{
    Fixture fixture;

    fixture.tap(fixture.buttonCenter());
    fixture.view.update();

    const auto command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    REQUIRE(std::holds_alternative<api::DeckPlay>(*command));
    CHECK(std::get<api::DeckPlay>(*command).deck == deck);
}

TEST_CASE("DeckView: pressing the button on a playing deck sends DeckPause")
{
    Fixture fixture;
    fixture.snapshot.write({.deck = deck, .play_state = api::PlayState::Playing});

    fixture.tap(fixture.buttonCenter());
    fixture.view.update();

    const auto command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    CHECK(std::holds_alternative<api::DeckPause>(*command));
}

TEST_CASE("DeckView: moving the fader sends the channel volume once per change")
{
    Fixture     fixture;
    const Rect &fader = fixture.view.fader().bounds();
    const Point middle{fader.x + fader.width / 2, fader.y + fader.height / 2};

    fixture.router.handle({{1, 1}, PointerPhase::Down, middle});
    fixture.view.update();
    fixture.view.update();

    const auto command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    REQUIRE(std::holds_alternative<api::MixerSetChannelVolume>(*command));
    CHECK(std::get<api::MixerSetChannelVolume>(*command).value == doctest::Approx(0.5F));
    CHECK_FALSE(fixture.queue.tryPop().has_value());
}

TEST_CASE("DeckView: commands that do not fit into a full queue are sent later")
{
    Fixture fixture;
    while (fixture.queue.tryPush(api::DeckPause{deck}))
    {
    }

    fixture.tap(fixture.buttonCenter());
    const Rect &fader = fixture.view.fader().bounds();
    fixture.router.handle({{1, 2}, PointerPhase::Down, {fader.x + 1, fader.y + fader.height - 1}});
    fixture.view.update();

    while (fixture.queue.tryPop())
    {
    }
    fixture.view.update();

    const auto first  = fixture.queue.tryPop();
    const auto second = fixture.queue.tryPop();
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(std::holds_alternative<api::DeckPlay>(*first));
    REQUIRE(std::holds_alternative<api::MixerSetChannelVolume>(*second));
    CHECK(std::get<api::MixerSetChannelVolume>(*second).value == doctest::Approx(0.0F));
}

TEST_CASE("DeckView: the fader starts at the volume of the snapshot")
{
    api::CommandQueue       queue;
    api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = deck, .channel_volume = 0.4F}};
    DeckView                view{deck, queue, snapshot};

    view.update();

    CHECK(view.fader().value() == doctest::Approx(0.4F));
    CHECK_FALSE(queue.tryPop().has_value());
}
