// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/gain_ramp.hpp"

#include "engine/audio_source.hpp"

#include <algorithm>
#include <cstddef>

namespace djw::core::engine
{
    template <typename Action> void GainRamp::run(std::size_t frames, Action action) noexcept // [audio-thread]
    {
        if (frames == 0)
        {
            return;
        }
        const float step = (target_gain - current) / static_cast<float>(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float gain = current + step * static_cast<float>(frame + 1);
            for (std::size_t channel = 0; channel < stereo_channels; ++channel)
            {
                action((frame * stereo_channels) + channel, gain);
            }
        }
        current = target_gain;
    }

    void GainRamp::addTo(std::span<const float> input, std::span<float> output) noexcept // [audio-thread]
    {
        run(std::min(input.size(), output.size()) / stereo_channels,
            [input, output](std::size_t index, float gain) { output[index] += input[index] * gain; });
    }

    void GainRamp::applyTo(std::span<float> samples) noexcept // [audio-thread]
    {
        run(samples.size() / stereo_channels, [samples](std::size_t index, float gain) { samples[index] *= gain; });
    }
} // namespace djw::core::engine
