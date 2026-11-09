// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "golden.hpp"

#include <cmath>
#include <cstdlib>
#include <string_view>

namespace djw::tests
{
    namespace
    {
        bool updateRequested()
        {
            const char *value = std::getenv("DJW_UPDATE_GOLDEN");
            return value != nullptr && std::string_view{value} == "1";
        }
    } // namespace

    GoldenResult compareWithGolden(const StereoAudio &rendered, const std::filesystem::path &golden_file,
                                   float tolerance)
    {
        if (updateRequested())
        {
            if (auto written = writeWav(golden_file, rendered); !written)
            {
                return {false, written.error()};
            }
            return {true, "golden file updated: " + golden_file.string()};
        }

        const auto golden = readWav(golden_file);
        if (!golden)
        {
            return {false, golden.error() + " (create it with DJW_UPDATE_GOLDEN=1)"};
        }
        if (golden->sample_rate != rendered.sample_rate)
        {
            return {false, "sample rate " + std::to_string(rendered.sample_rate) + ", golden has " +
                               std::to_string(golden->sample_rate)};
        }
        if (golden->samples.size() != rendered.samples.size())
        {
            return {false, std::to_string(rendered.samples.size()) + " samples, golden has " +
                               std::to_string(golden->samples.size())};
        }

        float       max_deviation = 0.0F;
        std::size_t worst_sample  = 0;
        for (std::size_t i = 0; i < rendered.samples.size(); ++i)
        {
            const float deviation = std::abs(rendered.samples[i] - golden->samples[i]);
            if (deviation > max_deviation)
            {
                max_deviation = deviation;
                worst_sample  = i;
            }
        }
        if (max_deviation > tolerance)
        {
            return {false, "max deviation " + std::to_string(max_deviation) + " at frame " +
                               std::to_string(worst_sample / 2) + " exceeds tolerance " + std::to_string(tolerance)};
        }
        return {true, {}};
    }
} // namespace djw::tests
