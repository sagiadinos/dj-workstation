// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/time_stretcher.hpp"

#if __has_include(<soundtouch/SoundTouch.h>)
#include <soundtouch/SoundTouch.h>
#else
#include <SoundTouch.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace djw::core::engine
{
    namespace
    {
        /// Frames read from the source at a time
        constexpr std::size_t chunk_frames = 512;
        /// Frames SoundTouch holds before the warm-up reads them out: more than any backlog in use
        constexpr std::size_t warmup_frames = 16'384;
    } // namespace

    struct TimeStretcher::Impl
    {
            explicit Impl(std::uint32_t sample_rate)
            {
                touch.setSampleRate(sample_rate);
                touch.setChannels(stereo_channels);
                // Seeks the best overlap in a coarser grid: much less CPU for a little less quality (Raspberry Pi)
                touch.setSetting(SETTING_USE_QUICKSEEK, 1);
                touch.setSetting(SETTING_USE_AA_FILTER, 0); // only matters for rate changes, which are not used
                warmUp();
            }

            /// Runs silence through SoundTouch at the slowest and fastest tempo, so its buffers have grown
            void warmUp()
            {
                const std::vector<float> silence(warmup_frames * stereo_channels, 0.0F);
                std::vector<float>       sink(warmup_frames * stereo_channels * 4);
                for (const double tempo : {min_tempo, max_tempo, 1.0})
                {
                    touch.setTempo(tempo);
                    for (std::size_t fed = 0; fed < warmup_frames; fed += chunk_frames)
                    {
                        touch.putSamples(silence.data(), chunk_frames);
                    }
                    while (touch.receiveSamples(sink.data(), static_cast<unsigned int>(sink.size() / stereo_channels)) >
                           0)
                    {
                    }
                    touch.flush();
                    while (touch.receiveSamples(sink.data(), static_cast<unsigned int>(sink.size() / stereo_channels)) >
                           0)
                    {
                    }
                    touch.clear();
                }
                current_tempo = 1.0;
            }

            soundtouch::SoundTouch                            touch;
            std::array<float, chunk_frames * stereo_channels> chunk{};
            double                                            current_tempo = 1.0;
            double                                            start         = 0; ///< source frame at the last reset
            double                                            fed           = 0; ///< frames given to SoundTouch since
            bool                                              flushed       = false;
    };

    TimeStretcher::TimeStretcher(std::uint32_t sample_rate) : impl{std::make_unique<Impl>(sample_rate)} {}

    TimeStretcher::~TimeStretcher() = default;

    void TimeStretcher::reset(std::uint64_t frame) noexcept // [audio-thread]
    {
        impl->touch.clear();
        impl->start   = static_cast<double>(frame);
        impl->fed     = 0;
        impl->flushed = false;
    }

    std::size_t TimeStretcher::process(AudioSource &source, double tempo,
                                       std::span<float> output) noexcept // [audio-thread]
    {
        Impl &state = *impl;
        tempo       = std::clamp(tempo, min_tempo, max_tempo);
        if (std::abs(tempo - state.current_tempo) > 1e-6)
        {
            state.touch.setTempo(tempo);
            state.current_tempo = tempo;
        }

        const std::size_t wanted   = output.size() / stereo_channels;
        std::size_t       produced = 0;
        while (produced < wanted)
        {
            produced += state.touch.receiveSamples(output.data() + (produced * stereo_channels),
                                                   static_cast<unsigned int>(wanted - produced));
            if (produced == wanted || state.flushed)
            {
                break;
            }
            const std::size_t read = source.read(state.chunk);
            if (read > 0)
            {
                state.fed += static_cast<double>(read);
                state.touch.putSamples(state.chunk.data(), static_cast<unsigned int>(read));
            }
            else if (source.atEnd())
            {
                state.touch.flush(); // what SoundTouch still holds comes out as the last frames
                state.flushed = true;
            }
            else
            {
                break; // not decoded yet
            }
        }
        return produced;
    }

    double TimeStretcher::position() const noexcept // [audio-thread]
    {
        const Impl &state = *impl;
        // Fed, minus what has not been used yet and what has been used but not played
        const double behind = static_cast<double>(state.touch.numUnprocessedSamples()) +
                              (static_cast<double>(state.touch.numSamples()) * state.current_tempo);
        return state.start + std::max(0.0, state.fed - behind);
    }

    bool TimeStretcher::drained() const noexcept // [audio-thread]
    {
        return impl->flushed && impl->touch.numSamples() == 0;
    }
} // namespace djw::core::engine
