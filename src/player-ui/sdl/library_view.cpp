// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library_view.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin        = 12.0F;
        constexpr float gap           = 8.0F;
        constexpr float search_height = 48.0F;
        constexpr float status_height = 28.0F;

        constexpr Color status_color{160, 160, 160};
    } // namespace

    LibraryView::LibraryView(core::api::Library &library, Load load, Clock clock)
        : library{&library}, load{std::move(load)}, clock{std::move(clock)}, list{this->clock}
    {
    }

    void LibraryView::layout(const Rect &area)
    {
        const Rect inner = area.inset(margin);
        search_field.setBounds({inner.x, inner.y, inner.width, std::min(search_height, inner.height)});
        status_area = {inner.x, std::max(inner.y, inner.y + inner.height - status_height), inner.width,
                       std::min(status_height, inner.height)};

        const float list_top = inner.y + search_height + gap;
        list.setBounds({inner.x, list_top, inner.width, std::max(0.0F, status_area.y - gap - list_top)});
        list.scrollBy(0.0F); // keeps the scroll position valid for the new height
    }

    void LibraryView::addTo(PointerRouter &router)
    {
        router.add(search_field);
        router.add(list);
    }

    void LibraryView::update()
    {
        const core::api::LibraryScanStatus status = library->scanStatus();

        const bool text_changed = !searched || search_field.text() != searched_text;
        const bool scan_ended   = was_scanning && !status.running;
        const bool scan_refresh = status.running && clock() - last_search >= refresh_interval;
        if (text_changed || scan_ended || scan_refresh)
        {
            search(!text_changed);
        }
        was_scanning = status.running;
        updateStatus(status);

        if (const auto row = list.takeActivated(); row && load)
        {
            load(list.tracks()[*row]);
        }
    }

    void LibraryView::search(bool keep_position)
    {
        searched_text = search_field.text();
        searched      = true;
        last_search   = clock();
        auto found    = library->search(searched_text, result_limit);
        if (found)
        {
            search_error.clear();
            list.setTracks(std::move(*found), keep_position);
        }
        else
        {
            search_error = found.error();
        }
    }

    void LibraryView::updateStatus(const core::api::LibraryScanStatus &status)
    {
        status_text = std::to_string(status.tracks_total) + " tracks";
        if (status.running)
        {
            status_text += " - scanning, " + std::to_string(status.files_found) + " files";
        }
        if (!search_error.empty())
        {
            status_text += " - " + search_error;
        }
        else if (!status.error.empty())
        {
            status_text += " - " + status.error;
        }
    }

    void LibraryView::draw(Canvas &canvas) const
    {
        search_field.draw(canvas);
        list.draw(canvas);
        canvas.textElided({status_area.x, status_area.y + ((status_area.height - canvas.textHeight()) / 2)},
                          status_text, status_color, status_area.width);
    }

    void LibraryView::keyboard(const KeyboardInput &input)
    {
        static_cast<void>(search_field.input(input));
    }

    void LibraryView::wheel(Point position, float dy)
    {
        if (list.bounds().contains(position))
        {
            list.scrollBy(dy * TrackList::row_height);
        }
    }
} // namespace djw::player_ui::sdl
