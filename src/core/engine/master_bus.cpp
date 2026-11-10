// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/master_bus.hpp"

#include <algorithm>

namespace djw::core::engine
{
    MasterBus::MasterBus(std::uint32_t sample_rate) noexcept : limiter{sample_rate}, meter{sample_rate} {}

    void MasterBus::setVolume(float value) noexcept // [audio-thread]
    {
        volume_ramp.setTarget(std::clamp(value, 0.0F, 1.0F));
    }

    void MasterBus::process(std::span<float> samples) noexcept // [audio-thread]
    {
        volume_ramp.applyTo(samples);
        limiter.process(samples);
        meter.process(samples);
    }
} // namespace djw::core::engine
