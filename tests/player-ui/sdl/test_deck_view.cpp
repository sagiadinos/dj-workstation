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

namespace
{
    Point centerOf(const Rect &area)
    {
        return {area.x + area.width / 2, area.y + area.height / 2};
    }
} // namespace

TEST_CASE("DeckView: pitchOf maps the fader to tenths of a percent with a detent at the center")
{
    CHECK(DeckView::pitchOf(0.5F) == 0.0F);
    CHECK(DeckView::pitchOf(1.0F) == doctest::Approx(12.0F));
    CHECK(DeckView::pitchOf(0.0F) == doctest::Approx(-12.0F));
    CHECK(DeckView::pitchOf(0.5F + 0.002F) == 0.0F); // within the detent
    CHECK(DeckView::pitchOf(0.75F) == doctest::Approx(6.0F));
    CHECK(DeckView::pitchOf(2.0F) == doctest::Approx(12.0F)); // clamped
    CHECK(DeckView::faderValueOf(DeckView::pitchOf(0.7F)) == doctest::Approx(0.7F).epsilon(0.01));
    CHECK(DeckView::faderValueOf(0.0F) == 0.5F);
}

TEST_CASE("DeckView: the pitch fader sends the pitch once per change")
{
    Fixture     fixture;
    const Rect &fader = fixture.view.pitchFader().bounds();
    CHECK(fader.height > 0);
    CHECK(fixture.view.pitchFader().value() == 0.5F);

    fixture.router.handle({{1, 1}, PointerPhase::Down, {fader.x + (fader.width / 2), fader.y + 1}});
    fixture.view.update();
    fixture.view.update();

    const auto command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    REQUIRE(std::holds_alternative<api::DeckSetPitch>(*command));
    CHECK(std::get<api::DeckSetPitch>(*command).percent > 10.0F);
    CHECK_FALSE(fixture.queue.tryPop().has_value());
}

TEST_CASE("DeckView: the 0 button puts the pitch back to zero")
{
    Fixture     fixture;
    const Rect &fader = fixture.view.pitchFader().bounds();
    fixture.router.handle({{1, 1}, PointerPhase::Down, {fader.x + (fader.width / 2), fader.y + 1}});
    fixture.router.handle({{1, 1}, PointerPhase::Up, {fader.x + (fader.width / 2), fader.y + 1}});
    fixture.view.update();
    static_cast<void>(fixture.queue.tryPop());

    fixture.tap(centerOf(fixture.view.pitchZeroButton().bounds()));
    fixture.view.update();

    const auto command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    REQUIRE(std::holds_alternative<api::DeckSetPitch>(*command));
    CHECK(std::get<api::DeckSetPitch>(*command).percent == 0.0F);
    CHECK(fixture.view.pitchFader().value() == 0.5F);
}

TEST_CASE("DeckView: the pitch fader starts at the pitch of the snapshot and sends nothing")
{
    api::CommandQueue       queue;
    api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = deck, .pitch_percent = 6.0F}};
    DeckView                view{deck, queue, snapshot};
    view.update();
    CHECK(view.pitchFader().value() == doctest::Approx(0.75F));
    CHECK_FALSE(queue.tryPop().has_value());
}

TEST_CASE("DeckView: the KEY button toggles keylock from the state shown")
{
    Fixture fixture;
    fixture.tap(centerOf(fixture.view.keylockButton().bounds()));
    fixture.view.update();
    auto command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    REQUIRE(std::holds_alternative<api::DeckSetKeylock>(*command));
    CHECK(std::get<api::DeckSetKeylock>(*command).enabled);
    CHECK_FALSE(fixture.view.keylockButton().isHighlighted());

    fixture.snapshot.write({.deck = deck, .keylock = true});
    fixture.view.update();
    CHECK(fixture.view.keylockButton().isHighlighted());
    fixture.tap(centerOf(fixture.view.keylockButton().bounds()));
    fixture.view.update();
    command = fixture.queue.tryPop();
    REQUIRE(command.has_value());
    REQUIRE(std::holds_alternative<api::DeckSetKeylock>(*command));
    CHECK_FALSE(std::get<api::DeckSetKeylock>(*command).enabled);
}

TEST_CASE("DeckView: the pitch shows in percent with a sign")
{
    Fixture fixture;
    fixture.snapshot.write({.deck = deck, .pitch_percent = -3.5F});
    fixture.view.update();
    // sent_pitch follows the fader, not the snapshot: moving the fader to -3.5 % shows it
    const Rect &fader = fixture.view.pitchFader().bounds();
    // Fader value 0.5 - 3.5/24 = 0.354 from the bottom
    fixture.router.handle({{1, 1}, PointerPhase::Down, {fader.x + 1, fader.y + (fader.height * (1.0F - 0.354F))}});
    fixture.view.update();
    struct Canvas final : djw::player_ui::sdl::Canvas
    {
            void  fillRect(const Rect &, Color) override {}
            void  fillTriangle(Point, Point, Point, Color) override {}
            void  text(Point, std::string_view value, Color) override { texts.emplace_back(value); }
            void  textElided(Point, std::string_view value, Color, float) override { texts.emplace_back(value); }
            float textHeight() const noexcept override { return 10.0F; }
            float textWidth(std::string_view) const noexcept override { return 10.0F; }
            void  setClip(std::optional<Rect>) override {}
            std::vector<std::string> texts;
    } canvas;
    fixture.view.draw(canvas);
    bool found = false;
    for (const auto &text : canvas.texts)
    {
        found = found || (text.size() > 2 && text.front() == '-' && text.ends_with(" %"));
    }
    CHECK(found);
}
