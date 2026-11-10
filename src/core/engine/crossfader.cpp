// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/crossfader.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace djw::core::engine
{
    namespace
    {
        /// Constant power curve over 0.0 (silent) ... 1.0 (full), exact at both ends
        float constantPower(float amount) noexcept
        {
            if (amount <= 0.0F)
            {
                return 0.0F;
            }
            if (amount >= 1.0F)
            {
                return 1.0F;
            }
            return std::sin(amount * std::numbers::pi_v<float> / 2.0F);
        }
    } // namespace

    void Crossfader::setPosition(float value) noexcept // [audio-thread]
    {
        fader_position = std::clamp(value, -1.0F, 1.0F);
    }

    float Crossfader::effectivePosition() const noexcept
    {
        switch (momentary_side)
        {
            case api::CrossfaderSide::A:
                return -1.0F;
            case api::CrossfaderSide::B:
                return 1.0F;
            case api::CrossfaderSide::None:
                break;
        }
        return fader_position;
    }

    float Crossfader::gain(api::CrossfaderSide side) const noexcept // [audio-thread]
    {
        // 0.0 at side A's end, 1.0 at side B's end
        const float towards_b = (effectivePosition() + 1.0F) / 2.0F;
        switch (side)
        {
            case api::CrossfaderSide::A:
                return constantPower(1.0F - towards_b);
            case api::CrossfaderSide::B:
                return constantPower(towards_b);
            case api::CrossfaderSide::None:
                break;
        }
        return 1.0F;
    }
} // namespace djw::core::engine
