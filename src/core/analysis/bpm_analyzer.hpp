// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ANALYSIS_BPM_ANALYZER_HPP
#define DJW_CORE_ANALYSIS_BPM_ANALYZER_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace djw::core::analysis
{
    /**
     * Finds the tempo of a track (QM-DSP: onset detection function and beat tracking).
     *
     * Feed it the decoded track in blocks, then ask for the result. Runs on an analysis thread, never on the audio
     * thread. The tempo is folded into 70 to 180 BPM, because a beat tracker often finds half or double the tempo a
     * DJ would count; what is still wrong the user corrects by halving or doubling.
     */
    class BpmAnalyzer
    {
        public:
            static constexpr double min_bpm = 70.0;
            static constexpr double max_bpm = 180.0;

            explicit BpmAnalyzer(std::uint32_t sample_rate);
            BpmAnalyzer(const BpmAnalyzer &)            = delete;
            BpmAnalyzer &operator=(const BpmAnalyzer &) = delete;
            BpmAnalyzer(BpmAnalyzer &&)                 = delete;
            BpmAnalyzer &operator=(BpmAnalyzer &&)      = delete;
            ~BpmAnalyzer();

            /// Interleaved stereo float32 at the sample rate given to the constructor
            void feed(std::span<const float> stereo);

            /// The tempo, nullopt if the track is too short or has no beat. Call once, after the last feed().
            [[nodiscard]] std::optional<double> finish();

        private:
            struct Impl;
            std::unique_ptr<Impl> impl;
    };
} // namespace djw::core::analysis

#endif // DJW_CORE_ANALYSIS_BPM_ANALYZER_HPP
