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
    BufferSource       source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck               deck{DeckId{1}, source, 0};
    std::vector<float> output(4, 9.0F);

    deck.render(output);

    CHECK(output == std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 0);
}

TEST_CASE("Deck: a playing deck renders its source and counts frames")
{
    BufferSource       source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck               deck{DeckId{1}, source, 0};
    std::vector<float> output(4);

    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{0.1F, 0.2F, 0.3F, 0.4F});
    CHECK(deck.positionFrames() == 2);
}

TEST_CASE("Deck: when the source runs dry the rest is silence and the position stops")
{
    BufferSource       source{{0.1F, 0.2F}};
    Deck               deck{DeckId{1}, source, 0};
    std::vector<float> output(6, 9.0F);

    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{0.1F, 0.2F, 0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 1);
}

TEST_CASE("Deck: pause keeps the position, play continues from there")
{
    BufferSource       source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck               deck{DeckId{1}, source, 0};
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
    BufferSource       source{std::vector<float>(20, 1.0F)};
    Deck               deck{DeckId{1}, source, 4};
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
    BufferSource       source{std::vector<float>(20, 1.0F)};
    Deck               deck{DeckId{1}, source, 4};
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
    BufferSource       source{std::vector<float>(20, 1.0F)};
    Deck               deck{DeckId{1}, source, 4};
    std::vector<float> output(4);

    deck.play();
    deck.pause();
    deck.render(output);
    deck.play();
    CHECK(deck.isPlaying());
    deck.render(output);
    CHECK(output == std::vector<float>(4, 1.0F));
}
