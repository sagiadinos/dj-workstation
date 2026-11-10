// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/output_routing.hpp"

#include "engine/audio_source.hpp"

#include <algorithm>

namespace djw::core::engine
{
    void routeOutput(OutputSignals signals, std::span<float> output,
                     std::size_t output_channels) noexcept // [audio-thread]
    {
        const std::span<const float> master          = signals.master;
        const std::span<const float> headphones      = signals.headphones;
        const bool                   with_headphones = output_channels >= headphone_output_channels;
        const std::size_t frames = std::min(master.size() / stereo_channels, output.size() / output_channels);
        std::ranges::fill(output.first(frames * output_channels), 0.0F);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const std::span<float> out = output.subspan(frame * output_channels, output_channels);
            out[0]                     = master[frame * stereo_channels];
            out[1]                     = master[(frame * stereo_channels) + 1];
            if (with_headphones)
            {
                out[2] = headphones[frame * stereo_channels];
                out[3] = headphones[(frame * stereo_channels) + 1];
            }
        }
    }
} // namespace djw::core::engine
