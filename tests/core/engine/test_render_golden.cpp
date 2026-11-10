// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/engine.hpp"

#include "golden.hpp"
#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <functional>
#include <map>
#include <span>

using namespace djw::core;

namespace
{
    constexpr std::uint32_t sample_rate  = 48'000;
    constexpr std::size_t   block_frames = 256;

    const std::filesystem::path golden_dir{DJW_TEST_GOLDEN_DIR};

    /// Interleaved output of a render, in the engine's output channels
    struct Rendered
    {
            std::uint32_t      sample_rate;
            std::size_t        channels;
            std::vector<float> samples;

            /// The channel pair starting at first_channel: 0 is the master, 2 the headphones
            [[nodiscard]] djw::tests::StereoAudio pair(std::size_t first_channel) const
            {
                djw::tests::StereoAudio audio{.sample_rate = sample_rate, .samples = {}};
                for (std::size_t index = first_channel; index < samples.size(); index += channels)
                {
                    audio.samples.push_back(samples[index]);
                    audio.samples.push_back(samples[index + 1]);
                }
                return audio;
            }
    };

    /// Renders block_count audio blocks; before a block listed in commands, its command is pushed into the queue.
    Rendered renderScenario(engine::Engine &engine, api::CommandQueue &queue, std::size_t block_count,
                            const std::multimap<std::size_t, api::Command> &commands)
    {
        const std::size_t block_samples = block_frames * engine.outputChannels();
        Rendered          rendered{.sample_rate = engine.sampleRate(),
                                   .channels    = engine.outputChannels(),
                                   .samples     = std::vector<float>(block_count * block_samples)};
        for (std::size_t block = 0; block < block_count; ++block)
        {
            const auto [first, last] = commands.equal_range(block);
            for (auto it = first; it != last; ++it)
            {
                REQUIRE(queue.tryPush(it->second));
            }
            engine.render(std::span{rendered.samples}.subspan(block * block_samples, block_samples));
        }
        return rendered;
    }

    void checkGolden(const djw::tests::StereoAudio &audio, const std::string &name)
    {
        const auto result = djw::tests::compareWithGolden(audio, golden_dir / "engine" / name);
        INFO(result.message);
        CHECK(result.matches);
    }
} // namespace

TEST_CASE("Render: play, lower the channel volume, pause")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = block_frames}};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = engine.addDeck();
    REQUIRE(engine.setTrack(deck, std::make_unique<djw::tests::SineSource>(440.0, sample_rate, 0.5F)));

    const auto audio = renderScenario(engine, queue, 48,
                                      {
                                          {4, api::DeckPlay{deck}},
                                          {20, api::MixerSetChannelVolume{deck, 0.25F}},
                                          {40, api::DeckPause{deck}},
                                      });

    checkGolden(audio.pair(0), "play_volume_pause.wav");
}

TEST_CASE("Render: pitch +8 %, pitch bend, CUE back to the cue point")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = block_frames}};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = engine.addDeck();
    REQUIRE(engine.setTrack(deck, std::make_unique<djw::tests::SineSource>(440.0, sample_rate, 0.5F)));

    // Blocks 0-11 at +8 %, 12-19 with bend up as well (+8 % and +4 %), 20 CUE: fade out, back to the start
    const auto audio = renderScenario(engine, queue, 24,
                                      {
                                          {0, api::DeckSetPitch{deck, 8.0F}},
                                          {0, api::DeckPlay{deck}},
                                          {12, api::DeckPitchBend{deck, 1}},
                                          {20, api::DeckPitchBend{deck, 0}},
                                          {20, api::DeckCue{deck, true}},
                                      });

    checkGolden(audio.pair(0), "pitch_bend_cue.wav");
    CHECK(engine.deckSnapshot(deck)->read().position_seconds == 0.0);
}

TEST_CASE("Render: crossfader A to B over the center with the limiter, momentary A, master volume")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = block_frames}};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  deck_a = engine.addDeck();
    const api::DeckId  deck_b = engine.addDeck();
    REQUIRE(engine.setTrack(deck_a, std::make_unique<djw::tests::SineSource>(440.0, sample_rate, 0.9F)));
    REQUIRE(engine.setTrack(deck_b, std::make_unique<djw::tests::SineSource>(660.0, sample_rate, 0.9F)));

    // 0-7 A only, 8-15 center (the sum peaks at 1.27: limited), 16-23 B only, 24-31 momentary A, 32-39 back at B,
    // 40-47 B at half master volume
    const auto audio = renderScenario(engine, queue, 48,
                                      {
                                          {0, api::MixerAssignCrossfader{deck_a, api::CrossfaderSide::A}},
                                          {0, api::MixerAssignCrossfader{deck_b, api::CrossfaderSide::B}},
                                          {0, api::MixerSetCrossfader{-1.0F}},
                                          {0, api::DeckPlay{deck_a}},
                                          {0, api::DeckPlay{deck_b}},
                                          {8, api::MixerSetCrossfader{0.0F}},
                                          {16, api::MixerSetCrossfader{1.0F}},
                                          {24, api::MixerCrossfaderMomentary{api::CrossfaderSide::A}},
                                          {32, api::MixerCrossfaderMomentary{api::CrossfaderSide::None}},
                                          {40, api::MixerSetMasterVolume{0.5F}},
                                      });

    checkGolden(audio.pair(0), "crossfader_limiter.wav");
}

TEST_CASE("Render: 4 channels, pre-listening a deck on the headphones while the other one plays on the master")
{
    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = block_frames, .output_channels = 4}};
    api::CommandQueue &queue  = engine.addCommandQueue();
    const api::DeckId  on_air = engine.addDeck();
    const api::DeckId  next   = engine.addDeck();
    REQUIRE(engine.setTrack(on_air, std::make_unique<djw::tests::SineSource>(440.0, sample_rate, 0.5F)));
    REQUIRE(engine.setTrack(next, std::make_unique<djw::tests::SineSource>(660.0, sample_rate, 0.5F)));

    // 0-11 the next deck only on the headphones, 12-23 cue mix halfway to the master, 24-35 the next deck's fader
    // up: both on the master, headphones at half volume, 36-47 cue off: master only on the headphones
    const auto audio = renderScenario(engine, queue, 48,
                                      {
                                          {0, api::MixerSetChannelVolume{next, 0.0F}},
                                          {0, api::MixerSetHeadphoneCue{next, true}},
                                          {0, api::DeckPlay{on_air}},
                                          {0, api::DeckPlay{next}},
                                          {12, api::MixerSetCueMix{0.5F}},
                                          {24, api::MixerSetChannelVolume{next, 1.0F}},
                                          {24, api::MixerSetHeadphoneVolume{0.5F}},
                                          {36, api::MixerSetHeadphoneCue{next, false}},
                                      });

    checkGolden(audio.pair(0), "headphones_master.wav");
    checkGolden(audio.pair(2), "headphones_cue.wav");
}
