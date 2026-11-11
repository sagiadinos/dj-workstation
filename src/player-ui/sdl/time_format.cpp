// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "time_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace djw::player_ui::sdl
{
    namespace
    {
        /// "m:ss" of whole seconds
        std::string minutesSeconds(std::int64_t whole_seconds)
        {
            const auto  seconds = whole_seconds % 60;
            std::string text    = std::to_string(whole_seconds / 60) + ':';
            if (seconds < 10)
            {
                text += '0';
            }
            return text + std::to_string(seconds);
        }
    } // namespace

    std::string formatPosition(double seconds)
    {
        // Truncated, not rounded: the display must not show a tenth that has not been played yet
        const auto tenths = static_cast<std::int64_t>(std::max(0.0, seconds) * 10.0);
        return minutesSeconds(tenths / 10) + '.' + std::to_string(tenths % 10);
    }

    std::string formatClock(double seconds, bool remaining)
    {
        const auto        hundredths = static_cast<std::int64_t>(std::max(0.0, seconds) * 100.0);
        const std::int64_t whole      = hundredths / 100;
        const auto         two        = [](std::int64_t value) { return (value < 10 ? "0" : "") + std::to_string(value); };
        return std::string{remaining ? "-" : ""} + two(whole / 3'600) + ':' + two((whole / 60) % 60) + ':' +
               two(whole % 60) + ':' + two(hundredths % 100);
    }

    std::string formatBpm(double bpm)
    {
        const long tenths = std::lround(std::max(0.0, bpm) * 10.0);
        return std::to_string(tenths / 10) + '.' + std::to_string(tenths % 10);
    }

    std::string formatDuration(double seconds)
    {
        return minutesSeconds(std::llround(std::max(0.0, seconds)));
    }
} // namespace djw::player_ui::sdl
