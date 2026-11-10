// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/resampler.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace djw::core::engine
{
    namespace
    {
        /// Catmull-Rom between points[1] and points[2], shaped by points[0] and points[3]; t = 0 gives exactly
        /// points[1]
        float hermite(const std::array<float, 4> &points, float t) noexcept
        {
            const auto [previous, x0, x1, next] = points;
            const float c1                      = 0.5F * (x1 - previous);
            const float c2                      = previous - 2.5F * x0 + 2.0F * x1 - 0.5F * next;
            const float c3                      = 0.5F * (next - previous) + 1.5F * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        }
    } // namespace

    Resampler::Resampler() : buffer(capacity_frames * stereo_channels)
    {
        reset(0);
    }

    void Resampler::reset(std::uint64_t frame) noexcept // [audio-thread]
    {
        // Index 0 is the silent history before frame, index 1 is frame
        buffer[0]    = 0.0F;
        buffer[1]    = 0.0F;
        count        = 1;
        read         = 1.0;
        origin       = static_cast<double>(frame) - 1.0;
        source_ended = false;
        real_end     = 0;
    }

    bool Resampler::drained() const noexcept
    {
        return source_ended && static_cast<std::size_t>(read) >= real_end;
    }

    std::size_t Resampler::process(AudioSource &source, double speed,
                                   std::span<float> output) noexcept // [audio-thread]
    {
        const std::size_t frames   = output.size() / stereo_channels;
        std::size_t       produced = 0;
        while (produced < frames)
        {
            if (drained() || !fill(source))
            {
                break;
            }
            const auto   index = static_cast<std::size_t>(read);
            const auto   t     = static_cast<float>(read - static_cast<double>(index));
            const float *x     = buffer.data() + (index - 1) * stereo_channels;
            for (std::size_t channel = 0; channel < stereo_channels; ++channel)
            {
                output[produced * stereo_channels + channel] =
                    hermite({x[channel], x[stereo_channels + channel], x[2 * stereo_channels + channel],
                             x[3 * stereo_channels + channel]},
                            t);
            }
            read += speed;
            ++produced;
        }
        return produced;
    }

    bool Resampler::fill(AudioSource &source) noexcept // [audio-thread]
    {
        while (count < static_cast<std::size_t>(read) + 3)
        {
            if (count == capacity_frames)
            {
                compact();
                continue;
            }
            if (source_ended)
            {
                // Silence as lookahead after the last frame
                std::fill_n(buffer.begin() + static_cast<std::ptrdiff_t>(count * stereo_channels), stereo_channels,
                            0.0F);
                ++count;
                continue;
            }
            const std::size_t got = source.read(std::span{buffer}.subspan(count * stereo_channels));
            if (got == 0)
            {
                if (!source.atEnd())
                {
                    return false;
                }
                source_ended = true;
                real_end     = count;
                continue;
            }
            count += got;
        }
        return true;
    }

    void Resampler::compact() noexcept // [audio-thread]
    {
        // Keep the history frame before the current one
        const std::size_t shift = static_cast<std::size_t>(read) - 1;
        std::copy(buffer.begin() + static_cast<std::ptrdiff_t>(shift * stereo_channels),
                  buffer.begin() + static_cast<std::ptrdiff_t>(count * stereo_channels), buffer.begin());
        count -= shift;
        read -= static_cast<double>(shift);
        origin += static_cast<double>(shift);
        if (source_ended)
        {
            real_end -= std::min(real_end, shift);
        }
    }
} // namespace djw::core::engine
