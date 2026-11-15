// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "beat_view.hpp"

#include "look.hpp"
#include "player_number.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float gap       = 6.0F;
        constexpr float frame     = 3.0F;
        constexpr float bar_pitch = 2.0F;
        constexpr float bar_width = 1.5F;
        /// The pitch is never so far down that a player stands still; a guard against dividing by 0
        constexpr double slowest = 0.01;
        /// The button that turns the lanes
        constexpr float button_size = 34.0F;

        /// The bands, drawn over each other from the bass up: red bass, green mids, blue highs; the part played
        /// darker
        constexpr std::array<Color, 3> coming_colors{Color{235, 45, 40}, Color{60, 205, 70}, Color{70, 130, 255}};
        constexpr std::array<Color, 3> played_colors{Color{120, 30, 28}, Color{35, 105, 40}, Color{40, 70, 135}};
        constexpr Color                playhead_color{255, 255, 255};
        constexpr Color                cue_color{255, 165, 0};
    } // namespace

    BeatView::BeatView(const Translator &translator, Sides sides, double peaks_per_second)
        : translator{&translator}, sides{std::move(sides)}, peaks_per_second{peaks_per_second}
    {
    }

    void BeatView::layout(const Rect &area)
    {
        this->area = area;
        layoutLanes();
    }

    void BeatView::setVertical(bool on)
    {
        is_vertical = on;
        layoutLanes();
    }

    void BeatView::addTo(PointerRouter &router)
    {
        router.add(turn_button);
    }

    void BeatView::layoutLanes()
    {
        const Rect inner = area.inset(gap);
        // In the upper right corner, over the end of the first lane (its name is at the other end)
        const float button = std::min({button_size, inner.width, inner.height});
        turn_button.setBounds({inner.x + inner.width - button, inner.y, button, button});
        if (is_vertical)
        {
            const float half = std::max(0.0F, (inner.width - gap) / 2);
            lane_a           = {inner.x, inner.y, half, inner.height};
            lane_b           = {inner.x + half + gap, inner.y, half, inner.height};
            return;
        }
        const float half = std::max(0.0F, (inner.height - gap) / 2);
        lane_a           = {inner.x, inner.y, inner.width, half};
        lane_b           = {inner.x, inner.y + half + gap, inner.width, half};
    }

    void BeatView::update()
    {
        if (turn_button.takePress())
        {
            setVertical(!is_vertical);
        }
        // The picture shows how the lanes will stand after a press
        turn_button.setIcon(is_vertical ? TextButton::Icon::LanesHorizontal : TextButton::Icon::LanesVertical);
        turn_button.setTip(translator->tr(is_vertical ? "beats.turn_horizontal" : "beats.turn_vertical"));
        sides_shown = sides ? sides() : BeatSides{};
    }

    void BeatView::draw(Canvas &canvas) const
    {
        drawLane(canvas, lane_a, 'A', sides_shown.a);
        drawLane(canvas, lane_b, 'B', sides_shown.b);
        turn_button.draw(canvas);
    }

    void BeatView::drawLane(Canvas &canvas, const Rect &lane, char side, const std::optional<BeatDeck> &deck) const
    {
        if (lane.width <= 2 * frame || lane.height <= 2 * frame)
        {
            return;
        }
        look::sunken(canvas, lane);
        const Rect  inner  = lane.inset(frame);
        const float length = is_vertical ? inner.height : inner.width; // along the time
        const float depth  = is_vertical ? inner.width : inner.height; // across it
        const float middle = length / 2;
        // A piece of the lane: from along to along + extent in time, from across to across + thickness across it
        const auto piece = [this, &inner](float along, float extent, float across, float thickness)
        {
            return is_vertical ? Rect{inner.x + across, inner.y + along, thickness, extent}
                               : Rect{inner.x + along, inner.y + across, extent, thickness};
        };

        if (deck && deck->state.track_loaded && deck->peaks)
        {
            const core::api::TrackPeaks &peaks = *deck->peaks;
            const double                 speed = std::max(slowest, 1.0 + (deck->state.pitch_percent / 100.0));
            const double                 seconds_per_point = shown_seconds / static_cast<double>(length);
            const double                 position          = deck->state.position_seconds;
            const auto                   bars              = static_cast<std::size_t>(length / bar_pitch);
            // The value at a point along the lane; neighbouring bars share their border, so no value is drawn twice
            const auto value_at = [&](float along)
            {
                const double seconds = position + (static_cast<double>(along - middle) * seconds_per_point * speed);
                return std::floor(seconds * peaks_per_second);
            };
            for (std::size_t bar = 0; bar < bars; ++bar)
            {
                // The values this bar stands for, and the loudest of each band
                const float  along = static_cast<float>(bar) * bar_pitch;
                const double first = value_at(along);
                const double end   = value_at(along + bar_pitch);
                if (end <= 0.0 || first >= static_cast<double>(peaks.size()))
                {
                    continue; // before the start of the track or after its end
                }
                const auto                  from = static_cast<std::size_t>(std::max(0.0, first));
                const auto                  to   = std::max(from + 1, static_cast<std::size_t>(end));
                std::array<std::uint8_t, 3> loud{};
                for (std::size_t index = from; index < to; ++index)
                {
                    const core::api::TrackPeaks::Bands bands = peaks.at(index);
                    loud = {std::max(loud[0], bands.low), std::max(loud[1], bands.mid), std::max(loud[2], bands.high)};
                }
                const auto &colors = along < middle ? played_colors : coming_colors;
                for (std::size_t band = 0; band < loud.size(); ++band)
                {
                    if (loud[band] == 0)
                    {
                        continue;
                    }
                    const float size = std::max(1.0F, static_cast<float>(loud[band]) / 255.0F * depth);
                    canvas.fillRect(piece(along, bar_width, (depth - size) / 2, size), colors[band]);
                }
            }
            const double cue_along = middle + ((deck->state.cue_seconds - position) / speed / seconds_per_point);
            if (cue_along >= 0.0 && cue_along <= static_cast<double>(length))
            {
                canvas.fillRect(piece(static_cast<float>(cue_along) - 1.0F, 2.0F, 0.0F, depth), cue_color);
            }
        }
        canvas.fillRect(piece(middle - 1.0F, 2.0F, 0.0F, depth), playhead_color);

        std::string label{side};
        label += "  ";
        label += deck ? translator->tr("mixer.player", {{"number", playerNumber(deck->number)}})
                      : translator->tr("mixer.none");
        // Not under the turn button where it lies over this lane
        const Rect &button = turn_button.bounds();
        const bool  under  = button.x < inner.x + inner.width && button.x + button.width > inner.x &&
                             button.y < inner.y + inner.height && button.y + button.height > inner.y;
        canvas.textElided({inner.x + frame, inner.y + frame}, label, look::colors().text_dim,
                          inner.width - (2 * frame) - (under ? button.width : 0.0F));
    }
} // namespace djw::player_ui::sdl
