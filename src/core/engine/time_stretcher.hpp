// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_TIME_STRETCHER_HPP
#define DJW_CORE_ENGINE_TIME_STRETCHER_HPP

#include "api/time_stretch.hpp"
#include "engine/audio_source.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace djw::core::engine
{
    /**
     * Plays an AudioSource at a different tempo without changing its pitch (keylock, master tempo). Made by
     * makeTimeStretcher() with one of the libraries built in (SoundTouch, Signalsmith Stretch); the deck sees
     * only this interface.
     *
     * Like the Resampler it reads the source one after the other; the deck seeks the source and then reset()s. The
     * position follows what has been played: the frames fed to the library minus what it still holds, at the tempo.
     * It is exact to within a few milliseconds, which is enough for display and CUE.
     *
     * Construction allocates everything the stretcher will need (and may run the library once over the tempo range
     * so its buffers are as large as they get); the methods marked [audio-thread] do not touch the heap afterwards
     * (a test counts allocations for every library). Construct it outside the audio thread.
     */
    class TimeStretcher
    {
        public:
            /// Slowest and fastest tempo the buffers are prepared for
            static constexpr double min_tempo = 0.5;
            static constexpr double max_tempo = 2.0;

            TimeStretcher()                                 = default;
            TimeStretcher(const TimeStretcher &)            = delete;
            TimeStretcher &operator=(const TimeStretcher &) = delete;
            TimeStretcher(TimeStretcher &&)                 = delete;
            TimeStretcher &operator=(TimeStretcher &&)      = delete;
            virtual ~TimeStretcher()                        = default;

            /// Starts over at frame of the source (after loading, a seek or when keylock is turned on); nothing is held
            virtual void reset(std::uint64_t frame) noexcept = 0; // [audio-thread]
            /// Fills up to output.size() / 2 frames of interleaved stereo with the source at tempo (1.0 = original,
            /// clamped to min_tempo ... max_tempo). Returns the frames produced; fewer when the source has nothing
            /// more right now or the end is reached.
            virtual std::size_t process(AudioSource &source, double tempo,
                                        std::span<float> output) noexcept = 0; // [audio-thread]
            /// Source frame of the next output frame
            [[nodiscard]] virtual double position() const noexcept = 0; // [audio-thread]
            /// Every frame of the source has been played
            [[nodiscard]] virtual bool drained() const noexcept = 0; // [audio-thread]
    };

    /// A stretcher with the library chosen; one that is not built in gives SoundTouch
    [[nodiscard]] std::unique_ptr<TimeStretcher> makeTimeStretcher(api::TimeStretch library, std::uint32_t sample_rate);
    /// The libraries built in, SoundTouch first
    [[nodiscard]] std::vector<api::TimeStretch> builtTimeStretchers();

    // The libraries one by one; makeTimeStretcher() chooses among them. Signalsmith Stretch exists only when built
    // in (DJW_WITH_SIGNALSMITH_STRETCH).
    [[nodiscard]] std::unique_ptr<TimeStretcher> makeSoundTouchStretcher(std::uint32_t sample_rate);
    [[nodiscard]] std::unique_ptr<TimeStretcher> makeSignalsmithStretcher(std::uint32_t sample_rate);
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_TIME_STRETCHER_HPP
