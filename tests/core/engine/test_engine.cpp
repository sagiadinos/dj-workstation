// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/engine.hpp"

#include "allocation_counter.hpp"
#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <vector>

using namespace djw::core;
using djw::tests::AllocationCounter;
using djw::tests::BufferSource;
using djw::tests::SineSource;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;

    engine::EngineConfig smallBlocks()
    {
        return {.sample_rate = sample_rate, .max_block_frames = 4};
    }
} // namespace

TEST_CASE("Engine: renders silence until a deck is started by command")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    BufferSource       source{std::vector<float>(8, 0.5F)};
    const api::DeckId  deck = engine.addDeck(source);
    std::vector<float> output(4);

    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.0F));

    REQUIRE(queue.tryPush(api::DeckPlay{deck}));
    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.5F));
}

TEST_CASE("Engine: the snapshot reflects play state, volume and position")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    SineSource         source{440.0, sample_rate, 0.5F};
    const api::DeckId  deck = engine.addDeck(source);

    api::DeckSnapshotBuffer *snapshot = engine.deckSnapshot(deck);
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot->read().deck == deck);
    CHECK(snapshot->read().play_state == api::PlayState::Stopped);

    REQUIRE(queue.tryPush(api::DeckPlay{deck}));
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck, 0.25F}));
    std::vector<float> output(sample_rate / 10 * 2); // 100 ms
    engine.render(output);

    const api::DeckSnapshot state = snapshot->read();
    CHECK(state.play_state == api::PlayState::Playing);
    CHECK(state.channel_volume == doctest::Approx(0.25F));
    CHECK(state.position_seconds == doctest::Approx(0.1));
}

TEST_CASE("Engine: commands for unknown decks are ignored")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    BufferSource       source{std::vector<float>(4, 0.5F)};
    engine.addDeck(source);
    std::vector<float> output(4);

    REQUIRE(queue.tryPush(api::DeckPlay{api::DeckId{99}}));
    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.0F));
    CHECK(engine.deckSnapshot(api::DeckId{99}) == nullptr);
}

TEST_CASE("Engine: the decks are summed")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    BufferSource       first{std::vector<float>(4, 0.25F)};
    BufferSource       second{std::vector<float>(4, 0.5F)};
    REQUIRE(queue.tryPush(api::DeckPlay{engine.addDeck(first)}));
    REQUIRE(queue.tryPush(api::DeckPlay{engine.addDeck(second)}));
    std::vector<float> output(4);

    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.75F));
}

TEST_CASE("Engine: commands from several producers all arrive")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &ui         = engine.addCommandQueue();
    api::CommandQueue &controller = engine.addCommandQueue();
    BufferSource       source{std::vector<float>(4, 1.0F)};
    const api::DeckId  deck = engine.addDeck(source);

    REQUIRE(ui.tryPush(api::DeckPlay{deck}));
    REQUIRE(controller.tryPush(api::MixerSetChannelVolume{deck, 0.5F}));
    std::vector<float> output(4);
    engine.render(output);

    CHECK(engine.deckSnapshot(deck)->read().play_state == api::PlayState::Playing);
    CHECK(engine.deckSnapshot(deck)->read().channel_volume == doctest::Approx(0.5F));
}

TEST_CASE("Engine: a request larger than the block size gives the same result as small requests")
{
    std::vector<float> whole(64);
    std::vector<float> pieces;
    {
        engine::Engine engine{smallBlocks()};
        SineSource     source{1'000.0, sample_rate, 0.5F};
        REQUIRE(engine.addCommandQueue().tryPush(api::DeckPlay{engine.addDeck(source)}));
        engine.render(whole);
    }
    {
        engine::Engine engine{smallBlocks()};
        SineSource     source{1'000.0, sample_rate, 0.5F};
        REQUIRE(engine.addCommandQueue().tryPush(api::DeckPlay{engine.addDeck(source)}));
        std::vector<float> block(8);
        for (int i = 0; i < 8; ++i)
        {
            engine.render(block);
            pieces.insert(pieces.end(), block.begin(), block.end());
        }
    }
    CHECK(whole == pieces);
}

TEST_CASE("Engine: render does not allocate or free memory")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 256}};
    api::CommandQueue &queue = engine.addCommandQueue();
    SineSource         first{440.0, sample_rate, 0.5F};
    SineSource         second{660.0, sample_rate, 0.5F};
    const api::DeckId  deck_a = engine.addDeck(first);
    const api::DeckId  deck_b = engine.addDeck(second);
    std::vector<float> output(1'000 * 2); // not a multiple of the block size
    REQUIRE(queue.tryPush(api::DeckPlay{deck_a}));
    REQUIRE(queue.tryPush(api::DeckPlay{deck_b}));
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck_a, 0.5F}));
    REQUIRE(queue.tryPush(api::DeckPause{deck_b}));

    const AllocationCounter counter;
    for (int i = 0; i < 100; ++i)
    {
        engine.render(output);
        engine.deckSnapshot(deck_a)->read();
    }

    CHECK(counter.allocations() == 0);
    CHECK(counter.deallocations() == 0);
}

TEST_CASE("AllocationCounter: notices allocations, so the test above can fail")
{
    const AllocationCounter counter;
    auto                    memory = std::make_unique<int>(1);
    memory.reset();
    CHECK(counter.allocations() == 1);
    CHECK(counter.deallocations() == 1);
}
