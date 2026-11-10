// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/deck.hpp"

#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::api::DeckId;
using djw::core::engine::Deck;
using djw::tests::BufferSource;

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
