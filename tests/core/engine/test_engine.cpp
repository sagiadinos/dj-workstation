// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/engine.hpp"

#include "allocation_counter.hpp"
#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using namespace djw::core;
using djw::tests::AllocationCounter;
using djw::tests::BufferSource;
using djw::tests::LifeCounter;
using djw::tests::ramp;
using djw::tests::SineSource;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;

    engine::EngineConfig smallBlocks()
    {
        return {.sample_rate = sample_rate, .max_block_frames = 4};
    }

    std::unique_ptr<BufferSource> constant(std::size_t frames, float value, LifeCounter life = nullptr)
    {
        return std::make_unique<BufferSource>(std::vector<float>(frames * 2, value), std::move(life));
    }

    /// A deck with a track, already playing
    api::DeckId playingDeck(engine::Engine &engine, api::CommandQueue &queue,
                            std::unique_ptr<engine::AudioSource> track)
    {
        const api::DeckId deck = engine.addDeck();
        REQUIRE(engine.setTrack(deck, std::move(track)));
        REQUIRE(queue.tryPush(api::DeckPlay{deck}));
        return deck;
    }
} // namespace

TEST_CASE("Engine: renders silence until a deck is started by command")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = engine.addDeck();
    REQUIRE(engine.setTrack(deck, constant(8, 0.5F)));
    std::vector<float> output(4);

    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.0F));

    REQUIRE(queue.tryPush(api::DeckPlay{deck}));
    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.5F));
}

TEST_CASE("Engine: the snapshot reflects track, play state, volume, position and duration")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = engine.addDeck();

    api::DeckSnapshotBuffer *snapshot = engine.deckSnapshot(deck);
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot->read().deck == deck);
    CHECK_FALSE(snapshot->read().track_loaded);

    REQUIRE(engine.setTrack(deck, constant(sample_rate, 0.5F)));
    REQUIRE(queue.tryPush(api::DeckPlay{deck}));
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck, 0.25F}));
    std::vector<float> output(sample_rate / 10 * 2); // 100 ms
    engine.render(output);

    const api::DeckSnapshot state = snapshot->read();
    CHECK(state.track_loaded);
    CHECK(state.play_state == api::PlayState::Playing);
    CHECK(state.channel_volume == doctest::Approx(0.25F));
    CHECK(state.position_seconds == doctest::Approx(0.1));
    CHECK(state.duration_seconds == doctest::Approx(1.0));
}

TEST_CASE("Engine: commands for unknown decks are ignored")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    REQUIRE(engine.setTrack(engine.addDeck(), constant(4, 0.5F)));
    std::vector<float> output(4);

    REQUIRE(queue.tryPush(api::DeckPlay{api::DeckId{99}}));
    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.0F));
    CHECK(engine.deckSnapshot(api::DeckId{99}) == nullptr);
    CHECK_FALSE(engine.setTrack(api::DeckId{99}, constant(4, 0.5F)));
    CHECK_FALSE(engine.removeDeck(api::DeckId{99}));
}

TEST_CASE("Engine: the decks are summed")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    playingDeck(engine, queue, constant(4, 0.25F));
    playingDeck(engine, queue, constant(4, 0.5F));
    std::vector<float> output(4);

    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.75F));
}

TEST_CASE("Engine: commands from several producers all arrive")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &ui         = engine.addCommandQueue();
    api::CommandQueue &controller = engine.addCommandQueue();
    const api::DeckId  deck       = engine.addDeck();
    REQUIRE(engine.setTrack(deck, constant(4, 1.0F)));

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
        playingDeck(engine, engine.addCommandQueue(), std::make_unique<SineSource>(1'000.0, sample_rate, 0.5F));
        engine.render(whole);
    }
    {
        engine::Engine engine{smallBlocks()};
        playingDeck(engine, engine.addCommandQueue(), std::make_unique<SineSource>(1'000.0, sample_rate, 0.5F));
        std::vector<float> block(8);
        for (int i = 0; i < 8; ++i)
        {
            engine.render(block);
            pieces.insert(pieces.end(), block.begin(), block.end());
        }
    }
    CHECK(whole == pieces);
}

TEST_CASE("Engine: a deck added while rendering takes part from the next render")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    std::vector<float> output(4);
    playingDeck(engine, queue, constant(100, 0.25F));
    engine.render(output);

    playingDeck(engine, queue, constant(100, 0.5F));
    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.75F));
}

TEST_CASE("Engine: a removed deck is silent at once and freed after the next render")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    auto               life  = std::make_shared<std::atomic<int>>(0);
    const api::DeckId  deck  = playingDeck(engine, queue, constant(100, 0.5F, life));
    std::vector<float> output(4);
    engine.render(output);

    REQUIRE(engine.removeDeck(deck));
    CHECK(engine.deckSnapshot(deck) == nullptr);
    engine.collectGarbage();
    CHECK(life->load() == 1); // the audio thread may still be inside the last render

    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.0F));
    engine.collectGarbage();
    CHECK(life->load() == 0);
}

TEST_CASE("Engine: loading a track replaces the old one, stops the deck and frees the old track later")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    auto               life  = std::make_shared<std::atomic<int>>(0);
    const api::DeckId  deck  = playingDeck(engine, queue, constant(100, 0.5F, life));
    std::vector<float> output(4);
    engine.render(output);

    REQUIRE(engine.setTrack(deck, constant(100, 0.25F)));
    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.0F));
    CHECK(engine.deckSnapshot(deck)->read().play_state == api::PlayState::Stopped);
    CHECK(engine.deckSnapshot(deck)->read().position_seconds == 0.0);
    engine.collectGarbage();
    CHECK(life->load() == 0);

    REQUIRE(queue.tryPush(api::DeckPlay{deck}));
    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.25F));
}

TEST_CASE("Engine: unloading a track leaves an empty deck")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = playingDeck(engine, queue, constant(100, 0.5F));
    std::vector<float> output(4);
    engine.render(output);

    REQUIRE(engine.setTrack(deck, nullptr));
    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.0F));
    CHECK_FALSE(engine.deckSnapshot(deck)->read().track_loaded);
}

TEST_CASE("Engine: DeckSeek jumps to the position in seconds")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = playingDeck(engine, queue, std::make_unique<BufferSource>(ramp(sample_rate)));

    REQUIRE(queue.tryPush(api::DeckSeek{deck, 0.5}));
    std::vector<float> output(4);
    engine.render(output);

    CHECK(output == std::vector<float>{24'000.0F, 24'000.0F, 24'001.0F, 24'001.0F});
    CHECK(engine.deckSnapshot(deck)->read().position_seconds == doctest::Approx(24'002.0 / sample_rate));
}

TEST_CASE("Engine: CUE, pitch and pitch bend arrive at the deck and show in the snapshot")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = engine.addDeck();
    REQUIRE(engine.setTrack(deck, std::make_unique<BufferSource>(ramp(sample_rate))));
    std::vector<float> output(4);

    REQUIRE(queue.tryPush(api::DeckSeek{deck, 0.5}));
    REQUIRE(queue.tryPush(api::DeckCue{deck, true}));
    REQUIRE(queue.tryPush(api::DeckCue{deck, false}));
    REQUIRE(queue.tryPush(api::DeckSetPitch{deck, -6.5F}));
    REQUIRE(queue.tryPush(api::DeckPitchBend{deck, 1}));
    engine.render(output);

    const api::DeckSnapshot state = engine.deckSnapshot(deck)->read();
    CHECK(state.cue_seconds == doctest::Approx(0.5));
    CHECK(state.pitch_percent == doctest::Approx(-6.5F));
    CHECK(state.play_state == api::PlayState::Stopped);
}

TEST_CASE("Engine: a deck stops at the end of its track")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = playingDeck(engine, queue, constant(6, 0.5F));
    std::vector<float> output(16);

    engine.render(output);

    const api::DeckSnapshot state = engine.deckSnapshot(deck)->read();
    CHECK(state.play_state == api::PlayState::Stopped);
    CHECK(state.position_seconds == doctest::Approx(state.duration_seconds));
}

TEST_CASE("Engine: decks and tracks can change while another thread renders")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 64}};
    api::CommandQueue &queue = engine.addCommandQueue();
    auto               life  = std::make_shared<std::atomic<int>>(0);
    std::atomic<bool>  running{true};

    std::thread audio(
        [&engine, &running]
        {
            std::vector<float> output(64 * 2);
            while (running.load())
            {
                engine.render(output);
            }
        });

    std::vector<api::DeckId> decks;
    for (int round = 0; round < 300; ++round)
    {
        const api::DeckId deck = engine.addDeck();
        decks.push_back(deck);
        REQUIRE(engine.setTrack(deck, constant(1'000, 0.1F, life)));
        static_cast<void>(queue.tryPush(api::DeckPlay{deck}));
        if (round % 3 == 0)
        {
            REQUIRE(engine.setTrack(decks.front(), constant(1'000, 0.2F, life)));
        }
        if (decks.size() > 4)
        {
            REQUIRE(engine.removeDeck(decks.front()));
            decks.erase(decks.begin());
        }
        engine.collectGarbage();
    }

    running.store(false);
    audio.join();
    // One more render, so everything retired during the run is certainly no longer seen
    std::vector<float> last(2);
    engine.render(last);
    engine.collectGarbage();
    CHECK(life->load() == static_cast<int>(decks.size()));
}

TEST_CASE("Engine: render does not allocate or free memory")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 256}};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  deck_a = playingDeck(engine, queue, std::make_unique<SineSource>(440.0, sample_rate, 0.5F));
    const api::DeckId  deck_b = playingDeck(engine, queue, std::make_unique<BufferSource>(ramp(sample_rate)));
    std::vector<float> output(1'000 * 2); // not a multiple of the block size
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck_a, 0.5F}));
    REQUIRE(queue.tryPush(api::DeckSeek{deck_b, 0.5}));
    REQUIRE(queue.tryPush(api::DeckSetPitch{deck_a, 7.5F}));
    REQUIRE(queue.tryPush(api::DeckPitchBend{deck_a, -1}));
    REQUIRE(queue.tryPush(api::DeckCue{deck_b, true}));
    REQUIRE(queue.tryPush(api::DeckCue{deck_b, false}));
    REQUIRE(queue.tryPush(api::DeckPause{deck_b}));

    const AllocationCounter counter;
    for (int i = 0; i < 100; ++i)
    {
        engine.render(output);
        static_cast<void>(engine.deckSnapshot(deck_a)->read());
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
