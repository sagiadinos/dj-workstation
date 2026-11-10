// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_MIXER_CHANNEL_HPP
#define DJW_CORE_ENGINE_MIXER_CHANNEL_HPP

#include "api/crossfader_side.hpp"
#include "api/snapshot.hpp"
#include "engine/crossfader.hpp"
#include "engine/gain_ramp.hpp"
#include "engine/peak_meter.hpp"

#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /// Where a mixer channel adds its signal to, interleaved stereo of the same size
    struct MixTargets
    {
            std::span<float> master;
            /// Headphone cue, before the faders
            std::span<float> cue;
    };

    /**
     * Mixer channel of one deck: meters the deck, applies channel volume and crossfader and adds the signal to the
     * mix; with headphone cue on, it also adds the signal, taken before the fader, to the headphone cue.
     *
     * Volume, crossfader and cue changes are ramped over one audio block, so moving a fader does not click.
     */
    class MixerChannel
    {
        public:
            explicit MixerChannel(std::uint32_t sample_rate) noexcept : meter{sample_rate} {}

            /// Target volume, clamped to 0.0 ... 1.0
            void                setVolume(float value) noexcept; // [audio-thread]
            [[nodiscard]] float volume() const noexcept { return channel_volume; }

            void setCrossfaderSide(api::CrossfaderSide side) noexcept { crossfader_side = side; } // [audio-thread]
            [[nodiscard]] api::CrossfaderSide crossfaderSide() const noexcept { return crossfader_side; }

            void               setHeadphoneCue(bool enabled) noexcept { cue_enabled = enabled; } // [audio-thread]
            [[nodiscard]] bool headphoneCue() const noexcept { return cue_enabled; }

            /// Level of the deck on the headphones, clamped to 0.0 ... 1.0
            void                setHeadphoneLevel(float value) noexcept; // [audio-thread]
            [[nodiscard]] float headphoneLevel() const noexcept { return cue_level; }

            /// Adds input * volume * crossfader gain to master and input * headphone level (if cued) to cue; all
            /// interleaved stereo of the same size.
            void mixInto(std::span<const float> input, MixTargets targets,
                         const Crossfader &crossfader) noexcept; // [audio-thread]

            /// Level of the input, before the fader
            [[nodiscard]] api::StereoLevel level() const noexcept { return meter.level(); }

        private:
            float               channel_volume  = 1.0F;
            api::CrossfaderSide crossfader_side = api::CrossfaderSide::None;
            bool                cue_enabled     = false;
            float               cue_level       = 1.0F;
            GainRamp            gain;
            GainRamp            cue_gain{0.0F};
            PeakMeter           meter;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_MIXER_CHANNEL_HPP
