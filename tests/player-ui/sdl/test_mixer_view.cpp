// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mixer_view.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <variant>
#include <vector>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;
namespace api = djw::core::api;
using namespace std::chrono_literals;

namespace
{
    constexpr api::DeckId first{1};
    constexpr api::DeckId second{2};
    constexpr api::DeckId third{3};

    Point centerOf(const Rect &area)
    {
        return {area.x + (area.width / 2), area.y + (area.height / 2)};
    }

    struct Fixture
    {
            api::CommandQueue                     queue;
            api::MixerSnapshotBuffer              snapshot{api::MixerSnapshot{}};
            Translator                            translator;
            std::vector<MixerDeck>                players{{first, 1, api::CrossfaderSide::A},
                                                          {second, 2, api::CrossfaderSide::B},
                                                          {third, 3, api::CrossfaderSide::None}};
            std::chrono::steady_clock::time_point now{};
            MixerView     view{queue, snapshot, translator, [this] { return players; }, [this] { return now; }};
            PointerRouter router;

            Fixture()
            {
                view.layout({0, 0, 440, 300});
                view.addTo(router);
                view.update();
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            void tapCenter(const Rect &area) { tap(centerOf(area)); }

            /// Opens a drop-down and chooses entry index
            void choose(const DropDown &drop, std::size_t index)
            {
                tapCenter(drop.bounds());
                REQUIRE(drop.isOpen());
                tap(centerOf(drop.itemArea(index)));
                view.update();
            }

            std::vector<api::Command> drain()
            {
                std::vector<api::Command> all;
                while (const auto command = queue.tryPop())
                {
                    all.push_back(*command);
                }
                return all;
            }
    };

    /// The slider position of a value along the slider, from -1 to +1
    Point sliderAt(const Slider &slider, float position)
    {
        const Rect &area = slider.bounds();
        return {area.x + (Slider::knob_width / 2) +
                    ((area.width - Slider::knob_width) * MixerView::sliderValueOf(position)),
                area.y + 5};
    }
} // namespace

TEST_CASE("MixerView: the two drop-downs list the players with none first and show who stands on each side")
{
    Fixture fixture;
    fixture.tapCenter(fixture.view.sideA().bounds());
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("(none)"));
    CHECK(canvas.drew("Player 01"));
    CHECK(canvas.drew("Player 02"));
    CHECK(canvas.drew("Player 03"));
    CHECK(fixture.view.sideA().selectedIndex() == 1); // Player 01 on A
    CHECK(fixture.view.sideB().selectedIndex() == 2); // Player 02 on B
}

TEST_CASE("MixerView: A is left, B is right")
{
    Fixture fixture;
    CHECK(fixture.view.sideA().bounds().x < fixture.view.sideB().bounds().x);
    CHECK(fixture.view.momentaryA().bounds().x < fixture.view.momentaryB().bounds().x);
    CHECK(fixture.view.momentaryA().bounds().x < fixture.view.crossfader().bounds().x);
    CHECK(fixture.view.crossfader().bounds().x < fixture.view.momentaryB().bounds().x);
}

TEST_CASE("MixerView: choosing a player for A puts it on A and takes the one there off")
{
    Fixture fixture;
    fixture.choose(fixture.view.sideA(), 3); // Player 03
    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 2);
    const auto *off = std::get_if<api::MixerAssignCrossfader>(&commands[0]);
    const auto *on  = std::get_if<api::MixerAssignCrossfader>(&commands[1]);
    REQUIRE(off != nullptr);
    REQUIRE(on != nullptr);
    CHECK(off->deck == first);
    CHECK(off->side == api::CrossfaderSide::None);
    CHECK(on->deck == third);
    CHECK(on->side == api::CrossfaderSide::A);
}

TEST_CASE("MixerView: choosing none takes the player off the side")
{
    Fixture fixture;
    fixture.choose(fixture.view.sideB(), 0);
    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    const auto *off = std::get_if<api::MixerAssignCrossfader>(&commands[0]);
    REQUIRE(off != nullptr);
    CHECK(off->deck == second);
    CHECK(off->side == api::CrossfaderSide::None);
}

TEST_CASE("MixerView: a player taken from the other side stands on the new side only")
{
    Fixture fixture;
    fixture.choose(fixture.view.sideA(), 2); // Player 02 is on B, now it goes to A
    const auto commands = fixture.drain();
    // Player 01 steps down from A, Player 02 goes to A (it leaves B by that: a deck has one side)
    REQUIRE(commands.size() == 2);
    CHECK(std::get<api::MixerAssignCrossfader>(commands[0]).deck == first);
    CHECK(std::get<api::MixerAssignCrossfader>(commands[1]).deck == second);
    CHECK(std::get<api::MixerAssignCrossfader>(commands[1]).side == api::CrossfaderSide::A);
}

TEST_CASE("MixerView: the selection follows the players reported")
{
    Fixture fixture;
    fixture.players = {{second, 2, api::CrossfaderSide::A}, {third, 3, api::CrossfaderSide::None}};
    fixture.view.update();
    CHECK(fixture.view.sideA().selectedIndex() == 1);
    CHECK(fixture.view.sideB().selectedIndex() == 0); // nobody: none

    // With no players there is only "(none)"
    fixture.players.clear();
    fixture.view.update();
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK_FALSE(canvas.drew("Player 01"));
}

TEST_CASE("MixerView: the crossfader starts at the position of the mixer")
{
    api::CommandQueue        queue;
    api::MixerSnapshotBuffer snapshot{api::MixerSnapshot{.crossfader_position = 0.5F}};
    Translator               translator;
    MixerView                view{queue, snapshot, translator, [] { return std::vector<MixerDeck>{}; }};
    view.layout({0, 0, 440, 300});
    view.update();
    CHECK(view.crossfader().value() == doctest::Approx(0.75F));
    CHECK_FALSE(queue.tryPop().has_value());
}

TEST_CASE("MixerView: moving the crossfader sends the position from -1 to +1, once per change")
{
    Fixture fixture;
    fixture.router.handle({{1, 1}, PointerPhase::Down, sliderAt(fixture.view.crossfader(), -0.5F)});
    fixture.view.update();
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::MixerSetCrossfader>(commands[0]));
    CHECK(std::get<api::MixerSetCrossfader>(commands[0]).position == doctest::Approx(-0.5F).epsilon(0.03));

    fixture.router.handle({{1, 1}, PointerPhase::Move, sliderAt(fixture.view.crossfader(), 1.0F)});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::get<api::MixerSetCrossfader>(commands[0]).position == doctest::Approx(1.0F));
}

TEST_CASE("MixerView: Zero puts the crossfader in the center")
{
    Fixture fixture;
    fixture.router.handle({{1, 1}, PointerPhase::Down, sliderAt(fixture.view.crossfader(), 1.0F)});
    fixture.router.handle({{1, 1}, PointerPhase::Up, sliderAt(fixture.view.crossfader(), 1.0F)});
    fixture.view.update();
    static_cast<void>(fixture.drain());

    fixture.tapCenter(fixture.view.zeroButton().bounds());
    fixture.view.update();
    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::get<api::MixerSetCrossfader>(commands[0]).position == 0.0F);
    CHECK(fixture.view.crossfader().value() == doctest::Approx(0.5F));
}

TEST_CASE("MixerView: the A and B buttons act as momentary buttons while held")
{
    Fixture     fixture;
    const Point on_b = centerOf(fixture.view.momentaryB().bounds());
    fixture.router.handle({{1, 1}, PointerPhase::Down, on_b});
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::MixerCrossfaderMomentary>(commands[0]));
    CHECK(std::get<api::MixerCrossfaderMomentary>(commands[0]).side == api::CrossfaderSide::B);

    fixture.snapshot.write({.crossfader_momentary = api::CrossfaderSide::B});
    fixture.view.update();
    CHECK(fixture.view.momentaryB().isHighlighted());
    CHECK_FALSE(fixture.view.momentaryA().isHighlighted());

    fixture.router.handle({{1, 1}, PointerPhase::Up, on_b});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::get<api::MixerCrossfaderMomentary>(commands[0]).side == api::CrossfaderSide::None);

    fixture.tapCenter(fixture.view.momentaryA().bounds());
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 2);
    CHECK(std::get<api::MixerCrossfaderMomentary>(commands[0]).side == api::CrossfaderSide::A);
}

TEST_CASE("MixerView: Fade now moves the crossfader to the other side within the fade time")
{
    Fixture fixture;
    // Start at A (-1)
    fixture.router.handle({{1, 1}, PointerPhase::Down, sliderAt(fixture.view.crossfader(), -1.0F)});
    fixture.router.handle({{1, 1}, PointerPhase::Up, sliderAt(fixture.view.crossfader(), -1.0F)});
    fixture.view.update();
    static_cast<void>(fixture.drain());
    CHECK(fixture.view.fadeTime().value() == MixerView::default_fade_seconds);

    fixture.tapCenter(fixture.view.fadeButton().bounds());
    fixture.view.update();
    CHECK(fixture.view.fading());

    fixture.now += 1s; // a fifth of 5 s
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE_FALSE(commands.empty());
    CHECK(std::get<api::MixerSetCrossfader>(commands.back()).position == doctest::Approx(-0.6F).epsilon(0.03));

    fixture.now += 2s + 500ms; // 3.5 s of 5: seven tenths of the way from -1 to +1
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE_FALSE(commands.empty());
    CHECK(std::get<api::MixerSetCrossfader>(commands.back()).position == doctest::Approx(0.4F).epsilon(0.03));

    fixture.now += 10s; // long over
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE_FALSE(commands.empty());
    CHECK(std::get<api::MixerSetCrossfader>(commands.back()).position == doctest::Approx(1.0F));
    CHECK_FALSE(fixture.view.fading());

    // From B it goes back to A
    fixture.tapCenter(fixture.view.fadeButton().bounds());
    fixture.view.update();
    fixture.now += 10s;
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE_FALSE(commands.empty());
    CHECK(std::get<api::MixerSetCrossfader>(commands.back()).position == doctest::Approx(-1.0F));
}

TEST_CASE("MixerView: the fade time can be set, and the user moving the crossfader ends a fade")
{
    Fixture     fixture;
    const Rect &stepper = fixture.view.fadeTime().bounds();
    fixture.tap({stepper.x + stepper.width - 5, stepper.y + 5}); // plus
    CHECK(fixture.view.fadeTime().value() == MixerView::default_fade_seconds + 1);
    fixture.view.update();

    fixture.tapCenter(fixture.view.fadeButton().bounds());
    fixture.view.update();
    REQUIRE(fixture.view.fading());
    fixture.router.handle({{1, 1}, PointerPhase::Down, sliderAt(fixture.view.crossfader(), 0.2F)});
    fixture.view.update();
    CHECK_FALSE(fixture.view.fading());
}

TEST_CASE("MixerView: a command that does not fit into a full queue is sent later")
{
    Fixture fixture;
    while (fixture.queue.tryPush(api::MixerSetMasterVolume{1.0F}))
    {
    }
    fixture.router.handle({{1, 1}, PointerPhase::Down, centerOf(fixture.view.momentaryA().bounds())});
    fixture.view.update();
    static_cast<void>(fixture.drain());
    fixture.view.update();
    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::holds_alternative<api::MixerCrossfaderMomentary>(commands[0]));
}

TEST_CASE("MixerView: the texts follow the language")
{
    Fixture fixture;
    fixture.translator.setLanguage(Language::German);
    fixture.view.update();
    CHECK(fixture.view.zeroButton().label() == "Null");
    CHECK(fixture.view.fadeButton().label() == "Jetzt faden!");
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Player 01"));
    CHECK(canvas.drew("Überblendzeit (s)"));
}

TEST_CASE("MixerView: the master meter lies above the crossfader and shows the master level, clip lamp held a while")
{
    Fixture            fixture;
    api::MixerSnapshot mixer{};
    mixer.master_level = {0.5F, 1.0F};
    fixture.snapshot.write(mixer);
    fixture.view.update();
    CHECK(fixture.view.masterLevel().right == doctest::Approx(1.0F));
    CHECK(fixture.view.masterClip());
    const Rect meter = fixture.view.masterMeterArea();
    CHECK(meter.width > 100);
    CHECK(meter.y + meter.height <= fixture.view.crossfader().bounds().y);

    mixer.master_level = {0.1F, 0.1F};
    fixture.snapshot.write(mixer);
    fixture.now += 2s;
    fixture.view.update();
    CHECK_FALSE(fixture.view.masterClip());

    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(std::ranges::any_of(canvas.rects, [&meter](const Rect &rect)
                              { return rect.y >= meter.y && rect.y + rect.height <= meter.y + meter.height + 0.01F; }));
}
