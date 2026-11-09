// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_MIXER_CHANNEL_HPP
#define DJW_CORE_ENGINE_MIXER_CHANNEL_HPP

#include <span>

namespace djw::core::engine
{
    /**
     * Mixer channel of one deck: applies the channel volume and adds the signal to the mix.
     *
     * A volume change is ramped linearly over one audio block, so moving the fader does not click.
     */
    class MixerChannel
    {
        public:
            /// Target volume, clamped to 0.0 ... 1.0
            void  setVolume(float value) noexcept; // [audio-thread]
            float volume() const noexcept { return target_volume; }

            /// Adds input * volume to output, both interleaved stereo of the same size.
            void mixInto(std::span<const float> input, std::span<float> output) noexcept; // [audio-thread]

        private:
            float current_volume = 1.0F;
            float target_volume  = 1.0F;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_MIXER_CHANNEL_HPP
