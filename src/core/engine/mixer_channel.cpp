// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/mixer_channel.hpp"

#include <algorithm>

namespace djw::core::engine
{
    void MixerChannel::setVolume(float value) noexcept // [audio-thread]
    {
        channel_volume = std::clamp(value, 0.0F, 1.0F);
    }

    void MixerChannel::setHeadphoneLevel(float value) noexcept // [audio-thread]
    {
        cue_level = std::clamp(value, 0.0F, 1.0F);
    }

    void MixerChannel::mixInto(std::span<const float> input, MixTargets targets,
                               const Crossfader &crossfader) noexcept // [audio-thread]
    {
        meter.process(input);
        gain.setTarget(channel_volume * crossfader.gain(crossfader_side));
        gain.addTo(input, targets.master);
        cue_gain.setTarget(cue_enabled ? cue_level : 0.0F);
        cue_gain.addTo(input, targets.cue);
    }
} // namespace djw::core::engine
