// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_RESAMPLER_HPP
#define DJW_CORE_ENGINE_RESAMPLER_HPP

#include "engine/audio_source.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace djw::core::engine
{
    /**
     * Plays an AudioSource at variable speed (varispeed: tempo and pitch change together).
     *
     * 4-point Hermite (Catmull-Rom) interpolation: cheap enough for the audio thread on a Raspberry Pi, exact at
     * speed 1.0 (the output is the input, bit for bit) and exact for linear signals at any speed. It keeps a window
     * of input frames: one before the current position (history) and two after it (lookahead). The source is
     * therefore read two frames ahead of the position; after the last frame of the source two frames of silence
     * serve as lookahead.
     */
    class Resampler
    {
        public:
            /// Input frames held at most; more only means fewer calls into the source
            static constexpr std::size_t capacity_frames = 1'024;

            Resampler();

            /// Starts over at frame of the source (after loading or a seek); the history is silence.
            void reset(std::uint64_t frame) noexcept; // [audio-thread]

            /// Fills up to output.size() / 2 frames of interleaved stereo, reading source at speed (1.0 = original).
            /// Returns the frames produced; fewer when the source has nothing more right now or the end is reached.
            std::size_t process(AudioSource &source, double speed, std::span<float> output) noexcept; // [audio-thread]

            /// Position of the next output frame in source frames
            [[nodiscard]] double position() const noexcept { return origin + read; }

            /// Every frame of the source has been played
            [[nodiscard]] bool drained() const noexcept;

        private:
            /// Makes sure the frames up to index floor(read) + 2 are in the buffer. False if the source has nothing
            /// right now (not decoded yet).
            bool fill(AudioSource &source) noexcept; // [audio-thread]
            /// Moves the frames still needed to the start of the buffer
            void compact() noexcept; // [audio-thread]

            std::vector<float> buffer;
            std::size_t        count        = 0; ///< frames in the buffer
            double             read         = 0; ///< position in the buffer: index of the current frame plus fraction
            double             origin       = 0; ///< source frame of buffer index 0
            bool               source_ended = false;
            std::size_t        real_end     = 0; ///< buffer index after the last frame of the source
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_RESAMPLER_HPP
