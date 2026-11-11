// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "deck_view.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <chrono>
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
                view.layout({0, 0, 380, 514});
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

    template <typename Command> const Command *find(const std::vector<api::Command> &commands)
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
    const Rect  window{0, 0, 380, 514};
    const Rect &load = fixture.view.loadButton().bounds();
    CHECK(load.width > 0);
    CHECK(load.height > 0);
    CHECK(load.x < 20);
    CHECK(load.y < 20);

    for (const Rect *area :
         {&fixture.view.loadButton().bounds(), &fixture.view.bendDownButton().bounds(),
          &fixture.view.bendUpButton().bounds(), &fixture.view.keylockButton().bounds(),
          &fixture.view.timeButton().bounds(), &fixture.view.cueButton().bounds(), &fixture.view.playButton().bounds(),
          &fixture.view.headphoneButton().bounds(), &fixture.view.waveform().bounds(), &fixture.view.fader().bounds(),
          &fixture.view.headphoneFader().bounds(), &fixture.view.pitchFader().bounds(),
          &fixture.view.cueLoadBox().bounds()})
    {
        CHECK(area->width > 0);
        CHECK(area->height > 0);
        CHECK(inside(*area, window));
    }
}

TEST_CASE("DeckView: the pitch fader sits right of the display, the transport row below the waveform")
{
    Fixture fixture;
    CHECK(fixture.view.pitchFader().bounds().x >
          fixture.view.playButton().bounds().x + fixture.view.playButton().bounds().width);
    CHECK(fixture.view.cueButton().bounds().y > fixture.view.waveform().bounds().y);
    CHECK(fixture.view.playButton().bounds().y == fixture.view.cueButton().bounds().y);
    CHECK(fixture.view.fader().bounds().y >
          fixture.view.cueButton().bounds().y + fixture.view.cueButton().bounds().height);
}

TEST_CASE("DeckView: tiny areas give empty widgets instead of negative sizes")
{
    Fixture fixture;
    fixture.view.layout({0, 0, 10, 10});
    CHECK(fixture.view.playButton().bounds().width >= 0);
    CHECK(fixture.view.fader().bounds().height >= 0);
    CHECK(fixture.view.pitchFader().bounds().height >= 0);
    CHECK(fixture.view.waveform().bounds().width >= 0);
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

    // A double tap on the pitch fader sets the pitch to 0
    fixture.tapCenter(fixture.view.pitchFader().bounds());
    fixture.tapCenter(fixture.view.pitchFader().bounds());
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
    Fixture     fixture;
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

TEST_CASE("DeckView: the waveform shows the position and the cue point and seeks when the user touches it")
{
    Fixture fixture;
    fixture.show({.deck             = deck,
                  .track_loaded     = true,
                  .position_seconds = 50.0,
                  .duration_seconds = 200.0,
                  .cue_seconds      = 100.0});
    CHECK(fixture.view.waveform().position() == doctest::Approx(0.25F));

    const Rect &bar   = fixture.view.waveform().bounds();
    const Rect  inner = bar.inset(3.0F);
    // Three quarters along the track
    const float target = inner.x + (inner.width * 0.75F);
    fixture.router.handle({{1, 1}, PointerPhase::Down, {target, bar.y + 5}});
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 51.0, .duration_seconds = 200.0});
    // While held the waveform keeps the finger's position
    CHECK(fixture.view.waveform().position() == doctest::Approx(0.75F).epsilon(0.02));

    auto        commands = fixture.drain();
    const auto *scrub    = find<api::DeckScrub>(commands);
    REQUIRE(scrub != nullptr);
    CHECK(scrub->active);
    CHECK(scrub->position_seconds == doctest::Approx(150.0).epsilon(0.02));

    // Dragging on moves the loop; letting go leaves it running (CUE or play end it, in the deck)
    const float further = inner.x + (inner.width * 0.9F);
    fixture.router.handle({{1, 1}, PointerPhase::Move, {further, bar.y + 5}});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(find<api::DeckScrub>(commands) != nullptr);
    CHECK(find<api::DeckScrub>(commands)->position_seconds == doctest::Approx(180.0).epsilon(0.02));

    fixture.router.handle({{1, 1}, PointerPhase::Up, {further, bar.y + 5}});
    fixture.view.update();
    commands = fixture.drain();
    CHECK(find<api::DeckScrub>(commands) == nullptr);
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 180.0, .duration_seconds = 200.0});
    CHECK(fixture.view.waveform().position() == doctest::Approx(0.9F));
}

TEST_CASE("DeckView: without a track the waveform does not seek")
{
    Fixture     fixture;
    const Rect &bar = fixture.view.waveform().bounds();
    fixture.router.handle({{1, 1}, PointerPhase::Down, {bar.x + 100, bar.y + 5}});
    fixture.view.update();
    CHECK(find<api::DeckScrub>(fixture.drain()) == nullptr);
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
    fixture.show({.deck             = deck,
                  .track_loaded     = true,
                  .play_state       = api::PlayState::Playing,
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
    commands         = fixture.drain();
    const auto *sent = find<api::MixerSetHeadphoneLevel>(commands);
    REQUIRE(sent != nullptr);
    CHECK(sent->value == doctest::Approx(0.0F));
}

TEST_CASE("DeckView: the Cue-Load option is off at first, a tap switches it, loading a track sends nothing")
{
    Fixture fixture;
    CHECK_FALSE(fixture.view.cueLoad());
    fixture.tapCenter(fixture.view.cueLoadBox().bounds());
    fixture.view.update();
    CHECK(fixture.view.cueLoad());
    fixture.view.setTrack("C", "D");
    fixture.view.update();
    CHECK(fixture.drain().empty()); // the owner loads the track at its cue point, the view sends no command for it

    fixture.tapCenter(fixture.view.cueLoadBox().bounds());
    CHECK_FALSE(fixture.view.cueLoad());
    fixture.view.setCueLoad(true);
    CHECK(fixture.view.cueLoad());
}

TEST_CASE("DeckView: a cue point the user set is reported once, one that came with a track is not")
{
    Fixture             fixture;
    std::vector<double> reported;
    fixture.view.setOnCueSet([&reported](double seconds) { reported.push_back(seconds); });

    // A track loaded at its stored cue point: the cue seconds change, the count does not
    fixture.show({.deck = deck, .track_loaded = true, .cue_seconds = 42.0, .cue_set_count = 0});
    fixture.show({.deck = deck, .track_loaded = true, .cue_seconds = 42.0, .cue_set_count = 0});
    CHECK(reported.empty());

    // The user sets a cue point with CUE
    fixture.show({.deck = deck, .track_loaded = true, .cue_seconds = 12.5, .cue_set_count = 1});
    REQUIRE(reported.size() == 1);
    CHECK(reported[0] == 12.5);
    fixture.show({.deck = deck, .track_loaded = true, .cue_seconds = 12.5, .cue_set_count = 1}); // the same again
    CHECK(reported.size() == 1);

    // Another track comes with its own cue point
    fixture.show({.deck = deck, .track_loaded = true, .cue_seconds = 99.0, .cue_set_count = 1});
    CHECK(reported.size() == 1);

    fixture.show({.deck = deck, .track_loaded = true, .cue_seconds = 3.0, .cue_set_count = 2});
    REQUIRE(reported.size() == 2);
    CHECK(reported[1] == 3.0);
}

TEST_CASE("DeckView: the cue set count the view starts with is not a new cue point")
{
    api::CommandQueue       queue;
    api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = deck, .cue_set_count = 5}};
    Translator              translator;
    DeckView                view{deck, queue, snapshot, translator};
    int                     reports = 0;
    view.setOnCueSet([&reports](double) { ++reports; });
    view.update();
    CHECK(reports == 0);
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

TEST_CASE("DeckView: the waveform of the track and its tempo are shown, the tempo follows the pitch")
{
    Fixture fixture;
    CHECK_FALSE(fixture.view.shownBpm().has_value());
    CHECK_FALSE(fixture.view.waveform().hasWaveform());

    fixture.view.setWaveform({10, 200, 30});
    fixture.view.setBpm(125.0);
    CHECK(fixture.view.waveform().hasWaveform());
    REQUIRE(fixture.view.shownBpm().has_value());
    CHECK(*fixture.view.shownBpm() == doctest::Approx(125.0));

    fixture.show({.deck = deck, .track_loaded = true, .pitch_percent = 8.0F});
    // The fader shows what was sent last: drag it to +8 %
    const Rect &fader = fixture.view.pitchFader().bounds();
    const float y     = fader.y + 12.0F + ((fader.height - 24.0F) * (1.0F - DeckView::faderValueOf(8.0F)));
    fixture.router.handle({{1, 1}, PointerPhase::Down, {fader.x + (fader.width / 2), y}});
    fixture.view.update();
    CHECK(*fixture.view.shownBpm() == doctest::Approx(125.0 * 1.08).epsilon(0.01));

    djw::tests::RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("135.0 BPM"));
}

TEST_CASE("DeckView: the whole layout grows with the window in both directions, text included")
{
    Fixture    fixture;
    const Rect play_small = fixture.view.playButton().bounds();
    const Rect cue_small  = fixture.view.cueButton().bounds();
    CHECK(fixture.view.scale() == doctest::Approx(1.0F));

    fixture.view.layout({0, 0, 760, 1028}); // twice as big
    const Rect play_big = fixture.view.playButton().bounds();
    CHECK(play_big.x == doctest::Approx(play_small.x * 2));
    CHECK(play_big.y == doctest::Approx(play_small.y * 2));
    CHECK(play_big.width == doctest::Approx(play_small.width * 2));
    CHECK(play_big.height == doctest::Approx(play_small.height * 2)); // the buttons grow downwards too
    CHECK(fixture.view.cueButton().bounds().height == doctest::Approx(cue_small.height * 2));
    CHECK(fixture.view.scale() == doctest::Approx(2.0F));

    // Taller only: heights grow, widths stay, the text follows the smaller direction
    fixture.view.layout({0, 0, 380, 1028});
    CHECK(fixture.view.playButton().bounds().width == doctest::Approx(play_small.width));
    CHECK(fixture.view.playButton().bounds().height == doctest::Approx(play_small.height * 2));
    CHECK(fixture.view.scale() == doctest::Approx(1.0F));
}

TEST_CASE("DeckView: a tap on the waveform of a deck that stands starts the scrub loop there and leaves it running")
{
    Fixture fixture;
    fixture.show({.deck = deck, .track_loaded = true, .position_seconds = 0.0, .duration_seconds = 100.0});
    const Rect &bar   = fixture.view.waveform().bounds();
    const Rect  inner = bar.inset(3.0F);
    const Point at{inner.x + (inner.width * 0.5F), bar.y + 5};
    fixture.tap(at);
    fixture.view.update();

    const auto                  commands = fixture.drain();
    std::vector<api::DeckScrub> scrubs;
    for (const auto &command : commands)
    {
        if (const auto *scrub = std::get_if<api::DeckScrub>(&command))
        {
            scrubs.push_back(*scrub);
        }
    }
    REQUIRE(scrubs.size() == 1);
    CHECK(scrubs.back().active);
    CHECK(scrubs.back().position_seconds == doctest::Approx(50.0).epsilon(0.02));
}

TEST_CASE("DeckView: on a playing deck the waveform jumps at once instead of scrubbing")
{
    Fixture fixture;
    fixture.show({.deck             = deck,
                  .track_loaded     = true,
                  .play_state       = api::PlayState::Playing,
                  .position_seconds = 10.0,
                  .duration_seconds = 100.0});
    const Rect &bar   = fixture.view.waveform().bounds();
    const Rect  inner = bar.inset(3.0F);
    fixture.router.handle({{1, 1}, PointerPhase::Down, {inner.x + (inner.width * 0.5F), bar.y + 5}});
    fixture.view.update();
    auto commands = fixture.drain();
    CHECK(find<api::DeckScrub>(commands) == nullptr);
    REQUIRE(find<api::DeckSeek>(commands) != nullptr);
    CHECK(find<api::DeckSeek>(commands)->position_seconds == doctest::Approx(50.0).epsilon(0.02));

    // Dragging jumps along, letting go sends nothing more
    fixture.router.handle({{1, 1}, PointerPhase::Move, {inner.x + (inner.width * 0.8F), bar.y + 5}});
    fixture.view.update();
    commands = fixture.drain();
    REQUIRE(find<api::DeckSeek>(commands) != nullptr);
    CHECK(find<api::DeckSeek>(commands)->position_seconds == doctest::Approx(80.0).epsilon(0.02));
    fixture.router.handle({{1, 1}, PointerPhase::Up, {inner.x + (inner.width * 0.8F), bar.y + 5}});
    fixture.view.update();
    commands = fixture.drain();
    CHECK(find<api::DeckSeek>(commands) == nullptr);
    CHECK(find<api::DeckScrub>(commands) == nullptr);
}

TEST_CASE("DeckView: while a track loads the waveform shows how far it is")
{
    Fixture fixture;
    fixture.show({.deck = deck, .track_loaded = true, .duration_seconds = 100.0, .loaded_fraction = 0.4F});
    CHECK(fixture.view.waveform().loaded() == doctest::Approx(0.4F));

    fixture.show({.deck = deck, .track_loaded = true, .duration_seconds = 100.0});
    CHECK(fixture.view.waveform().loaded() == doctest::Approx(1.0F));
}

TEST_CASE("DeckView: a notice replaces the state for a while, then the state shows again")
{
    api::CommandQueue       queue;
    api::DeckSnapshotBuffer snapshot{
        api::DeckSnapshot{.deck = deck, .track_loaded = true, .play_state = api::PlayState::Playing}};
    const Translator                      translator;
    std::chrono::steady_clock::time_point now{};
    DeckView                              view{deck, queue, snapshot, translator, [&now] { return now; }};
    view.update();
    CHECK(view.stateText() == "playing");

    view.setNotice("player.notice.playing");
    CHECK(view.stateText() == "not loaded");
    now += DeckView::notice_time - std::chrono::milliseconds{1};
    CHECK(view.stateText() == "not loaded");
    now += std::chrono::milliseconds{1};
    CHECK(view.stateText() == "playing");
}
