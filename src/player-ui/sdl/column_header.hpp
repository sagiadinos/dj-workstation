// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_COLUMN_HEADER_HPP
#define DJW_PLAYER_UI_SDL_COLUMN_HEADER_HPP

#include "api/library.hpp"
#include "track_columns.hpp"
#include "widget.hpp"

#include <array>
#include <optional>
#include <string>

namespace djw::player_ui::sdl
{
    /// The column names above the track list; tapping a name asks to sort by that column. Shows the current sort.
    class ColumnHeader final : public Widget
    {
        public:
            ColumnHeader();

            void setSort(core::api::LibraryColumn column, bool descending) noexcept;
            /// The names shown, in the order of track_columns; the English ones until set
            void setNames(std::array<std::string, track_columns.size()> new_names) { names = std::move(new_names); }

            /// The column tapped since the last call, if any
            [[nodiscard]] std::optional<core::api::LibraryColumn> takeTapped() noexcept;

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            [[nodiscard]] std::optional<core::api::LibraryColumn> columnAt(Point position) const noexcept;

            std::array<std::string, track_columns.size()> names;
            core::api::LibraryColumn                      sort_by    = core::api::LibraryColumn::Artist;
            bool                                          descending = false;
            std::optional<core::api::LibraryColumn>       pressed;
            std::optional<core::api::LibraryColumn>       tapped;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_COLUMN_HEADER_HPP
