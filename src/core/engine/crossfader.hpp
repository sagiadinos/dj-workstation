// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_CROSSFADER_HPP
#define DJW_CORE_ENGINE_CROSSFADER_HPP

#include "api/crossfader_side.hpp"

namespace djw::core::engine
{
    /**
     * The crossfader: position, momentary A/B and the resulting gain per side.
     *
     * The curve is constant power: at the center both sides are at -3 dB, so two different tracks keep their
     * combined loudness during a fade. At the ends the far side is exactly silent.
     */
    class Crossfader
    {
        public:
            /// -1.0 (A) ... +1.0 (B), clamped
            void                setPosition(float value) noexcept; // [audio-thread]
            [[nodiscard]] float position() const noexcept { return fader_position; }

            /// While not None, the crossfader acts as if it were at the end of that side
            void setMomentary(api::CrossfaderSide side) noexcept { momentary_side = side; } // [audio-thread]
            [[nodiscard]] api::CrossfaderSide momentary() const noexcept { return momentary_side; }

            /// Gain for a deck on side; 1.0 for a deck on None
            [[nodiscard]] float gain(api::CrossfaderSide side) const noexcept; // [audio-thread]

        private:
            [[nodiscard]] float effectivePosition() const noexcept;

            float               fader_position = 0.0F;
            api::CrossfaderSide momentary_side = api::CrossfaderSide::None;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_CROSSFADER_HPP
