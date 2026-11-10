// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/limiter.hpp"

#include "engine/audio_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace djw::core::engine
{
    Limiter::Limiter(std::uint32_t sample_rate) noexcept
        : release_coefficient{static_cast<float>(std::exp(-1.0 / (release_seconds * sample_rate)))}
    {
    }

    void Limiter::process(std::span<float> samples) noexcept // [audio-thread]
    {
        const std::size_t frames = samples.size() / stereo_channels;
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            float &left  = samples[frame * stereo_channels];
            float &right = samples[(frame * stereo_channels) + 1];

            const float peak   = std::max(std::abs(left), std::abs(right));
            const float needed = peak > ceiling ? ceiling / peak : 1.0F;
            // Down at once, up smoothly; never above what this frame needs
            current_gain = needed < current_gain ? needed : needed - ((needed - current_gain) * release_coefficient);

            // The clamp only catches the rounding of ceiling / peak * peak
            left  = std::clamp(left * current_gain, -ceiling, ceiling);
            right = std::clamp(right * current_gain, -ceiling, ceiling);
        }
    }
} // namespace djw::core::engine
