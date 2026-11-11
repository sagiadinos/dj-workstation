// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "analysis/track_analysis.hpp"

#include "analysis/bpm_analyzer.hpp"
#include "decode/miniaudio_decoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace djw::core::analysis
{
    namespace
    {
        constexpr std::size_t stereo_channels = 2;
        constexpr std::size_t read_frames     = 4'096;
    } // namespace

    void WaveformBuilder::add(std::span<const float> stereo)
    {
        for (std::size_t frame = 0; frame + 1 < stereo.size(); frame += stereo_channels)
        {
            current = std::max({current, std::abs(stereo[frame]), std::abs(stereo[frame + 1])});
            if (++filled == block_frames)
            {
                peaks.push_back(current);
                current = 0.0F;
                filled  = 0;
            }
        }
    }

    std::vector<std::uint8_t> WaveformBuilder::finish() const
    {
        std::vector<float> all = peaks;
        if (filled > 0)
        {
            all.push_back(current);
        }
        const std::size_t         columns = std::min(waveform_columns, all.size());
        std::vector<std::uint8_t> result(columns, 0);
        for (std::size_t column = 0; column < columns; ++column)
        {
            // Block range of this column; every block belongs to exactly one column
            const std::size_t first   = column * all.size() / columns;
            const std::size_t last    = std::max(first + 1, (column + 1) * all.size() / columns);
            float             loudest = 0.0F;
            for (std::size_t block = first; block < last; ++block)
            {
                loudest = std::max(loudest, all[block]);
            }
            result[column] = static_cast<std::uint8_t>(std::lround(std::min(loudest, 1.0F) * 255.0F));
        }
        return result;
    }

    std::expected<TrackAnalysis, std::string> analyzeFile(const std::filesystem::path &file, std::uint32_t sample_rate,
                                                          const std::atomic<bool> &stop)
    {
        auto decoder = decode::MiniaudioDecoder::open(file, sample_rate);
        if (!decoder)
        {
            return std::unexpected{decoder.error()};
        }
        BpmAnalyzer                                      bpm{sample_rate};
        WaveformBuilder                                  waveform;
        std::array<float, read_frames * stereo_channels> block{};
        while (!stop.load(std::memory_order_relaxed))
        {
            const auto read = (*decoder)->read(block);
            if (!read)
            {
                return std::unexpected{read.error()};
            }
            if (*read == 0)
            {
                return TrackAnalysis{.bpm = bpm.finish(), .waveform = waveform.finish()};
            }
            const auto samples = std::span<const float>{block}.first(*read * stereo_channels);
            bpm.feed(samples);
            waveform.add(samples);
        }
        return std::unexpected{"analysis stopped"};
    }
} // namespace djw::core::analysis
