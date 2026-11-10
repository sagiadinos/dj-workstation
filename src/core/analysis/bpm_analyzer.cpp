// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "analysis/bpm_analyzer.hpp"

#include "dsp/onsets/DetectionFunction.h"
#include "dsp/tempotracking/TempoTrackV2.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace djw::core::analysis
{
    namespace
    {
        constexpr std::size_t stereo_channels = 2;
        // QM-DSP is tuned for 44.1 kHz with a step of 512 samples (86 detection values per second)
        constexpr double reference_rate = 44'100.0;
        constexpr double reference_step = 512.0;
        // Shorter tracks have too few beats to say anything
        constexpr double min_seconds = 8.0;
        // A detection function that never rises above this has no onsets
        constexpr double silence = 1e-6;

        DFConfig detectionConfig(int step)
        {
            DFConfig config{};
            config.stepSize            = step;
            config.frameLength         = step * 2;
            config.DFType              = DF_COMPLEXSD;
            config.dbRise              = 3.0;
            config.adaptiveWhitening   = false;
            config.whiteningRelaxCoeff = -1;
            config.whiteningFloor      = -1;
            return config;
        }

        int stepFor(std::uint32_t sample_rate)
        {
            return std::max(2, static_cast<int>(std::lround(reference_step * sample_rate / reference_rate)));
        }

        double median(std::vector<double> values)
        {
            const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
            std::ranges::nth_element(values, middle);
            return *middle;
        }
    } // namespace

    struct BpmAnalyzer::Impl
    {
            explicit Impl(std::uint32_t sample_rate)
                : rate{sample_rate}, step{stepFor(sample_rate)}, detection{detectionConfig(step)}
            {
                window.reserve(static_cast<std::size_t>(step) * 2);
            }

            std::uint32_t       rate;
            int                 step;
            DetectionFunction   detection;
            std::vector<double> window;   // the samples of the frame being collected
            std::vector<double> function; // one detection value per step
            std::uint64_t       frames = 0;
    };

    BpmAnalyzer::BpmAnalyzer(std::uint32_t sample_rate) : impl{std::make_unique<Impl>(sample_rate)} {}

    BpmAnalyzer::~BpmAnalyzer() = default;

    void BpmAnalyzer::feed(std::span<const float> stereo)
    {
        const auto frame_length = static_cast<std::size_t>(impl->step) * 2;
        for (std::size_t frame = 0; frame + 1 < stereo.size(); frame += stereo_channels)
        {
            impl->window.push_back((static_cast<double>(stereo[frame]) + static_cast<double>(stereo[frame + 1])) * 0.5);
            ++impl->frames;
            if (impl->window.size() == frame_length)
            {
                impl->function.push_back(impl->detection.processTimeDomain(impl->window.data()));
                impl->window.erase(impl->window.begin(), impl->window.begin() + impl->step);
            }
        }
    }

    std::optional<double> BpmAnalyzer::finish()
    {
        if (static_cast<double>(impl->frames) < min_seconds * impl->rate)
        {
            return std::nullopt;
        }
        // The first values come from the zero start of the phase vocoder
        constexpr std::size_t skip = 2;
        if (impl->function.size() <= skip)
        {
            return std::nullopt;
        }
        const std::vector<double> function(impl->function.begin() + skip, impl->function.end());
        if (std::ranges::max(function) < silence)
        {
            return std::nullopt; // nothing to count
        }

        TempoTrackV2        tracker{static_cast<float>(impl->rate), impl->step};
        std::vector<double> period(function.size(), 0.0); // the tracker fills it, it must have this size
        std::vector<double> tempi;
        std::vector<double> beats;
        tracker.calculateBeatPeriod(function, period, tempi);
        tracker.calculateBeats(function, period, beats);
        if (beats.size() < 8)
        {
            return std::nullopt;
        }

        std::vector<double> intervals;
        intervals.reserve(beats.size() - 1);
        for (std::size_t i = 1; i < beats.size(); ++i)
        {
            intervals.push_back(beats[i] - beats[i - 1]);
        }
        const double seconds_per_step = static_cast<double>(impl->step) / impl->rate;
        double       bpm              = 60.0 / (median(std::move(intervals)) * seconds_per_step);
        if (!std::isfinite(bpm) || bpm <= 0.0)
        {
            return std::nullopt;
        }
        while (bpm < min_bpm)
        {
            bpm *= 2.0;
        }
        while (bpm > max_bpm)
        {
            bpm /= 2.0;
        }
        return bpm;
    }
} // namespace djw::core::analysis
