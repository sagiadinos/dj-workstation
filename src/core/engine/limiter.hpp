// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_LIMITER_HPP
#define DJW_CORE_ENGINE_LIMITER_HPP

#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /**
     * Master limiter: keeps every output sample within ±1.0, so the output never clips.
     *
     * The gain drops at once to what the current frame needs (no look-ahead, so no added latency) and recovers
     * smoothly afterwards. Both channels share the gain, so the stereo image stays. Below full scale the signal
     * passes unchanged.
     */
    class Limiter
    {
        public:
            static constexpr float  ceiling         = 1.0F;
            static constexpr double release_seconds = 0.1;

            explicit Limiter(std::uint32_t sample_rate) noexcept;

            /// Limits one block of interleaved stereo in place
            void process(std::span<float> samples) noexcept; // [audio-thread]

            /// 1.0 when not limiting, lower while limiting
            [[nodiscard]] float gain() const noexcept { return current_gain; }

        private:
            /// Per frame, how much of the distance to the needed gain remains
            float release_coefficient;
            float current_gain = 1.0F;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_LIMITER_HPP
