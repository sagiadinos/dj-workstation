// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "time_format.hpp"

#include <algorithm>
#include <cstdint>

namespace djw::player_ui::sdl
{
    std::string formatPosition(double seconds)
    {
        // Truncated, not rounded: the display must not show a tenth that has not been played yet
        const auto tenths  = static_cast<std::int64_t>(std::max(0.0, seconds) * 10.0);
        const auto minutes = tenths / 600;
        const auto rest    = tenths % 600;

        std::string text = std::to_string(minutes) + ':';
        if (rest < 100)
        {
            text += '0';
        }
        text += std::to_string(rest / 10) + '.' + std::to_string(rest % 10);
        return text;
    }
} // namespace djw::player_ui::sdl
