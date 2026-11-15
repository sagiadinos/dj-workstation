// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_BEAT_VIEW_HPP
#define DJW_PLAYER_UI_SDL_BEAT_VIEW_HPP

#include "api/snapshot.hpp"
#include "api/track_peaks.hpp"
#include "i18n/translator.hpp"
#include "view.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace djw::player_ui::sdl
{
    /// A player as the beat waveforms show it
    struct BeatDeck
    {
            /// Shown as "Player 01"
            int number = 0;
            /// The player's state as it read it last
            core::api::DeckSnapshot state{};
            /// The fine waveform of its track; nullptr for none (no track, or a streamed one)
            std::shared_ptr<const core::api::TrackPeaks> peaks{};
    };

    /// The players on the two sides of the crossfader; std::nullopt for a side without one
    struct BeatSides
    {
            std::optional<BeatDeck> a;
            std::optional<BeatDeck> b;
    };

    /**
     * The beat waveforms: the tracks of the players on crossfader sides A and B as waveforms that scroll past a fixed
     * playhead in the middle, for beatmatching by eye. Each side has a lane; horizontal, the lanes lie one above the
     * other (A on top) and time runs to the right; vertical, they stand side by side (A on the left) and time runs
     * down. A lane shows shown_seconds of real time: a player pitched up runs through more of its track in that time,
     * so two tracks at the same tempo show their beats the same distance apart. The part played is drawn darker,
     * the cue point as a red line.
     *
     * Which players are on the sides is asked for once per frame; the view sends nothing.
     */
    class BeatView final : public View
    {
        public:
            using Sides = std::function<BeatSides()>;

            /// The size the part opens at
            static constexpr float design_width  = 760.0F;
            static constexpr float design_height = 200.0F;
            /// The real time a lane shows, as much before the playhead as after it
            static constexpr double shown_seconds = 6.0;

            /// peaks_per_second: the values of a TrackPeaks per second of the track
            BeatView(const Translator &translator, Sides sides, double peaks_per_second);

            void layout(const Rect &area) override;
            void addTo(PointerRouter & /*router*/) override {}
            void update() override;
            void draw(Canvas &canvas) const override;

            /// Vertical lanes (time runs down) instead of horizontal ones; kept with the layout
            void               setVertical(bool on) noexcept;
            [[nodiscard]] bool vertical() const noexcept { return is_vertical; }

            /// The lane of side A and of side B; for tests
            [[nodiscard]] const Rect      &laneA() const noexcept { return lane_a; }
            [[nodiscard]] const Rect      &laneB() const noexcept { return lane_b; }
            [[nodiscard]] const BeatSides &shown() const noexcept { return sides_shown; }

        private:
            void layoutLanes();
            void drawLane(Canvas &canvas, const Rect &lane, char side, const std::optional<BeatDeck> &deck) const;

            const Translator *translator;
            Sides             sides;
            double            peaks_per_second;
            BeatSides         sides_shown;
            Rect              area;
            Rect              lane_a;
            Rect              lane_b;
            bool              is_vertical = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_BEAT_VIEW_HPP
