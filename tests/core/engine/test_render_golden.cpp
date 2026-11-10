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

    /// Renders block_count audio blocks; before a block listed in commands, its command is pushed into the queue.
    djw::tests::StereoAudio renderScenario(engine::Engine &engine, api::CommandQueue &queue, std::size_t block_count,
                                           const std::multimap<std::size_t, api::Command> &commands)
    {
        djw::tests::StereoAudio audio{.sample_rate = engine.sampleRate(),
                                      .samples     = std::vector<float>(block_count * block_frames * 2)};
        for (std::size_t block = 0; block < block_count; ++block)
        {
            const auto [first, last] = commands.equal_range(block);
            for (auto it = first; it != last; ++it)
            {
                REQUIRE(queue.tryPush(it->second));
            }
            engine.render(std::span{audio.samples}.subspan(block * block_frames * 2, block_frames * 2));
        }
        return audio;
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

    const auto result = djw::tests::compareWithGolden(audio, golden_dir / "engine" / "play_volume_pause.wav");
    INFO(result.message);
    CHECK(result.matches);
}
