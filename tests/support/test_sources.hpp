// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_TEST_SOURCES_HPP
#define DJW_TESTS_SUPPORT_TEST_SOURCES_HPP

#include "engine/audio_source.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace djw::tests
{
    /// Endless sine on both channels with unknown length. The phase is computed from the frame position, so the
    /// output is reproducible.
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

            void          seek(std::uint64_t frame) noexcept override { next_frame = frame; }
            std::uint64_t lengthFrames() const noexcept override { return 0; }
            bool          atEnd() const noexcept override { return false; }

        private:
            double        frequency;
            std::uint32_t sample_rate;
            float         amplitude;
            std::uint64_t next_frame = 0;
    };

    /// Counts living instances, to see when the engine frees a track
    using LifeCounter = std::shared_ptr<std::atomic<int>>;

    /// A fixed buffer of interleaved stereo, played like a fully decoded track.
    class BufferSource final : public core::engine::AudioSource
    {
        public:
            explicit BufferSource(std::vector<float> samples, LifeCounter life = nullptr)
                : samples{std::move(samples)}, life{std::move(life)}
            {
                if (this->life)
                {
                    ++*this->life;
                }
            }

            BufferSource(const BufferSource &)            = delete;
            BufferSource &operator=(const BufferSource &) = delete;
            BufferSource(BufferSource &&)                 = delete;
            BufferSource &operator=(BufferSource &&)      = delete;

            ~BufferSource() override
            {
                if (life)
                {
                    --*life;
                }
            }

            std::size_t read(std::span<float> output) noexcept override
            {
                const std::size_t count = std::min(output.size(), samples.size() - position) /
                                          core::engine::stereo_channels * core::engine::stereo_channels;
                std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(position), count, output.begin());
                position += count;
                return count / core::engine::stereo_channels;
            }

            void seek(std::uint64_t frame) noexcept override
            {
                position = std::min(static_cast<std::size_t>(frame) * core::engine::stereo_channels, samples.size());
            }

            std::uint64_t lengthFrames() const noexcept override
            {
                return samples.size() / core::engine::stereo_channels;
            }

            bool atEnd() const noexcept override { return position >= samples.size(); }

        private:
            std::vector<float> samples;
            LifeCounter        life;
            std::size_t        position = 0;
    };

    /// frames frames where frame n has the value n * step on both channels. With a power of two as step the values
    /// stay exact; a step below 1 / frames keeps them below full scale, so the master limiter leaves them alone.
    inline std::vector<float> ramp(std::size_t frames, float step = 1.0F)
    {
        std::vector<float> samples(frames * core::engine::stereo_channels);
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            samples[i] = static_cast<float>(i / core::engine::stereo_channels) * step;
        }
        return samples;
    }
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_TEST_SOURCES_HPP
