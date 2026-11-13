// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/deck.hpp"

#include "signal_analysis.hpp"
#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

using djw::core::api::DeckId;
using djw::core::engine::Deck;
using djw::tests::BufferSource;
using djw::tests::SineSource;

TEST_CASE("Deck: a paused deck renders silence and does not move")
{
    BufferSource source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(4, 9.0F);

    deck.render(output);

    CHECK(output == std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 0);
}

TEST_CASE("Deck: a playing deck renders its source and counts frames")
{
    BufferSource source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(4);

    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{0.1F, 0.2F, 0.3F, 0.4F});
    CHECK(deck.positionFrames() == 2);
}

TEST_CASE("Deck: when the source runs dry the rest is silence and the position stops")
{
    BufferSource source{{0.1F, 0.2F}};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(6, 9.0F);

    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{0.1F, 0.2F, 0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 1);
}

TEST_CASE("Deck: pause keeps the position, play continues from there")
{
    BufferSource source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(2);

    deck.play();
    deck.render(output);
    deck.pause();
    deck.render(output);
    CHECK(deck.positionFrames() == 1);

    deck.play();
    deck.render(output);
    CHECK(output == std::vector<float>{0.3F, 0.4F});
    CHECK(deck.positionFrames() == 2);
}

TEST_CASE("Deck: pause fades out linearly, then stays silent")
{
    BufferSource source{std::vector<float>(20, 1.0F)};
    Deck         deck{DeckId{1}, 4};
    deck.attach(&source);
    std::vector<float> output(4);

    deck.play();
    deck.render(output);
    deck.pause();
    CHECK_FALSE(deck.isPlaying());

    std::vector<float> fade(12, 9.0F); // 6 frames, the fade takes 4 of them
    deck.render(fade);
    CHECK(fade == std::vector<float>{0.75F, 0.75F, 0.5F, 0.5F, 0.25F, 0.25F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 2 + 4);

    deck.render(output);
    CHECK(output == std::vector<float>(4, 0.0F));
    CHECK(deck.positionFrames() == 6);
}

TEST_CASE("Deck: the fade continues across render calls")
{
    BufferSource source{std::vector<float>(20, 1.0F)};
    Deck         deck{DeckId{1}, 4};
    deck.attach(&source);
    std::vector<float> output(4); // 2 frames

    deck.play();
    deck.pause();
    deck.render(output);
    CHECK(output == std::vector<float>{0.75F, 0.75F, 0.5F, 0.5F});
    deck.render(output);
    CHECK(output == std::vector<float>{0.25F, 0.25F, 0.0F, 0.0F});
    deck.render(output);
    CHECK(output == std::vector<float>(4, 0.0F));
}

TEST_CASE("Deck: play during the fade continues at full level")
{
    BufferSource source{std::vector<float>(20, 1.0F)};
    Deck         deck{DeckId{1}, 4};
    deck.attach(&source);
    std::vector<float> output(4);

    deck.play();
    deck.pause();
    deck.render(output);
    deck.play();
    CHECK(deck.isPlaying());
    deck.render(output);
    CHECK(output == std::vector<float>(4, 1.0F));
}

TEST_CASE("Deck: without a track it renders silence and ignores play")
{
    Deck               deck{DeckId{1}, 0};
    std::vector<float> output(4, 9.0F);

    deck.play();
    deck.render(output);

    CHECK_FALSE(deck.hasTrack());
    CHECK_FALSE(deck.isPlaying());
    CHECK(output == std::vector<float>(4, 0.0F));
}

TEST_CASE("Deck: a new track starts stopped at its beginning")
{
    BufferSource       first{djw::tests::ramp(10)};
    BufferSource       second{djw::tests::ramp(10)};
    Deck               deck{DeckId{1}, 0};
    std::vector<float> output(4);

    deck.attach(&first);
    deck.play();
    deck.render(output);
    deck.attach(&second);

    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.positionFrames() == 0);
    deck.attach(&second); // the same track again changes nothing
    CHECK(deck.positionFrames() == 0);
}

TEST_CASE("Deck: seek moves the position and playback continues there")
{
    BufferSource       source{djw::tests::ramp(100)};
    Deck               deck{DeckId{1}, 0};
    std::vector<float> output(4);
    deck.attach(&source);

    deck.seek(40);
    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{40.0F, 40.0F, 41.0F, 41.0F});
    CHECK(deck.positionFrames() == 42);
}

TEST_CASE("Deck: seek beyond the end is clamped to the end")
{
    BufferSource source{djw::tests::ramp(100)};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);

    deck.seek(500);

    CHECK(deck.positionFrames() == 100);
}

TEST_CASE("Deck: at the end of the track the deck stops")
{
    BufferSource       source{djw::tests::ramp(3)};
    Deck               deck{DeckId{1}, 0};
    std::vector<float> output(8);
    deck.attach(&source);

    deck.play();
    deck.render(output);

    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.positionFrames() == 3);
}

namespace
{
    /// A stopped deck with a ramp track and no pause fade
    struct CueFixture
    {
            BufferSource       source{djw::tests::ramp(100'000)};
            Deck               deck{DeckId{1}, 0};
            std::vector<float> output = std::vector<float>(100 * 2);

            CueFixture() { deck.attach(&source); }

            void play(std::size_t frames)
            {
                output.resize(frames * 2);
                deck.render(output);
            }
    };
} // namespace

TEST_CASE("Deck CUE: when stopped away from the cue point it sets the cue point there")
{
    CueFixture fixture;
    fixture.deck.seek(1'000);

    fixture.deck.cue(true);
    fixture.deck.cue(false);

    CHECK(fixture.deck.cueFrame() == 1'000);
    CHECK_FALSE(fixture.deck.isPlaying());
}

TEST_CASE("Deck CUE: while playing it jumps back to the cue point and stops")
{
    CueFixture fixture;
    fixture.deck.seek(1'000);
    fixture.deck.cue(true);
    fixture.deck.cue(false);
    fixture.deck.play();
    fixture.play(500);

    fixture.deck.cue(true);
    fixture.play(10);

    CHECK_FALSE(fixture.deck.isPlaying());
    CHECK(fixture.deck.positionFrames() == 1'000.0);
    CHECK(fixture.output == std::vector<float>(20, 0.0F));
}

TEST_CASE("Deck CUE: held at the cue point it plays, on release it returns to the cue point")
{
    CueFixture fixture;

    fixture.deck.cue(true);
    CHECK(fixture.deck.isPlaying());
    fixture.play(300);
    CHECK(fixture.output[0] == 0.0F);
    CHECK(fixture.output[598] == 299.0F);

    fixture.deck.cue(false);
    CHECK_FALSE(fixture.deck.isPlaying());
    CHECK(fixture.deck.positionFrames() == 0.0);
}

TEST_CASE("Deck CUE: play while CUE is held keeps playing after release")
{
    CueFixture fixture;

    fixture.deck.cue(true);
    fixture.play(100);
    fixture.deck.play();
    fixture.deck.cue(false);
    fixture.play(100);

    CHECK(fixture.deck.isPlaying());
    CHECK(fixture.output[0] == 100.0F);
}

TEST_CASE("Deck CUE: returning to the cue point fades out first")
{
    BufferSource       source{std::vector<float>(2'000, 1.0F)};
    Deck               deck{DeckId{1}, 4};
    std::vector<float> output(8);
    deck.attach(&source);
    deck.play();
    deck.render(output);

    deck.cue(true);
    deck.render(output);

    CHECK(output == std::vector<float>{0.75F, 0.75F, 0.5F, 0.5F, 0.25F, 0.25F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 0.0);
}

TEST_CASE("Deck CUE: a new track has its cue point at the start")
{
    CueFixture   fixture;
    BufferSource other{djw::tests::ramp(100)};
    fixture.deck.seek(500);
    fixture.deck.cue(true);
    REQUIRE(fixture.deck.cueFrame() == 500);

    fixture.deck.attach(&other);

    CHECK(fixture.deck.cueFrame() == 0);
}

TEST_CASE("Deck pitch: +12 % plays 12 % faster, the position counts track frames")
{
    CueFixture fixture;
    fixture.deck.setPitch(12.0F);
    fixture.deck.play();

    fixture.play(1'000);

    CHECK(fixture.deck.positionFrames() == doctest::Approx(1'120.0));
    CHECK(fixture.output[2] == doctest::Approx(1.12));
}

TEST_CASE("Deck pitch: the range is ±12 %")
{
    Deck deck{DeckId{1}, 0};
    deck.setPitch(30.0F);
    CHECK(deck.pitchPercent() == 12.0F);
    deck.setPitch(-30.0F);
    CHECK(deck.pitchPercent() == -12.0F);
}

TEST_CASE("Deck pitch bend: changes the speed while held and adds to the pitch")
{
    Deck deck{DeckId{1}, 0};
    deck.setPitch(10.0F);

    deck.setPitchBend(1);
    CHECK(deck.speed() == doctest::Approx(1.10 * 1.04));
    deck.setPitchBend(-1);
    CHECK(deck.speed() == doctest::Approx(1.10 * 0.96));
    deck.setPitchBend(0);
    CHECK(deck.speed() == doctest::Approx(1.10));
    CHECK(deck.pitchPercent() == 10.0F);
}

namespace
{
    /// Plays a 440 Hz sine for a second at the pitch and returns what comes out after the start-up
    std::vector<float> playSine(Deck &deck, SineSource &source, float pitch_percent, bool keylock)
    {
        deck.attach(&source);
        deck.setPitch(pitch_percent);
        deck.setKeylock(keylock);
        deck.play();
        std::vector<float> all;
        std::vector<float> block(480 * 2);
        for (int i = 0; i < 110; ++i)
        {
            deck.render(block);
            all.insert(all.end(), block.begin(), block.end());
        }
        return {all.begin() + 4'800 * 2, all.end()};
    }
} // namespace

TEST_CASE("Deck: the pitch fader changes tempo and pitch together without keylock")
{
    SineSource source{440.0, 48'000, 0.5F};
    Deck       deck{DeckId{1}, 0};
    const auto output = playSine(deck, source, 8.0F, false);
    CHECK(djw::tests::frequencyOf(output, 48'000) == doctest::Approx(440.0 * 1.08).epsilon(0.01));
}

TEST_CASE("Deck: with keylock the pitch fader changes the tempo only")
{
    SineSource source{440.0, 48'000, 0.5F};
    Deck       deck{DeckId{1}, 0};
    const auto output = playSine(deck, source, 8.0F, true);
    CHECK(deck.keylock());
    CHECK(djw::tests::frequencyOf(output, 48'000) == doctest::Approx(440.0).epsilon(0.02));
    // The track moved at 1.08: 1.1 s of output are 1.188 s of track, less what the stretcher holds back
    CHECK(deck.positionFrames() > 1.1 * 1.08 * 48'000 - 3'000);
    CHECK(deck.positionFrames() < 1.1 * 1.08 * 48'000 + 500);
}

TEST_CASE("Deck: turning keylock on or off keeps the position")
{
    SineSource source{440.0, 48'000, 0.5F};
    Deck       deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.play();
    std::vector<float> block(480 * 2);
    for (int i = 0; i < 20; ++i)
    {
        deck.render(block);
    }
    const double before = deck.positionFrames();

    deck.setKeylock(true);
    CHECK(deck.keylock());
    CHECK(deck.positionFrames() == doctest::Approx(before).epsilon(1.0 / before));
    deck.render(block);
    CHECK(deck.positionFrames() > before);

    const double during = deck.positionFrames();
    deck.setKeylock(false);
    CHECK_FALSE(deck.keylock());
    CHECK(deck.positionFrames() == doctest::Approx(during).epsilon(1.0 / during));
}

TEST_CASE("Deck: seek and CUE work with keylock")
{
    BufferSource source{djw::tests::ramp(48'000)};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.setKeylock(true);

    deck.seek(10'000);
    CHECK(deck.positionFrames() == 10'000);
    deck.cue(true); // stopped away from the cue point: sets the cue point here
    deck.cue(false);
    CHECK(deck.cueFrame() == 10'000);
    deck.play();
    std::vector<float> block(480 * 2);
    for (int i = 0; i < 10; ++i)
    {
        deck.render(block);
    }
    CHECK(deck.positionFrames() > 10'000);
    deck.cue(true);
    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.positionFrames() == 10'000);
}

TEST_CASE("Deck: a track plays out to its end with keylock and the deck stops")
{
    BufferSource source{djw::tests::ramp(12'000)};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.setPitch(10.0F);
    deck.setKeylock(true);
    deck.play();
    std::vector<float> block(480 * 2);
    for (int i = 0; i < 100 && deck.isPlaying(); ++i)
    {
        deck.render(block);
    }
    CHECK_FALSE(deck.isPlaying());
}

TEST_CASE("Deck: a track with a start frame is taken at that frame, which is its cue point")
{
    BufferSource source{djw::tests::ramp(1'000)};
    source.setStartFrame(400);
    Deck deck{DeckId{1}, 0};
    deck.attach(&source);

    CHECK(deck.positionFrames() == 400);
    CHECK(deck.cueFrame() == 400);
    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.cueSetCount() == 0); // it came with the track, the user did not set it

    deck.play();
    std::vector<float> output(4);
    deck.render(output);
    CHECK(output == std::vector<float>{400.0F, 400.0F, 401.0F, 401.0F}); // the source was moved there as well
    CHECK(deck.positionFrames() == 402);

    // CUE goes back to that point
    deck.cue(true);
    CHECK(deck.positionFrames() == 400);
}

TEST_CASE("Deck: a start frame at or beyond the end is not used, one in a track of unknown length is")
{
    BufferSource beyond{djw::tests::ramp(100)};
    beyond.setStartFrame(100);
    Deck deck{DeckId{1}, 0};
    deck.attach(&beyond);
    CHECK(deck.positionFrames() == 0);
    CHECK(deck.cueFrame() == 0);

    SineSource endless{440.0, 48'000, 0.5F}; // length 0 is unknown
    endless.setStartFrame(5'000);
    deck.attach(&endless);
    CHECK(deck.positionFrames() == 5'000);
    CHECK(deck.cueFrame() == 5'000);
}

TEST_CASE("Deck: the next track starts at its own start frame, not at the cue point of the one before")
{
    BufferSource first{djw::tests::ramp(1'000)};
    first.setStartFrame(700);
    BufferSource second{djw::tests::ramp(1'000)};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&first);
    deck.attach(&second);
    CHECK(deck.positionFrames() == 0);
    CHECK(deck.cueFrame() == 0);
}

TEST_CASE("Deck: the cue point set with CUE is counted, the one that comes with a track is not")
{
    BufferSource source{djw::tests::ramp(48'000)};
    source.setStartFrame(1'000);
    Deck deck{DeckId{1}, 0};
    deck.attach(&source);
    CHECK(deck.cueSetCount() == 0);

    deck.seek(20'000);
    deck.cue(true); // stopped away from the cue point: sets it here
    deck.cue(false);
    CHECK(deck.cueFrame() == 20'000);
    CHECK(deck.cueSetCount() == 1);

    deck.cue(true); // stopped at the cue point: preview, no new cue point
    deck.cue(false);
    CHECK(deck.cueSetCount() == 1);

    deck.seek(30'000);
    deck.cue(true);
    deck.cue(false);
    CHECK(deck.cueSetCount() == 2);
}

namespace
{
    constexpr std::size_t scrub_track_frames = 20'000;

    /// A track whose frame n has the value 0.1 + n * 0.0001 on both channels, so a position can be told from the audio
    std::vector<float> positionTrack()
    {
        std::vector<float> samples;
        for (std::size_t frame = 0; frame < scrub_track_frames; ++frame)
        {
            const float value = 0.1F + (static_cast<float>(frame) * 0.0001F);
            samples.push_back(value);
            samples.push_back(value);
        }
        return samples;
    }

    float valueAtFrame(const std::vector<float> &output, std::size_t frame)
    {
        return output[frame * 2];
    }

    /// The value of the track's frame, as a scrub started there plays it
    float trackValue(std::size_t frame)
    {
        return 0.1F + (static_cast<float>(frame) * 0.0001F);
    }
} // namespace

TEST_CASE("Deck: scrubbing a stopped deck plays a short loop at the position, over and over")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);

    deck.scrub(10'000, true);
    CHECK(deck.scrubbing());
    CHECK_FALSE(deck.isPlaying()); // it did not play before
    CHECK(deck.positionFrames() == 10'000);

    std::vector<float> output(2 * 6'000);
    deck.render(output);
    // After the fade-in the loop plays the track from the position on
    CHECK(valueAtFrame(output, 1'000) == doctest::Approx(trackValue(10'000 + 1'000)).epsilon(0.001));
    // One loop later the same piece again
    const std::size_t loop = static_cast<std::size_t>(Deck::scrub_loop_seconds * 48'000);
    CHECK(valueAtFrame(output, loop + 1'000) == doctest::Approx(trackValue(10'000 + 1'000)).epsilon(0.001));
    // The ends of the loop are faded, so the repeat does not click
    CHECK(valueAtFrame(output, 0) < 0.2F * trackValue(10'000));
    CHECK(valueAtFrame(output, loop - 1) < 0.2F * trackValue(10'000 + loop));
    CHECK(deck.positionFrames() == 10'000); // the position is where the finger is
}

TEST_CASE("Deck: the fades of the scrub loop rise and fall evenly, on both channels")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.scrub(10'000, true);
    std::vector<float> output(2 * 6'000);
    deck.render(output);

    const auto ramp = static_cast<std::size_t>(Deck::scrub_ramp_seconds * 48'000);
    const auto loop = static_cast<std::size_t>(Deck::scrub_loop_seconds * 48'000);
    for (const std::size_t channel : {0U, 1U})
    {
        CAPTURE(channel);
        // Half way through the fade-in half the level, at its end the full level
        CHECK(output[((ramp / 2) * 2) + channel] ==
              doctest::Approx(0.5F * trackValue(10'000 + (ramp / 2))).epsilon(0.02));
        CHECK(output[(ramp * 2) + channel] == doctest::Approx(trackValue(10'000 + ramp)).epsilon(0.001));
        // Half way through the fade-out half the level again
        const std::size_t falling = loop - (ramp / 2);
        CHECK(output[(falling * 2) + channel] == doctest::Approx(0.5F * trackValue(10'000 + falling)).epsilon(0.02));
    }
    // The fade-in starts at 0 exactly and rises by one step per frame, the same on both channels
    CHECK(valueAtFrame(output, 0) == 0.0F);
    CHECK(valueAtFrame(output, 1) == doctest::Approx(trackValue(10'001) / static_cast<float>(ramp)).epsilon(0.01));
    for (std::size_t frame = 0; frame <= ramp; ++frame)
    {
        CAPTURE(frame);
        REQUIRE(output[frame * 2] == output[(frame * 2) + 1]);
    }
}

TEST_CASE("Deck: moving the scrub moves the loop, ending it leaves the deck stopped at the position")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(2 * 1'500);

    deck.scrub(2'000, true);
    deck.render(output);
    deck.scrub(15'000, true);
    deck.render(output);
    CHECK(valueAtFrame(output, 1'000) == doctest::Approx(trackValue(15'000 + 1'000)).epsilon(0.001));
    CHECK(deck.positionFrames() == 15'000);

    deck.scrub(16'000, false);
    CHECK_FALSE(deck.scrubbing());
    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.positionFrames() == 16'000);
    deck.render(output);
    CHECK(output == std::vector<float>(output.size(), 0.0F)); // stopped: silence

    deck.play();
    deck.render(output);
    CHECK(valueAtFrame(output, 0) == doctest::Approx(trackValue(16'000)).epsilon(0.001));
}

TEST_CASE("Deck: the loop of a deck that stood runs on until CUE sets the cue point there")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(2 * 6'000);

    deck.scrub(10'000, true);
    deck.pause(); // already standing: the loop goes on
    deck.render(output);
    CHECK(deck.scrubbing());
    deck.seek(12'000); // a seek moves the loop
    deck.render(output);
    CHECK(valueAtFrame(output, 1'000) == doctest::Approx(trackValue(12'000 + 1'000)).epsilon(0.001));

    deck.cue(true);
    CHECK_FALSE(deck.scrubbing());
    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.cueFrame() == 12'000);
    CHECK(deck.cueSetCount() == 1);
    CHECK(deck.positionFrames() == 12'000);
    deck.cue(false);
    deck.render(output);
    CHECK(output == std::vector<float>(output.size(), 0.0F)); // stopped at the cue point
}

TEST_CASE("Deck: CUE fades the loop of a deck that stood out like a pause, then the deck stands at the cue point")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 480};
    deck.attach(&source);
    std::vector<float> output(2 * 1'000);
    deck.scrub(10'000, true);
    deck.render(output); // in the middle of the loop now, at full level

    deck.cue(true);
    CHECK_FALSE(deck.scrubbing());
    CHECK_FALSE(deck.isPlaying());
    CHECK(deck.cueFrame() == 10'000);
    CHECK(deck.cueSetCount() == 1);
    CHECK(deck.positionFrames() == 10'000);
    deck.cue(false);

    deck.render(output);
    // The loop goes on from where it was, falling linearly to silence over the pause fade
    CHECK(valueAtFrame(output, 0) == doctest::Approx(trackValue(10'000 + 1'000) * 479.0F / 480.0F).epsilon(0.001));
    CHECK(valueAtFrame(output, 240) == doctest::Approx(trackValue(10'000 + 1'240) * 239.0F / 480.0F).epsilon(0.001));
    CHECK(valueAtFrame(output, 480) == 0.0F);
    CHECK(valueAtFrame(output, 999) == 0.0F);
    CHECK(deck.positionFrames() == 10'000);

    deck.play();
    deck.render(output);
    CHECK(valueAtFrame(output, 0) == doctest::Approx(trackValue(10'000)).epsilon(0.001));
}

TEST_CASE("Deck: play or a seek during the fade after CUE take over at once")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 480};
    deck.attach(&source);
    std::vector<float> output(2 * 100);
    deck.scrub(10'000, true);
    deck.render(output);
    deck.cue(true);
    deck.render(output); // part of the fade

    deck.seek(15'000);
    CHECK(deck.positionFrames() == 15'000);
    deck.render(output);
    CHECK(output == std::vector<float>(output.size(), 0.0F)); // stands at the new position

    deck.scrub(10'000, true);
    deck.render(output);
    deck.cue(true);
    deck.render(output);
    deck.play();
    CHECK(deck.isPlaying());
    deck.render(output);
    CHECK(valueAtFrame(output, 0) == doctest::Approx(trackValue(10'000)).epsilon(0.001));
}

TEST_CASE("Deck: play ends the loop of a deck that stood and plays from there")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(2 * 100);

    deck.scrub(10'000, true);
    deck.render(output);
    deck.play();
    CHECK_FALSE(deck.scrubbing());
    CHECK(deck.isPlaying());
    CHECK(deck.cueSetCount() == 0); // the cue point stays
    deck.render(output);
    CHECK(valueAtFrame(output, 0) == doctest::Approx(trackValue(10'000)).epsilon(0.001));
    CHECK(deck.positionFrames() == doctest::Approx(10'100));
}

TEST_CASE("Deck: a deck that played goes on playing after the scrub, from the new position")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(2 * 100);
    deck.play();
    deck.render(output);

    deck.scrub(8'000, true);
    CHECK(deck.isPlaying()); // seen from outside it still plays
    deck.pause();            // transport commands wait until the scrub is over: ignored
    deck.cue(true);
    deck.play();
    CHECK(deck.scrubbing());
    CHECK(deck.isPlaying());

    deck.scrub(9'000, false);
    CHECK_FALSE(deck.scrubbing());
    CHECK(deck.isPlaying());
    deck.render(output);
    CHECK(valueAtFrame(output, 0) == doctest::Approx(trackValue(9'000)).epsilon(0.001));
    CHECK(deck.positionFrames() == doctest::Approx(9'100));
}

TEST_CASE("Deck: a scrub past the end is clamped, without a track or without a scrub going on it does no harm")
{
    Deck deck{DeckId{1}, 0};
    deck.scrub(100, true); // no track
    deck.scrub(100, false);
    CHECK_FALSE(deck.scrubbing());

    BufferSource source{positionTrack()};
    deck.attach(&source);
    deck.scrub(1'000'000, true);
    CHECK(deck.positionFrames() == scrub_track_frames);
    std::vector<float> output(2 * 4'000);
    deck.render(output); // nothing to read at the end: silence, no crash
    CHECK(output == std::vector<float>(output.size(), 0.0F));

    deck.scrub(500, false); // ending without a scrub: a seek
    deck.scrub(700, false);
    CHECK(deck.positionFrames() == 700);
}

namespace
{
    /// A fully decoded track that can act as not decoded yet at the position: then it reads nothing, without being at
    /// the end
    class GapSource final : public djw::core::engine::AudioSource
    {
        public:
            explicit GapSource(std::vector<float> samples) : track{std::move(samples)} {}

            std::size_t   read(std::span<float> output) noexcept override { return gap ? 0 : track.read(output); }
            void          seek(std::uint64_t frame) noexcept override { track.seek(frame); }
            std::uint64_t lengthFrames() const noexcept override { return track.lengthFrames(); }
            bool          atEnd() const noexcept override { return !gap && track.atEnd(); }

            bool gap = false;

        private:
            BufferSource track;
    };
} // namespace

TEST_CASE("Deck: a track that is not decoded yet at the position does not stop the deck, with keylock neither")
{
    bool keylock = false;
    SUBCASE("without keylock")
    {
        keylock = false;
    }
    SUBCASE("with keylock")
    {
        keylock = true;
    }
    GapSource source{std::vector<float>(2 * 48'000, 0.5F)};
    Deck      deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.setKeylock(keylock);
    deck.play();
    std::vector<float> output(2 * 480);
    for (int i = 0; i < 10; ++i)
    {
        deck.render(output);
    }

    source.gap = true;
    for (int i = 0; i < 10; ++i)
    {
        deck.render(output);
    }
    CHECK(deck.isPlaying());

    source.gap       = false;
    bool sound_again = false;
    for (int i = 0; i < 10 && !sound_again; ++i)
    {
        deck.render(output);
        sound_again = std::ranges::any_of(output, [](float sample) { return sample != 0.0F; });
    }
    CHECK(sound_again);
    CHECK(deck.isPlaying());
}

TEST_CASE("Deck: a new track ends CUE held at the old one, its release does not move the new track")
{
    CueFixture   fixture;
    BufferSource other{djw::tests::ramp(5'000)};
    fixture.deck.cue(true); // plays while held
    fixture.play(100);

    fixture.deck.attach(&other);
    fixture.deck.seek(1'000);
    fixture.deck.cue(false);

    CHECK(fixture.deck.positionFrames() == 1'000.0);
    CHECK_FALSE(fixture.deck.isPlaying());
}

TEST_CASE("Deck: a new track during the fade back to the cue point does not return to a cue point later")
{
    BufferSource first{std::vector<float>(2 * 2'000, 1.0F)};
    BufferSource second{std::vector<float>(2 * 2'000, 1.0F)};
    Deck         deck{DeckId{1}, 4};
    deck.attach(&first);
    deck.play();
    std::vector<float> output(2 * 10);
    deck.render(output);
    deck.cue(true); // fades out, then would return to the cue point

    deck.attach(&second);
    deck.cue(true); // at the cue point: plays while held
    deck.render(output);
    deck.pause();
    deck.render(output); // the fade of the pause, 4 frames

    CHECK(deck.positionFrames() == 14.0);
}

TEST_CASE("Deck: a scrub during the fade back to the cue point ends that return")
{
    BufferSource source{std::vector<float>(2 * 2'000, 1.0F)};
    Deck         deck{DeckId{1}, 4};
    deck.attach(&source);
    deck.play();
    std::vector<float> output(2 * 10);
    deck.render(output);
    deck.cue(true);

    deck.scrub(500, true);
    deck.scrub(500, false);
    CHECK_FALSE(deck.isPlaying());
    deck.cue(true); // sets the cue point at 500
    deck.cue(false);
    deck.cue(true); // plays while held
    deck.render(output);
    deck.pause();
    deck.render(output);

    CHECK(deck.positionFrames() == 514.0);
}

TEST_CASE("Deck: a new track with keylock starts at its beginning")
{
    BufferSource first{djw::tests::ramp(48'000)};
    BufferSource second{djw::tests::ramp(48'000)};
    Deck         deck{DeckId{1}, 0};
    deck.setKeylock(true); // without a track
    CHECK(deck.keylock());
    deck.setKeylock(false);
    CHECK_FALSE(deck.keylock());
    deck.setKeylock(true);
    deck.attach(&first);
    deck.play();
    std::vector<float> output(2 * 480);
    for (int i = 0; i < 10; ++i)
    {
        deck.render(output);
    }
    REQUIRE(deck.positionFrames() > 1'000.0);

    deck.attach(&second);
    CHECK(deck.positionFrames() == 0.0);
}

TEST_CASE("Deck CUE: pause while CUE is held stops there, the release does not return to the cue point")
{
    CueFixture fixture;
    fixture.deck.cue(true);
    fixture.play(300);

    fixture.deck.pause();
    fixture.deck.cue(false);

    CHECK_FALSE(fixture.deck.isPlaying());
    CHECK(fixture.deck.positionFrames() == 300.0);
}

TEST_CASE("Deck CUE: a release without CUE playing does not move the deck")
{
    CueFixture fixture;
    fixture.deck.cue(true);
    fixture.play(300);
    fixture.deck.cue(false); // back at the cue point
    fixture.deck.seek(1'000);

    fixture.deck.cue(false);

    CHECK(fixture.deck.positionFrames() == 1'000.0);
}

TEST_CASE("Deck CUE: held to the end of the track the deck stops there, the release leaves it there")
{
    BufferSource source{djw::tests::ramp(200)};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.cue(true);
    std::vector<float> output(2 * 300);
    deck.render(output);
    REQUIRE_FALSE(deck.isPlaying());
    const double end = deck.positionFrames();
    REQUIRE(end > 100.0);

    deck.cue(false);
    CHECK(deck.positionFrames() == end);
}

TEST_CASE("Deck CUE: without a pause fade, CUE right after pause sets the cue point there")
{
    CueFixture fixture;
    fixture.deck.play();
    fixture.play(100);
    fixture.deck.pause();

    fixture.deck.cue(true);

    CHECK(fixture.deck.cueSetCount() == 1);
    CHECK(fixture.deck.cueFrame() == 100);
}

TEST_CASE("Deck: without a pause fade, the cue point set by CUE in a scrub plays at once while CUE is held again")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.scrub(10'000, true);
    deck.cue(true);
    deck.cue(false);

    deck.cue(true);
    CHECK(deck.isPlaying());
}

TEST_CASE("Deck: the scrub loop starts over after its length, also when the blocks do not fit into it")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 0};
    deck.attach(&source);
    deck.scrub(10'000, true);
    const auto loop = static_cast<std::size_t>(Deck::scrub_loop_seconds * 48'000);

    std::vector<float> output(2 * 1'000);
    for (std::size_t start = 0; start < 2 * loop; start += 1'000)
    {
        deck.render(output);
        for (std::size_t frame = 0; frame < 1'000; ++frame)
        {
            const std::size_t at = (start + frame) % loop;
            CAPTURE(start + frame);
            if (at == 0)
            {
                REQUIRE(valueAtFrame(output, frame) == 0.0F); // the loop starts over, faded in from silence
            }
            else if (at == 1)
            {
                REQUIRE(valueAtFrame(output, frame) < 0.01F);
            }
        }
    }
}

TEST_CASE("Deck: a scrub where nothing is decoded yet starts its loop from silence once the audio is there")
{
    GapSource source{positionTrack()};
    Deck      deck{DeckId{1}, 0};
    deck.attach(&source);
    std::vector<float> output(2 * 1'000);
    source.gap = true;
    deck.scrub(10'000, true);
    deck.render(output);
    CHECK(output == std::vector<float>(output.size(), 0.0F));

    source.gap = false;
    deck.render(output);
    CHECK(valueAtFrame(output, 0) == 0.0F);
    CHECK(valueAtFrame(output, 500) == doctest::Approx(trackValue(10'500)).epsilon(0.001));
}

TEST_CASE("Deck: the fade after CUE in a scrub runs across render calls, then the deck stands and CUE plays")
{
    BufferSource source{positionTrack()};
    Deck         deck{DeckId{1}, 480};
    deck.attach(&source);
    std::vector<float> output(2 * 1'000);
    deck.scrub(10'000, true);
    deck.render(output); // in the middle of the loop
    deck.cue(true);
    deck.cue(false);

    std::vector<float> block(2 * 100);
    deck.render(block);
    CHECK(valueAtFrame(block, 0) == doctest::Approx(trackValue(11'000) * 479.0F / 480.0F).epsilon(0.001));
    deck.render(block);
    CHECK(valueAtFrame(block, 0) == doctest::Approx(trackValue(11'100) * 379.0F / 480.0F).epsilon(0.001));

    deck.render(output); // the rest of the fade
    deck.render(output);
    CHECK(output == std::vector<float>(output.size(), 0.0F));
    CHECK(deck.positionFrames() == 10'000);

    deck.cue(true); // at the cue point: plays while held
    CHECK(deck.isPlaying());
}
