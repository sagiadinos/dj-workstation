// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/headphone_bus.hpp"

#include <algorithm>

namespace djw::core::engine
{
    void HeadphoneBus::setCueMix(float value) noexcept // [audio-thread]
    {
        cue_mix = std::clamp(value, 0.0F, 1.0F);
    }

    void HeadphoneBus::setVolume(float value) noexcept // [audio-thread]
    {
        headphone_volume = std::clamp(value, 0.0F, 1.0F);
    }

    void HeadphoneBus::process(HeadphoneSources sources, std::span<float> output) noexcept // [audio-thread]
    {
        std::ranges::fill(output, 0.0F);
        cue_gain.setTarget((1.0F - cue_mix) * headphone_volume);
        master_gain.setTarget(cue_mix * headphone_volume);
        cue_gain.addTo(sources.cue, output);
        master_gain.addTo(sources.master, output);
        limiter.process(output);
    }
} // namespace djw::core::engine
