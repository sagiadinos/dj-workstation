// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP
#define DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP

#include "api/library.hpp"
#include "clock.hpp"
#include "text_field.hpp"
#include "track_list.hpp"
#include "view.hpp"

#include <functional>
#include <string>

namespace djw::player_ui::sdl
{
    /**
     * The library window: a search field, the matching tracks and a status line with the track count and the scan
     * progress.
     *
     * Searches as the user types. While a scan runs the list is refreshed once per refresh_interval, so new tracks
     * show up, and once more when the scan ends. A double tap on a track hands it to load.
     */
    class LibraryView final : public View
    {
        public:
            using Load = std::function<void(const core::api::LibraryTrack &track)>;

            /// Most tracks listed at once; the search narrows it down
            static constexpr std::size_t               result_limit = 1'000;
            static constexpr std::chrono::milliseconds refresh_interval{1'000};

            LibraryView(core::api::Library &library, Load load, Clock clock = steadyClock());

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;
            void update() override;
            void draw(Canvas &canvas) const override;

            [[nodiscard]] bool wantsTextInput() const override { return search_field.focused(); }
            void               keyboard(const KeyboardInput &input) override;
            void               wheel(Point position, float dy) override;

            [[nodiscard]] const TextField   &searchField() const noexcept { return search_field; }
            [[nodiscard]] const TrackList   &trackList() const noexcept { return list; }
            [[nodiscard]] const std::string &statusText() const noexcept { return status_text; }

        private:
            void search(bool keep_position);
            void updateStatus(const core::api::LibraryScanStatus &status);

            core::api::Library                   *library;
            Load                                  load;
            Clock                                 clock;
            TextField                             search_field{"Search"};
            TrackList                             list;
            Rect                                  status_area;
            std::string                           status_text;
            std::string                           search_error;
            std::string                           searched_text;
            bool                                  searched     = false;
            bool                                  was_scanning = false;
            std::chrono::steady_clock::time_point last_search;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP
