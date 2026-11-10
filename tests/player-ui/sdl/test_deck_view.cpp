// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "deck_view.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <variant>
#include <vector>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;
namespace api = djw::core::api;

namespace
{
    constexpr api::DeckId deck{1};

    Point centerOf(const Rect &area)
    {
        return {area.x + (area.width / 2), area.y + (area.height / 2)};
    }

    struct Fixture
    {
            api::CommandQueue       queue;
            api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = deck}};
            Translator              translator;
            DeckView                view{deck, queue, snapshot, translator};
            PointerRouter           router;
            int                     loads = 0;

            Fixture()
            {
                view.setOnLoad([this] { ++loads; });
                view.layout({0, 0, 380, 480});
                view.addTo(router);
                view.update();
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            void tapCenter(const Rect &area) { tap(centerOf(area)); }

            void show(const api::DeckSnapshot &state)
            {
                snapshot.write(state);
                view.update();
            }

            /// Everything in the queue, in order
            std::vector<api::Command> drain()
            {
                std::vector<api::Command> all;
                while (const auto command = queue.tryPop())
                {
                    all.push_back(*command);
                }
                return all;
            }

            [[nodiscard]] static bool drew(const RecordingCanvas &canvas, std::string_view text)
            {
                return canvas.drew(text);
            }
    };

    template <typename Command>
    const Command *find(const std::vector<api::Command> &commands)
    {
        for (const api::Command &command : commands)
        {
            if (const auto *found = std::get_if<Command>(&command))
            {
                return found;
            }
        }
        return nullptr;
    }

    bool inside(const Rect &part, const Rect &whole)
    {
        return part.x >= whole.x && part.y >= whole.y && part.x + part.width <= whole.x + whole.width + 0.01F &&
               part.y + part.height <= whole.y + whole.height + 0.01F;
    }
} // namespace

TEST_CASE("DeckView: the Load button is at the top left, everything lies inside the window")
{
    Fixture     fixture;
    const Rect  window{0, 0, 380, 480};
    const Rect &load = fixture.view.loadButton().bounds();
    CHECK(load.width > 0);
    CHECK(load.height > 0);
    CHECK(load.x < 20);
    CHECK(load.y < 20);

    for (const Rect *area :
         {&fixture.view.loadButton().bounds(), &fixture.view.bendDownButton().bounds(),
          &fixture.view.bendUpButton().bounds(), &fixture.view.keylockButton().bounds(),
          &fixture.view.timeButton().bounds(), &fixture.view.pitchZeroButton().bounds(),
          &fixture.view.cueButton().bounds(), &fixture.view.playButton().bounds(),
          &fixture.view.headphoneButton().bounds(), &fixture.view.positionSlider().bounds(),
          &fixture.view.fader().bounds(), &fixture.view.headphoneFader().bounds(), &fixture.view.pitchFader().bounds(),
          &fixture.view.cueLoadBox().bounds()})
    {
        CHECK(area->width > 0);
        CHECK(area->height > 0);
        CHECK(inside(*area, window));
    }
}

TEST_CASE("DeckView: the pitch fader sits right of the display, the transport row below the position slider")
{
    Fixture fixture;
    CHECK(fixture.view.pitchFader().bounds().x > fixture.view.playButton().bounds().x + fixture.view.playButton().bounds().width);
    CHECK(fixture.view.cueButton().bounds().y > fixture.view.positionSlider().bounds().y);
    CHECK(fixture.view.playButton().bounds().y == fixture.view.cueButton().bounds().y);
    CHECK(fixture.view.fader().bounds().y > fixture.view.cueButton().bounds().y + fixture.view.cueButton().bounds().height);
}

TEST_CASE("DeckView: tiny areas give empty widgets instead of negative sizes")
{
    Fixture fixture;
    fixture.view.layout({0, 0, 10, 10});
    CHECK(fixture.view.playButton().bounds().width >= 0);
    CHECK(fixture.view.fader().bounds().height >= 0);
    CHECK(fixture.view.pitchFader().bounds().height >= 0);
    CHECK(fixture.view.positionSlider().bounds().width >= 0);
    CHECK(fixture.view.cueLoadBox().bounds().height >= 0);
}

TEST_CASE("DeckView: pressing play on a stopped deck sends DeckPlay")
{
    Fixture fixture;
    fixture.tapCenter(fixture.view.playButton().bounds());
    fixture.view.update();

    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::DeckPlay>(commands[0]));
    CHECK(std::get<api::DeckPlay>(commands[0]).deck == deck);
}

TEST_CASE("DeckView: pressing the button on a playing deck sends DeckPause")
{
    Fixture fixture;
    fixture.show({.deck = deck, .play_state = api::PlayState::Playing});
    fixture.tapCenter(fixture.view.playButton().bounds());
    fixture.view.update();

    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::holds_alternative<api::DeckPause>(commands[0]));
}

TEST_CASE("DeckView: moving the channel fader sends the volume once per change")
{
    Fixture fixture;
    fixture.router.handle({{1, 1}, PointerPhase::Down, centerOf(fixture.view.fader().bounds())});
    fixture.view.update();
    fixture.view.update();

    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::MixerSetChannelVolume>(commands[0]));
    CHECK(std::get<api::MixerSetChannelVolume>(commands[0]).value == doctest::Approx(0.5F).epsilon(0.1));
}

TEST_CASE("DeckView: commands that do not fit into a full queue are sent later, in order")
{
    Fixture fixture;
    while (fixture.queue.tryPush(api::DeckPause{deck}))
    {
    }

    fixture.tapCenter(fixture.view.playButton().bounds());
    const Rect &fader = fixture.view.fader().bounds();
    fixture.router.handle({{1, 2}, PointerPhase::Down, {fader.x + 1, fader.y + fader.height - 1}});
    fixture.view.update();
    static_cast<void>(fixture.drain());
    fixture.view.update();

    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 2);
    CHECK(std::holds_alternative<api::DeckPlay>(commands[0]));
    REQUIRE(std::holds_alternative<api::MixerSetChannelVolume>(commands[1]));
    CHECK(std::get<api::MixerSetChannelVolume>(commands[1]).value == doctest::Approx(0.0F));
}

TEST_CASE("DeckView: the faders start at the values of the snapshot and send nothing")
{
    api::CommandQueue       queue;
    api::DeckSnapshotBuffer snapshot{
        api::DeckSnapshot{.deck = deck, .channel_volume = 0.4F, .pitch_percent = 6.0F, .headphone_level = 0.7F}};
    Translator translator;
    DeckView   view{deck, queue, snapshot, translator};
    view.update();
    CHECK(view.fader().value() == doctest::Approx(0.4F));
    CHECK(view.headphoneFader().value() == doctest::Approx(0.7F));
    CHECK(view.pitchFader().value() == doctest::Approx(0.75F));
    CHECK_FALSE(queue.tryPop().has_value());
}

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

TEST_CASE("DeckView: the pitch fader sends the pitch once per change, Zero puts it back")
{
    Fixture     fixture;
    const Rect &fader = fixture.view.pitchFader().bounds();
    fixture.router.handle({{1, 1}, PointerPhase::Down, {fader.x + (fader.width / 2), fader.y + 1}});
    fixture.router.handle({{1, 1}, PointerPhase::Up, {fader.x + (fader.width / 2), fader.y + 1}});
    fixture.view.update();
    fixture.view.update();

    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::DeckSetPitch>(commands[0]));
    CHECK(std::get<api::DeckSetPitch>(commands[0]).percent > 10.0F);

    fixture.tapCenter(fixture.view.pitchZeroButton().bounds());
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::DeckSetPitch>(commands[0]));
    CHECK(std::get<api::DeckSetPitch>(commands[0]).percent == 0.0F);
    CHECK(fixture.view.pitchFader().value() == 0.5F);
}

TEST_CASE("DeckView: the KEY button toggles keylock from the state shown")
{
    Fixture fixture;
    fixture.tapCenter(fixture.view.keylockButton().bounds());
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::DeckSetKeylock>(commands[0]));
    CHECK(std::get<api::DeckSetKeylock>(commands[0]).enabled);
    CHECK_FALSE(fixture.view.keylockButton().isHighlighted());

    fixture.show({.deck = deck, .keylock = true});
    CHECK(fixture.view.keylockButton().isHighlighted());
    fixture.tapCenter(fixture.view.keylockButton().bounds());
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK_FALSE(std::get<api::DeckSetKeylock>(commands[0]).enabled);
}

TEST_CASE("DeckView: the Load button asks the owner, and is off while the player plays")
{
    Fixture fixture;
    CHECK(fixture.view.loadButton().isEnabled());
    fixture.tapCenter(fixture.view.loadButton().bounds());
    fixture.view.update();
    CHECK(fixture.loads == 1);

    fixture.show({.deck = deck, .play_state = api::PlayState::Playing});
    CHECK_FALSE(fixture.view.loadButton().isEnabled());
    fixture.tapCenter(fixture.view.loadButton().bounds());
    fixture.view.update();
    CHECK(fixture.loads == 1);

    fixture.show({.deck = deck, .play_state = api::PlayState::Stopped});
    CHECK(fixture.view.loadButton().isEnabled());
}

TEST_CASE("DeckView: without an owner the Load button is off")
{
    api::CommandQueue       queue;
    api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = deck}};
    Translator              translator;
    DeckView                view{deck, queue, snapshot, translator};
    view.update();
    CHECK_FALSE(view.loadButton().isEnabled());
}

TEST_CASE("DeckView: CUE sends its press and its release, also when both come within one frame")
{
    Fixture fixture;
    const Point on = centerOf(fixture.view.cueButton().bounds());
    fixture.router.handle({{1, 1}, PointerPhase::Down, on});
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::DeckCue>(commands[0]));
    CHECK(std::get<api::DeckCue>(commands[0]).pressed);

    // Released away from the button: still a release
    fixture.router.handle({{1, 1}, PointerPhase::Up, {5, 470}});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK_FALSE(std::get<api::DeckCue>(commands[0]).pressed);

    fixture.tap(on); // a quick tap: down and up before the next update
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 2);
    CHECK(std::get<api::DeckCue>(commands[0]).pressed);
    CHECK_FALSE(std::get<api::DeckCue>(commands[1]).pressed);
}

TEST_CASE("DeckView: the bend buttons act while held")
{
    Fixture     fixture;
    const Point faster = centerOf(fixture.view.bendUpButton().bounds());
    fixture.router.handle({{1, 1}, PointerPhase::Down, faster});
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::DeckPitchBend>(commands[0]));
    CHECK(std::get<api::DeckPitchBend>(commands[0]).direction == 1);
    fixture.router.handle({{1, 1}, PointerPhase::Up, faster});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::get<api::DeckPitchBend>(commands[0]).direction == 0);

    const Point slower = centerOf(fixture.view.bendDownButton().bounds());
    fixture.router.handle({{1, 1}, PointerPhase::Down, slower});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK(std::get<api::DeckPitchBend>(commands[0]).direction == -1);
}

TEST_CASE("DeckView: the position slider shows the position and seeks when the user moves it")
{
    Fixture fixture;
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 50.0, .duration_seconds = 200.0});
    CHECK(fixture.view.positionSlider().value() == doctest::Approx(0.25F));

    const Rect &slider = fixture.view.positionSlider().bounds();
    // Three quarters along the track of the slider
    const float target = slider.x + (Slider::knob_width / 2) + ((slider.width - Slider::knob_width) * 0.75F);
    fixture.router.handle({{1, 1}, PointerPhase::Down, {target, slider.y + 5}});
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 51.0, .duration_seconds = 200.0});
    // While held the slider keeps the finger's value
    CHECK(fixture.view.positionSlider().value() == doctest::Approx(0.75F).epsilon(0.02));

    const auto commands = fixture.drain();
    const auto *seek    = find<api::DeckSeek>(commands);
    REQUIRE(seek != nullptr);
    CHECK(seek->position_seconds == doctest::Approx(150.0).epsilon(0.02));

    fixture.router.handle({{1, 1}, PointerPhase::Up, {target, slider.y + 5}});
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 150.0, .duration_seconds = 200.0});
    CHECK(fixture.view.positionSlider().value() == doctest::Approx(0.75F));
}

TEST_CASE("DeckView: without a track the slider does not seek")
{
    Fixture fixture;
    const Rect &slider = fixture.view.positionSlider().bounds();
    fixture.router.handle({{1, 1}, PointerPhase::Down, {slider.x + 100, slider.y + 5}});
    fixture.view.update();
    CHECK(find<api::DeckSeek>(fixture.drain()) == nullptr);
}

TEST_CASE("DeckView: the display shows the elapsed time, the Time button switches to the remaining time")
{
    Fixture fixture;
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 1.0, .duration_seconds = 240.0});
    RecordingCanvas elapsed;
    fixture.view.draw(elapsed);
    CHECK(elapsed.drew("00:00:01:00"));

    fixture.tapCenter(fixture.view.timeButton().bounds());
    fixture.view.update();
    RecordingCanvas remaining;
    fixture.view.draw(remaining);
    CHECK(remaining.drew("-00:03:59:00"));
    CHECK(fixture.view.timeButton().isHighlighted());

    fixture.tapCenter(fixture.view.timeButton().bounds());
    fixture.view.update();
    RecordingCanvas again;
    fixture.view.draw(again);
    CHECK(again.drew("00:00:01:00"));
}

TEST_CASE("DeckView: the display shows the pitch and the track")
{
    Fixture fixture;
    fixture.show({.deck = deck, .pitch_percent = 0.0F});
    fixture.view.setTrack("Bill Withers", "Just The Two Of Us");
    const Rect &pitch = fixture.view.pitchFader().bounds();
    fixture.router.handle({{1, 1}, PointerPhase::Down, {pitch.x + 5, pitch.y + pitch.height * 0.75F}});
    fixture.view.update();

    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Bill Withers"));
    CHECK(canvas.drew("Just The Two Of Us"));
    bool found = false;
    for (const auto &drawn : canvas.texts)
    {
        found = found || (drawn.text.starts_with("Pitch: -") && drawn.text.ends_with(" %"));
    }
    CHECK(found);
    CHECK(fixture.view.artist() == "Bill Withers");
    CHECK(fixture.view.trackTitle() == "Just The Two Of Us");
}

TEST_CASE("DeckView: a message takes the place of the track in the display")
{
    Fixture fixture;
    fixture.view.setTrack("Artist", "Title");
    fixture.view.setMessage("Cannot load: x");
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Cannot load: x"));
    CHECK_FALSE(canvas.drew("Artist"));
}

TEST_CASE("DeckView: the state is loaded, cued, paused or playing")
{
    Fixture fixture;
    CHECK(fixture.view.stateText().empty()); // no track

    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 0.0, .cue_seconds = 0.0});
    CHECK(fixture.view.stateText() == "loaded");
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 12.0, .cue_seconds = 12.0});
    CHECK(fixture.view.stateText() == "cued");
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 30.0, .cue_seconds = 12.0});
    CHECK(fixture.view.stateText() == "paused");
    fixture.show({.deck = deck,
                  .track_loaded = true,
                  .play_state   = api::PlayState::Playing,
                  .position_seconds = 31.0,
                  .cue_seconds      = 12.0});
    CHECK(fixture.view.stateText() == "playing");
    CHECK(fixture.view.playing());
}

TEST_CASE("DeckView: the headphone button toggles the cue from the state shown, the headphone fader sends its level")
{
    Fixture fixture;
    fixture.tapCenter(fixture.view.headphoneButton().bounds());
    fixture.view.update();
    auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::MixerSetHeadphoneCue>(commands[0]));
    CHECK(std::get<api::MixerSetHeadphoneCue>(commands[0]).enabled);

    fixture.show({.deck = deck, .headphone_cue = true});
    CHECK(fixture.view.headphoneButton().isHighlighted());
    fixture.tapCenter(fixture.view.headphoneButton().bounds());
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    CHECK_FALSE(std::get<api::MixerSetHeadphoneCue>(commands[0]).enabled);

    const Rect &level = fixture.view.headphoneFader().bounds();
    fixture.router.handle({{1, 1}, PointerPhase::Down, {level.x + 5, level.y + level.height - 1}});
    fixture.view.update();
    commands = fixture.drain();
    const auto *sent = find<api::MixerSetHeadphoneLevel>(commands);
    REQUIRE(sent != nullptr);
    CHECK(sent->value == doctest::Approx(0.0F));
}

TEST_CASE("DeckView: with Cue-Load on a loaded track goes to the headphones")
{
    Fixture fixture;
    fixture.view.setTrack("A", "B");
    CHECK(fixture.drain().empty()); // off by default

    fixture.tapCenter(fixture.view.cueLoadBox().bounds());
    fixture.view.update();
    CHECK(fixture.view.cueLoadBox().checked());
    CHECK(fixture.drain().empty()); // switching it on does not touch the headphones

    fixture.view.setTrack("C", "D");
    fixture.view.update();
    const auto commands = fixture.drain();
    REQUIRE(commands.size() == 1);
    REQUIRE(std::holds_alternative<api::MixerSetHeadphoneCue>(commands[0]));
    CHECK(std::get<api::MixerSetHeadphoneCue>(commands[0]).enabled);
}

TEST_CASE("DeckView: the texts follow the language")
{
    Fixture fixture;
    fixture.translator.setLanguage(Language::German);
    fixture.show({.deck = deck, .track_loaded = true});
    CHECK(fixture.view.loadButton().label() == "Laden");
    CHECK(fixture.view.timeButton().label() == "Zeit");
    CHECK(fixture.view.stateText() == "geladen");
    CHECK(fixture.view.cueLoadBox().label() == "Cue-Load");
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Kopf"));

    fixture.translator.setLanguage(Language::Russian);
    fixture.view.update();
    CHECK(fixture.view.loadButton().label() == "Загрузить");
}
