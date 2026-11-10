// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_PEAK_METER_HPP
#define DJW_CORE_ENGINE_PEAK_METER_HPP

#include "api/snapshot.hpp"

#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /**
     * Peak meter for the VU displays: a new peak shows at once, then the level falls by 20 dB in 1.7 s (PPM
     * fall back), see api::StereoLevel.
     */
    class PeakMeter
    {
        public:
            static constexpr double fall_decibels = 20.0;
            static constexpr double fall_seconds  = 1.7;

            explicit PeakMeter(std::uint32_t sample_rate) noexcept;

            /// Measures one block of interleaved stereo
            void process(std::span<const float> samples) noexcept; // [audio-thread]

            [[nodiscard]] api::StereoLevel level() const noexcept { return current; }

        private:
            /// Factor the level falls by per frame
            double           fall_per_frame;
            api::StereoLevel current;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_PEAK_METER_HPP
