// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_TRACK_PEAKS_HPP
#define DJW_CORE_API_TRACK_PEAKS_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace djw::core::api
{
    /**
     * The fine waveform of a loaded track, for the scrolling beat waveforms: for every frames_per_peak frames the
     * loudest sample in three bands, each 0 (silent) to 255 (full scale): the bass (below bass_top), the mids
     * (mid_bottom to mid_top) and the highs (above high_bottom). The kick shows in the bass, hi-hats in the highs, as
     * in the three-colour waveforms of other DJ programs. The bands are made with one-pole filters in a row, steep
     * enough (the bass 24 dB per octave) that a kick hardly reaches the mids.
     *
     * Made when a track is loaded into memory and filled while it is decoded, so a part not decoded yet reads 0. One
     * thread writes (the decoding one), others read; the three bands of a value are one atomic word, so a reader sees
     * them either before or after they were written.
     */
    class TrackPeaks
    {
        public:
            /// About 190 values per second at 48 kHz
            static constexpr std::size_t frames_per_peak = 256;
            /// Where the bands end, in Hz
            static constexpr double bass_top    = 200.0;
            static constexpr double mid_bottom  = 300.0;
            static constexpr double mid_top     = 2'500.0;
            static constexpr double high_bottom = 4'000.0;

            struct Bands
            {
                    std::uint8_t low  = 0;
                    std::uint8_t mid  = 0;
                    std::uint8_t high = 0;

                    bool operator==(const Bands &) const = default;
            };

            explicit TrackPeaks(std::uint64_t length_frames, std::uint32_t sample_rate = 48'000)
                : peaks((length_frames + frames_per_peak - 1) / frames_per_peak),
                  bass{.step = stepFor(bass_top, sample_rate)}, mid_low_cut{.step = stepFor(mid_bottom, sample_rate)},
                  mid_high_cut{.step = stepFor(mid_top, sample_rate)}, highs{.step = stepFor(high_bottom, sample_rate)}
            {
            }

            [[nodiscard]] std::size_t size() const noexcept { return peaks.size(); }
            /// The bands at index, all 0 outside the track
            [[nodiscard]] Bands at(std::size_t index) const noexcept
            {
                if (index >= peaks.size())
                {
                    return {};
                }
                const std::uint32_t packed = peaks[index].load(std::memory_order_relaxed);
                return {.low  = static_cast<std::uint8_t>(packed & 0xFFU),
                        .mid  = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU),
                        .high = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU)};
            }

            /// Writer: raises the bands at index to at least bands
            void store(std::size_t index, Bands bands) noexcept
            {
                if (index >= peaks.size())
                {
                    return;
                }
                // Only this thread writes, so load and store suffice
                const Bands         old    = at(index);
                const std::uint32_t packed = std::uint32_t{std::max(old.low, bands.low)} |
                                             (std::uint32_t{std::max(old.mid, bands.mid)} << 8U) |
                                             (std::uint32_t{std::max(old.high, bands.high)} << 16U);
                peaks[index].store(packed, std::memory_order_relaxed);
            }

            /**
             * Writer: takes interleaved stereo starting at first_frame into the values it falls in. Blocks that follow
             * each other are filtered as one signal; a block that starts elsewhere (the decoder jumped) starts the
             * filters again from its first sample.
             */
            void add(std::uint64_t first_frame, std::span<const float> stereo) noexcept
            {
                constexpr std::size_t channels = 2;
                const std::size_t     frames   = stereo.size() / channels;
                if (frames == 0)
                {
                    return;
                }
                if (first_frame != next_frame)
                {
                    // As if the signal had stood at the first sample: no step that is not in the music
                    const float first = (stereo[0] + stereo[1]) / 2;
                    bass.reset(first);
                    mid_low_cut.reset(first);
                    mid_high_cut.reset(0.0F);
                    highs.reset(first);
                }
                next_frame = first_frame + frames;

                float loud_low  = 0.0F;
                float loud_mid  = 0.0F;
                float loud_high = 0.0F;
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    const float mono = (stereo[frame * channels] + stereo[(frame * channels) + 1]) / 2;
                    loud_low         = std::max(loud_low, std::abs(bass.next(mono)));
                    loud_mid         = std::max(loud_mid, std::abs(mid_high_cut.next(mid_low_cut.next(mono))));
                    loud_high        = std::max(loud_high, std::abs(highs.next(mono)));
                    // The value ends with this frame, or with the block: written then
                    const std::uint64_t at = first_frame + frame;
                    if ((at + 1) % frames_per_peak == 0 || frame + 1 == frames)
                    {
                        store(static_cast<std::size_t>(at / frames_per_peak),
                              {.low = level(loud_low), .mid = level(loud_mid), .high = level(loud_high)});
                        loud_low  = 0.0F;
                        loud_mid  = 0.0F;
                        loud_high = 0.0F;
                    }
                }
            }

        private:
            /// One-pole low passes in a row, 6 dB per octave each
            template <std::size_t Stages> struct LowPass
            {
                    float                     step = 0.0F;
                    std::array<float, Stages> state{};

                    float next(float input) noexcept
                    {
                        for (float &stage : state)
                        {
                            stage += step * (input - stage);
                            input = stage;
                        }
                        return input;
                    }
                    /// Settled on a signal standing at value
                    void reset(float value) noexcept { state.fill(value); }
            };

            /// One-pole high passes in a row: each takes the low pass of its input away
            template <std::size_t Stages> struct HighPass
            {
                    float                     step = 0.0F;
                    std::array<float, Stages> state{};

                    float next(float input) noexcept
                    {
                        for (float &stage : state)
                        {
                            stage += step * (input - stage);
                            input -= stage;
                        }
                        return input;
                    }
                    /// Settled on a signal standing at value: the first stage takes it all, the others get nothing
                    void reset(float value) noexcept
                    {
                        state.fill(0.0F);
                        state.front() = value;
                    }
            };

            static float stepFor(double frequency, std::uint32_t sample_rate) noexcept
            {
                return static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * frequency /
                                                         std::max(1.0, static_cast<double>(sample_rate))));
            }

            static std::uint8_t level(float loud) noexcept
            {
                return static_cast<std::uint8_t>(std::lround(std::min(loud, 1.0F) * 255.0F));
            }

            std::vector<std::atomic<std::uint32_t>> peaks;
            // Writer only
            LowPass<4>    bass;
            HighPass<2>   mid_low_cut;
            LowPass<2>    mid_high_cut;
            HighPass<2>   highs;
            std::uint64_t next_frame = static_cast<std::uint64_t>(-1);
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_TRACK_PEAKS_HPP
