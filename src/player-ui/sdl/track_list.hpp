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
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * Scrollable list of tracks with the columns of track_columns; tracks whose file is missing show in red.
     *
     * Dragging scrolls (touch and mouse alike); a tap selects a row, a second tap on the same row within
     * double_tap_time activates it, e.g. to load it into a deck. Holding a cell still for long_press_time presses it
     * long, e.g. to edit it; the release then is no tap. A drag never selects anything.
     */
    class TrackList final : public Widget
    {
        public:
            static constexpr float                     row_height = 44.0F; // a comfortable touch target
            static constexpr float                     tap_slop   = 8.0F;  // movement that still counts as tap
            static constexpr std::chrono::milliseconds double_tap_time{400};
            static constexpr std::chrono::milliseconds long_press_time{500};

            /// A cell of the list
            struct Cell
            {
                    std::size_t              row = 0;
                    core::api::LibraryColumn column{};

                    bool operator==(const Cell &) const = default;
            };

            explicit TrackList(Clock clock = steadyClock()) : clock{std::move(clock)} {}

            /// Replaces the rows; scrolls to the top and clears the selection unless keep_position
            void setTracks(std::vector<core::api::LibraryTrack> tracks, bool keep_position = false);
            [[nodiscard]] const std::vector<core::api::LibraryTrack> &tracks() const noexcept { return rows; }

            [[nodiscard]] std::optional<std::size_t> selected() const noexcept { return selection; }
            /// Selects a row; ignored for a row that does not exist
            void select(std::size_t row) noexcept
            {
                if (row < rows.size())
                {
                    selection = row;
                }
            }
            /// The row activated by a double tap since the last call, if any
            [[nodiscard]] std::optional<std::size_t> takeActivated() noexcept;
            /// The cell held for long_press_time, once per press; call once per frame, as nothing else checks the time
            [[nodiscard]] std::optional<Cell> takeLongPress();
            /// True once after the list was touched, e.g. to end an edit elsewhere
            [[nodiscard]] bool takeTouched() noexcept { return std::exchange(touched, false); }

            /// The row under position, std::nullopt outside the rows
            [[nodiscard]] std::optional<std::size_t> rowAt(Point position) const noexcept;

            /// Replaces one row, e.g. with edited tags; ignored for a row that does not exist
            void replaceTrack(std::size_t row, core::api::LibraryTrack track);
            /// Where a cell is on screen, std::nullopt when it is scrolled out of view
            [[nodiscard]] std::optional<Rect> cellArea(const Cell &cell) const;

            [[nodiscard]] float scrollOffset() const noexcept { return offset; }
            /// Scrolls by dy points (positive: content moves down, towards the top of the list), clamped
            void scrollBy(float dy) noexcept;

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            [[nodiscard]] float                      maxOffset() const noexcept;
            [[nodiscard]] std::optional<Cell>        cellAt(Point position) const noexcept;
            void                                     tap(Point position);

            Clock                                 clock;
            std::vector<core::api::LibraryTrack>  rows;
            float                                 offset = 0.0F;
            std::optional<std::size_t>            selection;
            std::optional<std::size_t>            activated;
            std::chrono::steady_clock::time_point last_tap_time;
            std::optional<std::size_t>            last_tap_row;
            bool                                  touched = false;
            // Current pointer
            Point                                 start;
            std::chrono::steady_clock::time_point down_time;
            float                                 start_offset = 0.0F;
            bool                                  held         = false;
            bool                                  dragging     = false;
            bool                                  long_pressed = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TRACK_LIST_HPP
