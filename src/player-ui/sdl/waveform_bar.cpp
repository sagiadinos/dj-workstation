// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "waveform_bar.hpp"

#include "look.hpp"

#include <algorithm>
#include <cmath>

namespace djw::player_ui::sdl
{
    namespace
    {
        // At the design height of the bar one bar per 2 points; bigger bars grow with the bar, so a big window
        // does not turn into hair-thin lines
        constexpr float design_height = 54.0F;
        constexpr float bar_pitch     = 2.0F;
        constexpr float bar_width     = 1.4F;

        constexpr Color unplayed_color{40, 140, 245};
        constexpr Color loading_color{105, 108, 115};
        constexpr Color played_color{255, 205, 0};
        constexpr Color idle_line{70, 72, 78};
        constexpr Color playhead_color{255, 255, 255};
        constexpr Color cue_color{235, 55, 55};

        /// The share of the columns that may lie below the bottom of the bar: a quiet intro or fade does not squeeze
        /// the rest of the track
        constexpr double quiet_share = 0.05;
        /// The span shown is at least this many values (12 dB at 60 dB for 255), so a track of an even loudness
        /// does not turn its little ups and downs into big ones
        constexpr std::uint8_t least_span = 51;
    } // namespace

    void WaveformBar::setWaveform(std::vector<std::uint8_t> peaks)
    {
        waveform = std::move(peaks);
        if (waveform.empty())
        {
            bottom = 0;
            top    = 255;
            return;
        }
        // Pulled apart: the loudest column at the top, the quiet end of the track (without its quietest 5 %) at
        // the bottom
        std::vector<std::uint8_t> sorted = waveform;
        std::ranges::sort(sorted);
        top                     = sorted.back();
        const auto quiet        = static_cast<std::size_t>(static_cast<double>(sorted.size() - 1) * quiet_share);
        const int  lowest_shown = std::min<int>(sorted[quiet], top - least_span);
        bottom                  = static_cast<std::uint8_t>(std::max(0, lowest_shown));
    }

    void WaveformBar::setLoaded(float fraction) noexcept
    {
        loaded_share = std::clamp(fraction, 0.0F, 1.0F);
    }

    void WaveformBar::setPosition(float fraction) noexcept
    {
        played = std::clamp(fraction, 0.0F, 1.0F);
    }

    void WaveformBar::pointerDown(Point position)
    {
        is_held = true;
        follow(position);
    }

    void WaveformBar::pointerMove(Point position)
    {
        follow(position);
    }

    void WaveformBar::pointerUp(Point /*position*/)
    {
        is_held = false;
    }

    void WaveformBar::follow(Point position) noexcept
    {
        const Rect area = inner();
        if (area.width <= 0.0F)
        {
            return;
        }
        played = std::clamp((position.x - area.x) / area.width, 0.0F, 1.0F);
        chosen = played;
    }

    void WaveformBar::draw(Canvas &canvas) const
    {
        const Rect &outer = bounds();
        if (outer.width <= 0.0F || outer.height <= 0.0F)
        {
            return;
        }
        look::sunken(canvas, outer);
        const Rect  area     = inner();
        const float middle   = area.y + (area.height / 2);
        const float played_x = area.x + (played * area.width);

        const float loaded_x = area.x + (loaded_share * area.width);
        if (waveform.empty())
        {
            // No waveform (yet): a line, blue as far as the track is loaded
            canvas.fillRect({area.x, middle - 1.0F, area.width, 2.0F}, idle_line);
            canvas.fillRect({area.x, middle - 1.5F, loaded_x - area.x, 3.0F}, unplayed_color);
        }
        else
        {
            const float grow = std::max(1.0F, area.height / design_height);
            const auto  bars = std::max<std::size_t>(1, static_cast<std::size_t>(area.width / (bar_pitch * grow)));
            for (std::size_t bar = 0; bar < bars; ++bar)
            {
                // The columns this bar stands for: the loudest of them
                const std::size_t first = bar * waveform.size() / bars;
                const std::size_t last  = std::max(first + 1, (bar + 1) * waveform.size() / bars);
                std::uint8_t      peak  = 0;
                for (std::size_t index = first; index < last && index < waveform.size(); ++index)
                {
                    peak = std::max(peak, waveform[index]);
                }
                const float span = std::max(1.0F, static_cast<float>(top) - static_cast<float>(bottom));
                const float share =
                    std::clamp((static_cast<float>(peak) - static_cast<float>(bottom)) / span, 0.0F, 1.0F);
                const float height = std::max(2.0F, share * area.height * 0.92F);
                const float x      = area.x + (static_cast<float>(bar) * area.width / static_cast<float>(bars));
                // Yellow: played; blue: in memory; gray: still loading
                const Color color = x >= loaded_x ? loading_color : (x < played_x ? played_color : unplayed_color);
                canvas.fillRect({x, middle - (height / 2), bar_width * grow, height}, color);
            }
        }

        if (cue)
        {
            const float x = area.x + (std::clamp(*cue, 0.0F, 1.0F) * area.width);
            canvas.fillRect({x - 1.0F, area.y, 2.0F, area.height}, cue_color);
            canvas.fillTriangle({x - 5.0F, area.y}, {x + 5.0F, area.y}, {x, area.y + 7.0F}, cue_color);
        }
        canvas.fillRect({played_x - 1.0F, area.y, 2.0F, area.height}, playhead_color);
    }
} // namespace djw::player_ui::sdl
