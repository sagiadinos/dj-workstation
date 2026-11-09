// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/mixer_channel.hpp"

#include "engine/audio_source.hpp"

#include <algorithm>
#include <cstddef>

namespace djw::core::engine
{
    void MixerChannel::setVolume(float value) noexcept // [audio-thread]
    {
        target_volume = std::clamp(value, 0.0F, 1.0F);
    }

    void MixerChannel::mixInto(std::span<const float> input, std::span<float> output) noexcept // [audio-thread]
    {
        const std::size_t frames = std::min(input.size(), output.size()) / stereo_channels;
        if (frames == 0)
        {
            return;
        }

        const float step = (target_volume - current_volume) / static_cast<float>(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float gain = current_volume + step * static_cast<float>(frame + 1);
            for (std::size_t channel = 0; channel < stereo_channels; ++channel)
            {
                const std::size_t index = frame * stereo_channels + channel;
                output[index] += input[index] * gain;
            }
        }
        current_volume = target_volume;
    }
} // namespace djw::core::engine
