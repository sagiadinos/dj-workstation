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
#include <cstdint>
#include <filesystem>
#include <optional>
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
     * show up, and again whenever the library's revision changes (scan end, written tag edits). A double tap on a
     * track hands it to load. Typed text goes to the field tapped last.
     *
     * Tag editor: a long press on an artist, title, genre or year cell opens an editor in the cell. Enter or touching
     * anything else saves, Escape cancels. The change goes to the library, which writes the file in the background;
     * the row shows the new value at once. Tracks for which in_use is true (loaded in a deck) are not changed.
     */
    class LibraryView final : public View
    {
        public:
            using Load  = std::function<void(const core::api::LibraryTrack &track)>;
            using InUse = std::function<bool(const std::filesystem::path &file)>;

            /// Most tracks listed at once; the search narrows it down
            static constexpr std::size_t               result_limit = 1'000;
            static constexpr std::chrono::milliseconds refresh_interval{1'000};

            /// The columns with a filter field, in this order
            static constexpr std::array<core::api::LibraryColumn, 4> filter_columns{
                core::api::LibraryColumn::Artist, core::api::LibraryColumn::Title, core::api::LibraryColumn::Genre,
                core::api::LibraryColumn::Year};

            LibraryView(core::api::Library &library, Load load, InUse in_use = {}, Clock clock = steadyClock());

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
            [[nodiscard]] bool                           editing() const noexcept { return edit.has_value(); }
            [[nodiscard]] const TextField               &cellEditor() const noexcept { return cell_editor; }

        private:
            /// The cell being edited; the track is kept by id, as a refresh may move its row
            struct CellEdit
            {
                    std::int64_t             track_id = 0;
                    core::api::LibraryColumn column{};
                    std::string              original;
            };

            /// The field tapped last takes the focus from all others
            void                                     resolveFocus();
            void                                     startEdit(const TrackList::Cell &cell);
            void                                     commitEdit();
            void                                     cancelEdit();
            void                                     placeEditor();
            [[nodiscard]] std::optional<std::size_t> rowOf(std::int64_t track_id) const;
            [[nodiscard]] core::api::LibraryQuery    currentQuery() const;
            void                                     applySortTap();
            void                                     search(const core::api::LibraryQuery &query, bool keep_position);
            void                                     updateStatus(const core::api::LibraryStatus &status);

            core::api::Library                   *library;
            Load                                  load;
            InUse                                 in_use;
            Clock                                 clock;
            TextField                             search_field{"Search"};
            std::array<TextField, 4>              filters{TextField{"Artist"}, TextField{"Title"}, TextField{"Genre"},
                                                          TextField{"Year"}};
            ColumnHeader                          header;
            TrackList                             list;
            TextField                             cell_editor;
            std::optional<CellEdit>               edit;
            std::string                           edit_message;
            core::api::LibraryColumn              sort_by    = core::api::LibraryColumn::Artist;
            bool                                  descending = false;
            Rect                                  status_area;
            std::string                           status_text;
            std::string                           search_error;
            core::api::LibraryQuery               searched;
            bool                                  has_searched  = false;
            std::uint64_t                         seen_revision = 0;
            std::chrono::steady_clock::time_point last_search;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP
