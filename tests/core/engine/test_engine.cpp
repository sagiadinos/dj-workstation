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
    constexpr float    step  = 1.0F / 65'536; // below full scale and exact
    const api::DeckId  deck  = playingDeck(engine, queue, std::make_unique<BufferSource>(ramp(sample_rate, step)));

    REQUIRE(queue.tryPush(api::DeckSeek{deck, 0.5}));
    std::vector<float> output(4);
    engine.render(output);

    CHECK(output == std::vector<float>{24'000 * step, 24'000 * step, 24'001 * step, 24'001 * step});
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

TEST_CASE("Engine: the crossfader fades between the decks on side A and B, decks on no side stay")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  deck_a = playingDeck(engine, queue, constant(100, 0.25F));
    const api::DeckId  deck_b = playingDeck(engine, queue, constant(100, 0.125F));
    playingDeck(engine, queue, constant(100, 0.5F)); // on no side
    REQUIRE(queue.tryPush(api::MixerAssignCrossfader{deck_a, api::CrossfaderSide::A}));
    REQUIRE(queue.tryPush(api::MixerAssignCrossfader{deck_b, api::CrossfaderSide::B}));
    REQUIRE(queue.tryPush(api::MixerSetCrossfader{-1.0F}));
    std::vector<float> output(4);
    engine.render(output); // ramps to the new gains

    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.75F));

    REQUIRE(queue.tryPush(api::MixerSetCrossfader{1.0F}));
    engine.render(output);
    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.625F));

    CHECK(engine.deckSnapshot(deck_a)->read().crossfader_side == api::CrossfaderSide::A);
    CHECK(engine.deckSnapshot(deck_b)->read().crossfader_side == api::CrossfaderSide::B);
    CHECK(engine.mixerSnapshot().read().crossfader_position == 1.0F);
}

TEST_CASE("Engine: momentary A/B holds a side and returns to the crossfader position on release")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  deck_a = playingDeck(engine, queue, constant(100, 0.25F));
    const api::DeckId  deck_b = playingDeck(engine, queue, constant(100, 0.125F));
    REQUIRE(queue.tryPush(api::MixerAssignCrossfader{deck_a, api::CrossfaderSide::A}));
    REQUIRE(queue.tryPush(api::MixerAssignCrossfader{deck_b, api::CrossfaderSide::B}));
    REQUIRE(queue.tryPush(api::MixerSetCrossfader{-1.0F}));
    std::vector<float> output(4);
    engine.render(output);

    REQUIRE(queue.tryPush(api::MixerCrossfaderMomentary{api::CrossfaderSide::B}));
    engine.render(output);
    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.125F));
    CHECK(engine.mixerSnapshot().read().crossfader_momentary == api::CrossfaderSide::B);
    CHECK(engine.mixerSnapshot().read().crossfader_position == -1.0F);

    REQUIRE(queue.tryPush(api::MixerCrossfaderMomentary{api::CrossfaderSide::None}));
    engine.render(output);
    engine.render(output);
    CHECK(output == std::vector<float>(4, 0.25F));
}

TEST_CASE("Engine: master volume and limiter work on the sum and show in the mixer snapshot")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    playingDeck(engine, queue, constant(100, 0.75F));
    playingDeck(engine, queue, constant(100, 0.75F));
    std::vector<float> output(4);

    engine.render(output); // 1.5: the limiter takes it down to full scale
    CHECK(output == std::vector<float>(4, 1.0F));
    CHECK(engine.mixerSnapshot().read().limiter_gain == doctest::Approx(1.0F / 1.5F));

    REQUIRE(queue.tryPush(api::MixerSetMasterVolume{0.5F}));
    engine.render(output);
    engine.render(output);
    engine.render(output); // 0.75 now; the limiter gain needs a while to come back up
    CHECK(output[0] < 0.75F);
    const api::MixerSnapshot mixer = engine.mixerSnapshot().read();
    CHECK(mixer.master_volume == 0.5F);
    CHECK(mixer.master_level.left == doctest::Approx(1.0F).epsilon(1e-3)); // the first block's peak, falling
}

TEST_CASE("Engine: VU levels per deck before the fader and for the master")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = playingDeck(engine, queue, constant(100, 0.5F));
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck, 0.0F}));
    std::vector<float> output(4);
    engine.render(output);
    engine.render(output);

    CHECK(engine.deckSnapshot(deck)->read().level.left == 0.5F);
    CHECK(engine.deckSnapshot(deck)->read().level.right == 0.5F);
    CHECK(engine.mixerSnapshot().read().master_level.left < 0.5F); // the ramp down from the first block, falling
}

TEST_CASE("Engine: with stereo output there are no headphones, cued decks are still on the master")
{
    engine::Engine     engine{smallBlocks()};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = playingDeck(engine, queue, constant(100, 0.5F));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneCue{deck, true}));
    std::vector<float> output(4);

    engine.render(output);

    CHECK(output == std::vector<float>(4, 0.5F));
    CHECK_FALSE(engine.headphonesAvailable());
    CHECK_FALSE(engine.mixerSnapshot().read().headphones_available);
    CHECK(engine.deckSnapshot(deck)->read().headphone_cue);
}

TEST_CASE("Engine: with 4 channels the master is on 1/2 and the cued decks are on the headphones on 3/4")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 4, .output_channels = 4}};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  master = playingDeck(engine, queue, constant(100, 0.25F));
    const api::DeckId  cued   = playingDeck(engine, queue, constant(100, 0.5F));
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{cued, 0.0F}));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneCue{cued, true}));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneLevel{cued, 0.5F}));
    std::vector<float> output(4 * 4);
    CHECK(engine.mixerSnapshot().read().headphones_available); // known before the first render
    engine.render(output);                                     // faders ramp

    engine.render(output);

    CHECK(output == std::vector<float>{0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, //
                                       0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F});
    CHECK(engine.headphonesAvailable());
    CHECK(engine.mixerSnapshot().read().headphones_available);
    CHECK(engine.deckSnapshot(cued)->read().headphone_level == 0.5F);
    CHECK_FALSE(engine.deckSnapshot(master)->read().headphone_cue);
}

TEST_CASE("Engine: cue mix and headphone volume shape the headphones and show in the snapshot")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 4, .output_channels = 4}};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = playingDeck(engine, queue, constant(100, 0.5F));
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck, 0.5F})); // master 0.25, cue 0.5
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneCue{deck, true}));
    REQUIRE(queue.tryPush(api::MixerSetCueMix{0.5F}));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneVolume{0.5F}));
    std::vector<float> output(2 * 4);
    engine.render(output);

    engine.render(output);

    // (0.5 * 0.5 + 0.25 * 0.5) * 0.5
    CHECK(output == std::vector<float>{0.25F, 0.25F, 0.1875F, 0.1875F, 0.25F, 0.25F, 0.1875F, 0.1875F});
    const api::MixerSnapshot mixer = engine.mixerSnapshot().read();
    CHECK(mixer.cue_mix == 0.5F);
    CHECK(mixer.headphone_volume == 0.5F);
}

TEST_CASE("Engine: output channels are at least stereo")
{
    const engine::Engine engine{{.sample_rate = sample_rate, .output_channels = 1}};
    CHECK(engine.outputChannels() == 2);
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
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 256, .output_channels = 4}};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  deck_a = playingDeck(engine, queue, std::make_unique<SineSource>(440.0, sample_rate, 0.5F));
    const api::DeckId  deck_b = playingDeck(engine, queue, std::make_unique<BufferSource>(ramp(sample_rate)));
    std::vector<float> output(1'000 * 4); // not a multiple of the block size
    REQUIRE(queue.tryPush(api::MixerSetChannelVolume{deck_a, 0.5F}));
    REQUIRE(queue.tryPush(api::DeckSeek{deck_b, 0.5}));
    REQUIRE(queue.tryPush(api::DeckSetPitch{deck_a, 7.5F}));
    REQUIRE(queue.tryPush(api::DeckPitchBend{deck_a, -1}));
    REQUIRE(queue.tryPush(api::DeckCue{deck_b, true}));
    REQUIRE(queue.tryPush(api::DeckCue{deck_b, false}));
    REQUIRE(queue.tryPush(api::DeckPause{deck_b}));
    REQUIRE(queue.tryPush(api::MixerAssignCrossfader{deck_a, api::CrossfaderSide::A}));
    REQUIRE(queue.tryPush(api::MixerSetCrossfader{0.3F}));
    REQUIRE(queue.tryPush(api::MixerCrossfaderMomentary{api::CrossfaderSide::B}));
    REQUIRE(queue.tryPush(api::MixerSetMasterVolume{0.8F}));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneCue{deck_a, true}));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneLevel{deck_a, 0.7F}));
    REQUIRE(queue.tryPush(api::MixerSetCueMix{0.4F}));
    REQUIRE(queue.tryPush(api::MixerSetHeadphoneVolume{0.9F}));

    const AllocationCounter counter;
    for (int i = 0; i < 100; ++i)
    {
        engine.render(output);
        static_cast<void>(engine.deckSnapshot(deck_a)->read());
        static_cast<void>(engine.mixerSnapshot().read());
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
