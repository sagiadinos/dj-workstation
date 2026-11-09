// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_TEST_SOURCES_HPP
#define DJW_TESTS_SUPPORT_TEST_SOURCES_HPP

#include "engine/audio_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace djw::tests
{
    /// Endless sine on both channels. The phase is computed from the frame count, so the output is reproducible.
    class SineSource final : public core::engine::AudioSource
    {
        public:
            SineSource(double frequency, std::uint32_t sample_rate, float amplitude) noexcept
                : frequency{frequency}, sample_rate{sample_rate}, amplitude{amplitude}
            {
            }

            std::size_t read(std::span<float> output) noexcept override
            {
                const std::size_t frames = output.size() / core::engine::stereo_channels;
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    const double phase    = 2.0 * std::numbers::pi * frequency * static_cast<double>(next_frame++) /
                                            static_cast<double>(sample_rate);
                    const auto   value    = static_cast<float>(amplitude * std::sin(phase));
                    output[frame * 2]     = value;
                    output[frame * 2 + 1] = value;
                }
                return frames;
            }

        private:
            double        frequency;
            std::uint32_t sample_rate;
            float         amplitude;
            std::uint64_t next_frame = 0;
    };

    /// Plays a fixed buffer of interleaved stereo once, then runs dry.
    class BufferSource final : public core::engine::AudioSource
    {
        public:
            explicit BufferSource(std::vector<float> samples) : samples{std::move(samples)} {}

            std::size_t read(std::span<float> output) noexcept override
            {
                const std::size_t count = std::min(output.size(), samples.size() - position) /
                                          core::engine::stereo_channels * core::engine::stereo_channels;
                std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(position), count, output.begin());
                position += count;
                return count / core::engine::stereo_channels;
            }

        private:
            std::vector<float> samples;
            std::size_t        position = 0;
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_TEST_SOURCES_HPP
