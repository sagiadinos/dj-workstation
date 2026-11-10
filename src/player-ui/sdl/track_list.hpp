// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TRACK_LIST_HPP
#define DJW_PLAYER_UI_SDL_TRACK_LIST_HPP

#include "api/library.hpp"
#include "clock.hpp"
#include "widget.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * Scrollable list of tracks with the columns of track_columns.
     *
     * Dragging scrolls (touch and mouse alike); a tap selects a row, a second tap on the same row within
     * double_tap_time activates it, e.g. to load it into a deck. A drag never selects anything.
     */
    class TrackList final : public Widget
    {
        public:
            static constexpr float                     row_height = 44.0F; // a comfortable touch target
            static constexpr float                     tap_slop   = 8.0F;  // movement that still counts as tap
            static constexpr std::chrono::milliseconds double_tap_time{400};

            explicit TrackList(Clock clock = steadyClock()) : clock{std::move(clock)} {}

            /// Replaces the rows; scrolls to the top and clears the selection unless keep_position
            void setTracks(std::vector<core::api::LibraryTrack> tracks, bool keep_position = false);
            [[nodiscard]] const std::vector<core::api::LibraryTrack> &tracks() const noexcept { return rows; }

            [[nodiscard]] std::optional<std::size_t> selected() const noexcept { return selection; }
            /// The row activated by a double tap since the last call, if any
            [[nodiscard]] std::optional<std::size_t> takeActivated() noexcept;

            [[nodiscard]] float scrollOffset() const noexcept { return offset; }
            /// Scrolls by dy points (positive: content moves down, towards the top of the list), clamped
            void scrollBy(float dy) noexcept;

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            [[nodiscard]] float                      maxOffset() const noexcept;
            [[nodiscard]] std::optional<std::size_t> rowAt(Point position) const noexcept;
            void                                     tap(Point position);

            Clock                                 clock;
            std::vector<core::api::LibraryTrack>  rows;
            float                                 offset = 0.0F;
            std::optional<std::size_t>            selection;
            std::optional<std::size_t>            activated;
            std::chrono::steady_clock::time_point last_tap_time;
            std::optional<std::size_t>            last_tap_row;
            // Current pointer
            Point start;
            float start_offset = 0.0F;
            bool  dragging     = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TRACK_LIST_HPP
