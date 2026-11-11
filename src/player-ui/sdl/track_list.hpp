// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TRACK_LIST_HPP
#define DJW_PLAYER_UI_SDL_TRACK_LIST_HPP

#include "api/library.hpp"
#include "clock.hpp"
#include "keyboard_input.hpp"
#include "widget.hpp"

#include <chrono>
#include <cstddef>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * Scrollable list of tracks with the columns of track_columns; tracks whose file is missing show in red.
     *
     * Dragging scrolls (touch and mouse alike) and, let go while moving, the list runs on and slows down (kinetic
     * scrolling, see advance()); a touch, the wheel or the keys stop it. A tap selects a row, a second tap on the same
     * row within double_tap_time activates it, e.g. to load it into a deck. Holding a cell still for long_press_time
     * presses it long, e.g. to edit it; the release then is no tap. A drag never selects anything.
     */
    class TrackList final : public Widget
    {
        public:
            static constexpr float                     row_height = 44.0F; // a comfortable touch target
            static constexpr float                     tap_slop   = 8.0F;  // movement that still counts as tap
            static constexpr std::chrono::milliseconds double_tap_time{400};
            static constexpr std::chrono::milliseconds long_press_time{500};
            /// Kinetic scrolling: how fast it slows down (the speed falls to 1/e in this time), the slowest let-go
            /// speed that starts it and the speed at which it stops, in points per second
            static constexpr double fling_time_constant = 0.325;
            static constexpr double fling_min_speed     = 60.0;
            static constexpr double fling_stop_speed    = 15.0;
            /// The moves within this time before the finger lifts give the speed
            static constexpr std::chrono::milliseconds fling_sample_time{100};

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
            /// Moves the selection by rows (negative: up), clamped to the list, and scrolls so it is in view; without
            /// a selection it starts at the first row (or the last, going up). Nothing without rows.
            void moveSelection(std::ptrdiff_t by) noexcept;
            /// The rows that fit in the list at once, at least 1: how far a page goes
            [[nodiscard]] std::size_t pageRows() const noexcept;
            /// The cursor keys move the selection a row, Page Up and Page Down a page; true if key was one of them
            bool navigate(EditKey key) noexcept;
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
            /// Scrolls by dy points (positive: content moves down, towards the top of the list), clamped; stops a
            /// running fling
            void scrollBy(float dy) noexcept;
            /// Moves a fling on to where it is now; call once per frame
            void advance();
            /// The list runs on after a drag
            [[nodiscard]] bool flinging() const noexcept { return fling.has_value(); }

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            [[nodiscard]] float               maxOffset() const noexcept;
            [[nodiscard]] std::optional<Cell> cellAt(Point position) const noexcept;
            void                              tap(Point position);
            void                              startFling();

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
            /// The drag's last positions: when and at which offset, for the speed at let-go
            struct Sample
            {
                    std::chrono::steady_clock::time_point time;
                    float                                 offset = 0.0F;
            };
            std::vector<Sample> samples;
            /// A list running on after a drag: from where and how fast (points per second, positive towards the end)
            struct Fling
            {
                    std::chrono::steady_clock::time_point start;
                    float                                 from  = 0.0F;
                    double                                speed = 0.0;
            };
            std::optional<Fling> fling;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TRACK_LIST_HPP
