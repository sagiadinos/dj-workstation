// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library_view.hpp"

#include "track_columns.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin        = 12.0F;
        constexpr float gap           = 8.0F;
        constexpr float search_height = 48.0F;
        constexpr float filter_height = 40.0F;
        constexpr float header_height = 30.0F;
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

        const Rect filter_row{inner.x, inner.y + search_height + gap, inner.width, filter_height};
        for (std::size_t index = 0; index < filters.size(); ++index)
        {
            // A little apart, so the fields read as separate fields
            const Rect column = columnArea(filter_row, filter_columns.at(index));
            filters.at(index).setBounds({column.x, column.y, std::max(0.0F, column.width - 4.0F), column.height});
        }

        const float header_top = filter_row.y + filter_height + 4.0F;
        header.setBounds({inner.x, header_top, inner.width, header_height});

        status_area          = {inner.x, std::max(inner.y, inner.y + inner.height - status_height), inner.width,
                                std::min(status_height, inner.height)};
        const float list_top = header_top + header_height;
        list.setBounds({inner.x, list_top, inner.width, std::max(0.0F, status_area.y - gap - list_top)});
        list.scrollBy(0.0F); // keeps the scroll position valid for the new height
    }

    void LibraryView::addTo(PointerRouter &router)
    {
        router.add(search_field);
        for (TextField &filter : filters)
        {
            router.add(filter);
        }
        router.add(header);
        router.add(list);
    }

    void LibraryView::resolveFocus()
    {
        const TextField *requested = search_field.takeFocusRequest() ? &search_field : nullptr;
        for (TextField &filter : filters)
        {
            if (filter.takeFocusRequest())
            {
                requested = &filter;
            }
        }
        if (requested == nullptr)
        {
            return;
        }
        search_field.setFocused(requested == &search_field);
        for (TextField &filter : filters)
        {
            filter.setFocused(requested == &filter);
        }
    }

    bool LibraryView::wantsTextInput() const
    {
        return search_field.focused() ||
               std::ranges::any_of(filters, [](const TextField &filter) { return filter.focused(); });
    }

    void LibraryView::keyboard(const KeyboardInput &input)
    {
        resolveFocus();
        static_cast<void>(search_field.input(input));
        for (TextField &filter : filters)
        {
            static_cast<void>(filter.input(input));
        }
    }

    void LibraryView::applySortTap()
    {
        const auto tapped = header.takeTapped();
        if (!tapped)
        {
            return;
        }
        descending = *tapped == sort_by && !descending;
        sort_by    = *tapped;
        header.setSort(sort_by, descending);
    }

    core::api::LibraryQuery LibraryView::currentQuery() const
    {
        return {
            .text       = search_field.text(),
            .artist     = filters[0].text(),
            .title      = filters[1].text(),
            .genre      = filters[2].text(),
            .year       = filters[3].text(),
            .sort_by    = sort_by,
            .descending = descending,
            .limit      = result_limit,
        };
    }

    void LibraryView::update()
    {
        resolveFocus();
        applySortTap();
        const core::api::LibraryScanStatus status = library->scanStatus();

        const core::api::LibraryQuery query         = currentQuery();
        const bool                    query_changed = !has_searched || query != searched;
        const bool                    scan_ended    = was_scanning && !status.running;
        const bool                    scan_refresh  = status.running && clock() - last_search >= refresh_interval;
        if (query_changed || scan_ended || scan_refresh)
        {
            search(query, !query_changed);
        }
        was_scanning = status.running;
        updateStatus(status);

        if (const auto row = list.takeActivated(); row && load)
        {
            load(list.tracks()[*row]);
        }
    }

    void LibraryView::search(const core::api::LibraryQuery &query, bool keep_position)
    {
        searched     = query;
        has_searched = true;
        last_search  = clock();
        auto found   = library->search(query);
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
        for (const TextField &filter : filters)
        {
            filter.draw(canvas);
        }
        header.draw(canvas);
        list.draw(canvas);
        canvas.textElided({status_area.x, status_area.y + ((status_area.height - canvas.textHeight()) / 2)},
                          status_text, status_color, status_area.width);
    }

    void LibraryView::wheel(Point position, float dy)
    {
        if (list.bounds().contains(position))
        {
            list.scrollBy(dy * TrackList::row_height);
        }
    }
} // namespace djw::player_ui::sdl
