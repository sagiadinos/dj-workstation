// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_HEADPHONE_BUS_HPP
#define DJW_CORE_ENGINE_HEADPHONE_BUS_HPP

#include "engine/gain_ramp.hpp"
#include "engine/limiter.hpp"

#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /// What the headphones are made of, interleaved stereo of the same size
    struct HeadphoneSources
    {
            /// Sum of the cued decks
            std::span<const float> cue;
            std::span<const float> master;
    };

    /**
     * Headphone section of the mixer: blends the cued decks with the master, applies the headphone volume and
     * limits the result, so the headphones never clip either.
     */
    class HeadphoneBus
    {
        public:
            explicit HeadphoneBus(std::uint32_t sample_rate) noexcept : limiter{sample_rate} {}

            /// 0.0 only the cued decks ... 1.0 only the master, clamped
            void                setCueMix(float value) noexcept; // [audio-thread]
            [[nodiscard]] float cueMix() const noexcept { return cue_mix; }

            /// 0.0 ... 1.0, clamped
            void                setVolume(float value) noexcept; // [audio-thread]
            [[nodiscard]] float volume() const noexcept { return headphone_volume; }

            /// output = (cue * (1 - cue mix) + master * cue mix) * volume, limited; all interleaved stereo
            void process(HeadphoneSources sources, std::span<float> output) noexcept; // [audio-thread]

        private:
            float    cue_mix          = 0.0F;
            float    headphone_volume = 1.0F;
            GainRamp cue_gain{1.0F};
            GainRamp master_gain{0.0F};
            Limiter  limiter;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_HEADPHONE_BUS_HPP
