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

        /// Keylock with SoundTouch (LGPL-2.1): fast and robust, a little metallic at larger tempo changes
        class SoundTouchStretcher final : public TimeStretcher
        {
            public:
                explicit SoundTouchStretcher(std::uint32_t sample_rate)
                {
                    touch.setSampleRate(sample_rate);
                    touch.setChannels(stereo_channels);
                    // Seeks the best overlap in a coarser grid: much less CPU for a little less quality (Raspberry Pi)
                    touch.setSetting(SETTING_USE_QUICKSEEK, 1);
                    touch.setSetting(SETTING_USE_AA_FILTER, 0); // only matters for rate changes, which are not used
                    warmUp();
                }

                void reset(std::uint64_t frame) noexcept override // [audio-thread]
                {
                    touch.clear();
                    start   = static_cast<double>(frame);
                    fed     = 0;
                    flushed = false;
                }

                std::size_t process(AudioSource &source, double tempo,
                                    std::span<float> output) noexcept override // [audio-thread]
                {
                    tempo = std::clamp(tempo, min_tempo, max_tempo);
                    if (std::abs(tempo - current_tempo) > 1e-6)
                    {
                        touch.setTempo(tempo);
                        current_tempo = tempo;
                    }

                    const std::size_t wanted   = output.size() / stereo_channels;
                    std::size_t       produced = 0;
                    while (produced < wanted)
                    {
                        produced += touch.receiveSamples(output.data() + (produced * stereo_channels),
                                                         static_cast<unsigned int>(wanted - produced));
                        if (produced == wanted || flushed)
                        {
                            break;
                        }
                        const std::size_t read = source.read(chunk);
                        if (read > 0)
                        {
                            fed += static_cast<double>(read);
                            touch.putSamples(chunk.data(), static_cast<unsigned int>(read));
                        }
                        else if (source.atEnd())
                        {
                            touch.flush(); // what SoundTouch still holds comes out as the last frames
                            flushed = true;
                        }
                        else
                        {
                            break; // not decoded yet
                        }
                    }
                    return produced;
                }

                [[nodiscard]] double position() const noexcept override // [audio-thread]
                {
                    // Fed, minus what has not been used yet and what has been used but not played
                    const double behind = static_cast<double>(touch.numUnprocessedSamples()) +
                                          (static_cast<double>(touch.numSamples()) * current_tempo);
                    return start + std::max(0.0, fed - behind);
                }

                [[nodiscard]] bool drained() const noexcept override // [audio-thread]
                {
                    return flushed && touch.numSamples() == 0;
                }

            private:
                /// Runs silence through SoundTouch at the slowest and fastest tempo, so its buffers have grown
                void warmUp()
                {
                    const std::vector<float> silence(warmup_frames * stereo_channels, 0.0F);
                    std::vector<float>       sink(warmup_frames * stereo_channels * 4);
                    const auto               drain = [this, &sink]
                    {
                        while (touch.receiveSamples(sink.data(),
                                                    static_cast<unsigned int>(sink.size() / stereo_channels)) > 0)
                        {
                        }
                    };
                    for (const double tempo : {min_tempo, max_tempo, 1.0})
                    {
                        touch.setTempo(tempo);
                        for (std::size_t given = 0; given < warmup_frames; given += chunk_frames)
                        {
                            touch.putSamples(silence.data(), chunk_frames);
                        }
                        drain();
                        touch.flush();
                        drain();
                        touch.clear();
                    }
                    current_tempo = 1.0;
                }

                soundtouch::SoundTouch                            touch;
                std::array<float, chunk_frames * stereo_channels> chunk{};
                double                                            current_tempo = 1.0;
                double                                            start         = 0; ///< source frame at the last reset
                double                                            fed           = 0; ///< frames given since
                bool                                              flushed       = false;
        };
    } // namespace

    std::unique_ptr<TimeStretcher> makeSoundTouchStretcher(std::uint32_t sample_rate)
    {
        return std::make_unique<SoundTouchStretcher>(sample_rate);
    }
} // namespace djw::core::engine
