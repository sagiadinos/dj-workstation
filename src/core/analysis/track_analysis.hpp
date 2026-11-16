// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ANALYSIS_TRACK_ANALYSIS_HPP
#define DJW_CORE_ANALYSIS_TRACK_ANALYSIS_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace djw::core::analysis
{
    /// Columns of the stored overview waveform, whatever the length of the track
    constexpr std::size_t waveform_columns = 1'024;
    /// The quietest level a waveform column tells apart (value 0), in dBFS; the loudest is 0 dBFS (value 255)
    constexpr double waveform_floor_db = -60.0;

    /// What the analysis finds in a track
    struct TrackAnalysis
    {
            /// nullopt if the track has no beat the analysis could find
            std::optional<double> bpm{};
            /// waveform_columns values: the loudness (RMS) of the track section in dB, 0 for waveform_floor_db or
            /// quieter to 255 for full scale. The loudness, not the peak: a track mastered loud has peaks near full
            /// scale everywhere, its loudness still shows intro, breakdown and drop
            std::vector<std::uint8_t> waveform{};
    };

    /**
     * Collects the overview waveform while a track is decoded: the mean square of every small block, folded into
     * waveform_columns columns at the end (the RMS of the blocks of a column, in dB), so the length of the track need
     * not be known.
     */
    class WaveformBuilder
    {
        public:
            /// Interleaved stereo float32
            void add(std::span<const float> stereo);
            /// Folds the blocks seen so far into waveform_columns columns (fewer blocks give a shorter result)
            [[nodiscard]] std::vector<std::uint8_t> finish() const;

        private:
            static constexpr std::size_t block_frames = 1'024;

            std::vector<double> squares; ///< the mean square of every full block
            double              current = 0.0;
            std::size_t         filled  = 0;
    };

    /**
     * Decodes the whole file and finds tempo and waveform. About a second per track at 44.1 kHz, so it runs on an
     * analysis thread. Stops early when stop is set; the result is then an error. An error also for a file that cannot
     * be decoded.
     */
    std::expected<TrackAnalysis, std::string> analyzeFile(const std::filesystem::path &file, std::uint32_t sample_rate,
                                                          const std::atomic<bool> &stop);
} // namespace djw::core::analysis

#endif // DJW_CORE_ANALYSIS_TRACK_ANALYSIS_HPP
