// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_MASTER_BUS_HPP
#define DJW_CORE_ENGINE_MASTER_BUS_HPP

#include "api/snapshot.hpp"
#include "engine/gain_ramp.hpp"
#include "engine/limiter.hpp"
#include "engine/peak_meter.hpp"

#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /// Master section of the mixer: master volume, then limiter, then the master meter.
    class MasterBus
    {
        public:
            explicit MasterBus(std::uint32_t sample_rate) noexcept;

            /// 0.0 ... 1.0, clamped
            void                setVolume(float value) noexcept; // [audio-thread]
            [[nodiscard]] float volume() const noexcept { return volume_ramp.target(); }

            /// Processes the mixed sum in place, interleaved stereo
            void process(std::span<float> samples) noexcept; // [audio-thread]

            [[nodiscard]] api::StereoLevel level() const noexcept { return meter.level(); }
            // mull-ignore-next
            [[nodiscard]] float limiterGain() const noexcept { return limiter.gain(); }

        private:
            GainRamp  volume_ramp;
            Limiter   limiter;
            PeakMeter meter;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_MASTER_BUS_HPP
