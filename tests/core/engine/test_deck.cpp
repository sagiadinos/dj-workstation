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
    Deck               deck{DeckId{1}, source};
    std::vector<float> output(4, 9.0F);

    deck.render(output);

    CHECK(output == std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 0);
}

TEST_CASE("Deck: a playing deck renders its source and counts frames")
{
    BufferSource       source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck               deck{DeckId{1}, source};
    std::vector<float> output(4);

    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{0.1F, 0.2F, 0.3F, 0.4F});
    CHECK(deck.positionFrames() == 2);
}

TEST_CASE("Deck: when the source runs dry the rest is silence and the position stops")
{
    BufferSource       source{{0.1F, 0.2F}};
    Deck               deck{DeckId{1}, source};
    std::vector<float> output(6, 9.0F);

    deck.play();
    deck.render(output);

    CHECK(output == std::vector<float>{0.1F, 0.2F, 0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(deck.positionFrames() == 1);
}

TEST_CASE("Deck: pause keeps the position, play continues from there")
{
    BufferSource       source{{0.1F, 0.2F, 0.3F, 0.4F}};
    Deck               deck{DeckId{1}, source};
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
