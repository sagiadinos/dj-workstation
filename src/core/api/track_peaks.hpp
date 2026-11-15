// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_TRACK_PEAKS_HPP
#define DJW_CORE_API_TRACK_PEAKS_HPP

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace djw::core::api
{
    /**
     * The fine waveform of a loaded track, for the scrolling beat waveforms: the loudest sample of every
     * frames_per_peak frames, 0 (silent) to 255 (full scale). Made when a track is loaded into memory and filled while
     * it is decoded, so a part not decoded yet reads 0. One thread writes (the decoding one), others read; values are
     * single bytes, so a reader sees a value either before or after it was written.
     */
    class TrackPeaks
    {
        public:
            /// About 190 values per second at 48 kHz
            static constexpr std::size_t frames_per_peak = 256;

            explicit TrackPeaks(std::uint64_t length_frames)
                : peaks((length_frames + frames_per_peak - 1) / frames_per_peak)
            {
            }

            [[nodiscard]] std::size_t size() const noexcept { return peaks.size(); }
            /// The value at index, 0 outside the track
            [[nodiscard]] std::uint8_t at(std::size_t index) const noexcept
            {
                return index < peaks.size() ? peaks[index].load(std::memory_order_relaxed) : std::uint8_t{0};
            }

            /// Writer: takes interleaved stereo starting at first_frame into the values it falls in
            void add(std::uint64_t first_frame, std::span<const float> stereo) noexcept
            {
                constexpr std::size_t channels = 2;
                const std::size_t     frames   = stereo.size() / channels;
                std::size_t           frame    = 0;
                while (frame < frames)
                {
                    const std::uint64_t at    = first_frame + frame;
                    const auto          index = static_cast<std::size_t>(at / frames_per_peak);
                    const std::size_t   end   = std::min(frames, frame + (frames_per_peak - (at % frames_per_peak)));
                    float               loud  = 0.0F;
                    for (std::size_t sample = frame * channels; sample < end * channels; ++sample)
                    {
                        loud = std::max(loud, std::abs(stereo[sample]));
                    }
                    if (index < peaks.size())
                    {
                        // A value may get frames from two calls; only this thread writes, so load and store suffice
                        const auto value = static_cast<std::uint8_t>(std::lround(std::min(loud, 1.0F) * 255.0F));
                        if (value > peaks[index].load(std::memory_order_relaxed))
                        {
                            peaks[index].store(value, std::memory_order_relaxed);
                        }
                    }
                    frame = end;
                }
            }

        private:
            std::vector<std::atomic<std::uint8_t>> peaks;
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_TRACK_PEAKS_HPP
