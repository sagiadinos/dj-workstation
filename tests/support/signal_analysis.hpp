// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_SIGNAL_ANALYSIS_HPP
#define DJW_TESTS_SUPPORT_SIGNAL_ANALYSIS_HPP

#include <cmath>
#include <cstddef>
#include <vector>

namespace djw::tests
{
    /// Frequency in Hz of the left channel of interleaved stereo, from its rising zero crossings; 0 if too few
    inline double frequencyOf(const std::vector<float> &stereo, double sample_rate)
    {
        const std::size_t frames    = stereo.size() / 2;
        std::size_t       crossings = 0;
        double            first     = 0;
        double            last      = 0;
        for (std::size_t frame = 1; frame < frames; ++frame)
        {
            const float before = stereo[(frame - 1) * 2];
            const float after  = stereo[frame * 2];
            if (before < 0.0F && after >= 0.0F)
            {
                // Where the line between the two samples crosses zero
                const double at = static_cast<double>(frame - 1) + (-before / (after - before));
                if (crossings == 0)
                {
                    first = at;
                }
                last = at;
                ++crossings;
            }
        }
        return crossings > 1 ? static_cast<double>(crossings - 1) * sample_rate / (last - first) : 0.0;
    }

    /// Root mean square of all samples
    inline double rmsOf(const std::vector<float> &samples)
    {
        double sum = 0;
        for (const float value : samples)
        {
            sum += static_cast<double>(value) * value;
        }
        return samples.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(samples.size()));
    }
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_SIGNAL_ANALYSIS_HPP
