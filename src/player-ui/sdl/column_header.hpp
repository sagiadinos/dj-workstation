// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_COLUMN_HEADER_HPP
#define DJW_PLAYER_UI_SDL_COLUMN_HEADER_HPP

#include "api/library.hpp"
#include "widget.hpp"

#include <optional>

namespace djw::player_ui::sdl
{
    /// The column names above the track list; tapping a name asks to sort by that column. Shows the current sort.
    class ColumnHeader final : public Widget
    {
        public:
            void setSort(core::api::LibraryColumn column, bool descending) noexcept;

            /// The column tapped since the last call, if any
            [[nodiscard]] std::optional<core::api::LibraryColumn> takeTapped() noexcept;

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            [[nodiscard]] std::optional<core::api::LibraryColumn> columnAt(Point position) const noexcept;

            core::api::LibraryColumn                sort_by    = core::api::LibraryColumn::Artist;
            bool                                    descending = false;
            std::optional<core::api::LibraryColumn> pressed;
            std::optional<core::api::LibraryColumn> tapped;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_COLUMN_HEADER_HPP
