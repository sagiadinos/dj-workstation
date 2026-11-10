// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/peak_meter.hpp"

#include "engine/audio_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace djw::core::engine
{
    PeakMeter::PeakMeter(std::uint32_t sample_rate) noexcept
        : fall_per_frame{std::pow(10.0, -fall_decibels / 20.0 / (fall_seconds * sample_rate))}
    {
    }

    void PeakMeter::process(std::span<const float> samples) noexcept // [audio-thread]
    {
        const std::size_t frames = samples.size() / stereo_channels;
        float             left   = 0.0F;
        float             right  = 0.0F;
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            left  = std::max(left, std::abs(samples[frame * stereo_channels]));
            right = std::max(right, std::abs(samples[(frame * stereo_channels) + 1]));
        }

        const auto fall = static_cast<float>(std::pow(fall_per_frame, static_cast<double>(frames)));
        current.left    = std::max(left, current.left * fall);
        current.right   = std::max(right, current.right * fall);
    }
} // namespace djw::core::engine
