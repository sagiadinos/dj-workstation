// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_TIME_STRETCHER_HPP
#define DJW_CORE_ENGINE_TIME_STRETCHER_HPP

#include "engine/audio_source.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace djw::core::engine
{
    /**
     * Plays an AudioSource at a different tempo without changing its pitch (keylock, master tempo), with SoundTouch.
     *
     * Like the Resampler it reads the source one after the other; the deck seeks the source and then reset()s. The
     * position follows what has been played: the frames fed to SoundTouch minus what it still holds. It is exact to
     * within the length of SoundTouch's processing window (some 10 ms), which is enough for display and CUE.
     *
     * The constructor allocates and runs SoundTouch through the tempo range once, so its buffers are as large as they
     * get; process() does not touch the heap afterwards (a test counts allocations). Construct it outside the audio
     * thread.
     */
    class TimeStretcher
    {
        public:
            /// Slowest and fastest tempo the buffers are prepared for
            static constexpr double min_tempo = 0.5;
            static constexpr double max_tempo = 2.0;

            explicit TimeStretcher(std::uint32_t sample_rate);
            TimeStretcher(const TimeStretcher &)            = delete;
            TimeStretcher &operator=(const TimeStretcher &) = delete;
            TimeStretcher(TimeStretcher &&)                 = delete;
            TimeStretcher &operator=(TimeStretcher &&)      = delete;
            ~TimeStretcher();

            /// Starts over at frame of the source (after loading, a seek or when keylock is turned on); nothing is held
            void reset(std::uint64_t frame) noexcept; // [audio-thread]

            /// Fills up to output.size() / 2 frames of interleaved stereo with the source at tempo (1.0 = original,
            /// clamped to min_tempo ... max_tempo). Returns the frames produced; fewer when the source has nothing
            /// more right now or the end is reached.
            std::size_t process(AudioSource &source, double tempo, std::span<float> output) noexcept; // [audio-thread]

            /// Source frame of the next output frame
            [[nodiscard]] double position() const noexcept; // [audio-thread]

            /// Every frame of the source has been played
            [[nodiscard]] bool drained() const noexcept; // [audio-thread]

        private:
            struct Impl;
            std::unique_ptr<Impl> impl;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_TIME_STRETCHER_HPP
