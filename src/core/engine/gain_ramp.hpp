// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_GAIN_RAMP_HPP
#define DJW_CORE_ENGINE_GAIN_RAMP_HPP

#include <cstddef>
#include <span>

namespace djw::core::engine
{
    /**
     * A gain that changes without clicks: a new target is reached linearly over the next block, on its last frame.
     *
     * Used by everything that has a fader: mixer channels, crossfader, master volume.
     */
    class GainRamp
    {
        public:
            explicit GainRamp(float initial = 1.0F) noexcept : current{initial}, target_gain{initial} {}

            void                setTarget(float gain) noexcept { target_gain = gain; } // [audio-thread]
            [[nodiscard]] float target() const noexcept { return target_gain; }

            /// output += input * gain, both interleaved stereo; the shorter one sets the block size.
            void addTo(std::span<const float> input, std::span<float> output) noexcept; // [audio-thread]
            /// samples *= gain, interleaved stereo
            void applyTo(std::span<float> samples) noexcept; // [audio-thread]

        private:
            template <typename Action> void run(std::size_t frames, Action action) noexcept; // [audio-thread]

            float current;
            float target_gain;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_GAIN_RAMP_HPP
