// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP
#define DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP

#include "api/library.hpp"
#include "clock.hpp"
#include "column_header.hpp"
#include "text_field.hpp"
#include "track_list.hpp"
#include "view.hpp"

#include <array>
#include <functional>
#include <string>

namespace djw::player_ui::sdl
{
    /**
     * The library window: a search field over all fields, filter fields for artist, title, genre and year right
     * above their columns, the column header (tap to sort, tap again to reverse), the matching tracks and a status
     * line with the track count and the scan progress.
     *
     * Searches as the user types. While a scan runs the list is refreshed once per refresh_interval, so new tracks
     * show up, and once more when the scan ends. A double tap on a track hands it to load. Typed text goes to the
     * field tapped last.
     */
    class LibraryView final : public View
    {
        public:
            using Load = std::function<void(const core::api::LibraryTrack &track)>;

            /// Most tracks listed at once; the search narrows it down
            static constexpr std::size_t               result_limit = 1'000;
            static constexpr std::chrono::milliseconds refresh_interval{1'000};

            /// The columns with a filter field, in this order
            static constexpr std::array<core::api::LibraryColumn, 4> filter_columns{
                core::api::LibraryColumn::Artist, core::api::LibraryColumn::Title, core::api::LibraryColumn::Genre,
                core::api::LibraryColumn::Year};

            LibraryView(core::api::Library &library, Load load, Clock clock = steadyClock());

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;
            void update() override;
            void draw(Canvas &canvas) const override;

            [[nodiscard]] bool wantsTextInput() const override;
            void               keyboard(const KeyboardInput &input) override;
            void               wheel(Point position, float dy) override;

            [[nodiscard]] const TextField &searchField() const noexcept { return search_field; }
            /// The filter field of filter_columns[index]
            [[nodiscard]] const TextField    &filterField(std::size_t index) const { return filters.at(index); }
            [[nodiscard]] const ColumnHeader &columnHeader() const noexcept { return header; }
            [[nodiscard]] const TrackList    &trackList() const noexcept { return list; }
            [[nodiscard]] const std::string  &statusText() const noexcept { return status_text; }
            /// The query of the tracks shown
            [[nodiscard]] const core::api::LibraryQuery &query() const noexcept { return searched; }

        private:
            /// The field tapped last takes the focus from all others
            void                                  resolveFocus();
            [[nodiscard]] core::api::LibraryQuery currentQuery() const;
            void                                  applySortTap();
            void                                  search(const core::api::LibraryQuery &query, bool keep_position);
            void                                  updateStatus(const core::api::LibraryScanStatus &status);

            core::api::Library                   *library;
            Load                                  load;
            Clock                                 clock;
            TextField                             search_field{"Search"};
            std::array<TextField, 4>              filters{TextField{"Artist"}, TextField{"Title"}, TextField{"Genre"},
                                                          TextField{"Year"}};
            ColumnHeader                          header;
            TrackList                             list;
            core::api::LibraryColumn              sort_by    = core::api::LibraryColumn::Artist;
            bool                                  descending = false;
            Rect                                  status_area;
            std::string                           status_text;
            std::string                           search_error;
            core::api::LibraryQuery               searched;
            bool                                  has_searched = false;
            bool                                  was_scanning = false;
            std::chrono::steady_clock::time_point last_search;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP
