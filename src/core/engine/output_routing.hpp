// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_OUTPUT_ROUTING_HPP
#define DJW_CORE_ENGINE_OUTPUT_ROUTING_HPP

#include <cstddef>
#include <span>

namespace djw::core::engine
{
    /// Channels from which on the headphones are routed: master on 1/2, headphones on 3/4
    inline constexpr std::size_t headphone_output_channels = 4;

    /// The two stereo signals of the mixer that go out, interleaved stereo of the same size
    struct OutputSignals
    {
            std::span<const float> master;
            std::span<const float> headphones;
    };

    /**
     * Writes the stereo master and headphone signals into the device's interleaved output with output_channels
     * channels: master on channels 1/2, headphones on 3/4 if the output has them, silence on any further channel.
     */
    void routeOutput(OutputSignals signals, std::span<float> output,
                     std::size_t output_channels) noexcept; // [audio-thread]
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_OUTPUT_ROUTING_HPP
