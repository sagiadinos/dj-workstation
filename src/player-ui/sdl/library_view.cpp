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

        constexpr float missing_button_width = 150.0F;
        constexpr float remove_button_width  = 220.0F;
        constexpr float add_button_width     = 170.0F;

        /// The value of a column as text, as the editor shows it; std::nullopt for columns that cannot be edited
        std::optional<std::string> editableValue(const core::api::LibraryTrack &track, core::api::LibraryColumn column)
        {
            switch (column)
            {
                case core::api::LibraryColumn::Artist:
                    return track.artist;
                case core::api::LibraryColumn::Title:
                    return track.title;
                case core::api::LibraryColumn::Genre:
                    return track.genre;
                case core::api::LibraryColumn::Year:
                    return track.year != 0 ? std::to_string(track.year) : std::string{};
                case core::api::LibraryColumn::Duration:
                    break;
            }
            return std::nullopt;
        }

        /// A year as typed: empty is 0 (no year), otherwise up to four digits
        std::optional<int> parseYear(std::string_view text)
        {
            constexpr std::size_t max_digits = 4;
            if (text.size() > max_digits)
            {
                return std::nullopt;
            }
            int year = 0;
            for (const char digit : text)
            {
                if (digit < '0' || digit > '9')
                {
                    return std::nullopt;
                }
                year = (year * 10) + (digit - '0');
            }
            return year; // empty: 0, no year
        }
    } // namespace

    LibraryView::LibraryView(core::api::Library &library, Load load, InUse in_use, Clock clock)
        : library{&library}, load{std::move(load)}, in_use{std::move(in_use)}, clock{std::move(clock)},
          list{this->clock}
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
        placeEditor();
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
        router.add(missing_button);
        router.add(remove_button);
        router.add(add_button);
        router.add(cell_editor); // on top of the list; without an edit it has no area and catches nothing
    }

    void LibraryView::resolveFocus()
    {
        const std::array<TextField *, 6> fields{&search_field, &filters[0], &filters[1],
                                                &filters[2],   &filters[3], &cell_editor};
        const TextField                 *requested = nullptr;
        for (TextField *field : fields)
        {
            if (field->takeFocusRequest())
            {
                requested = field;
            }
        }
        if (requested == nullptr)
        {
            return;
        }
        for (TextField *field : fields)
        {
            field->setFocused(field == requested);
        }
        if (edit && requested != &cell_editor)
        {
            commitEdit(); // another field took over
        }
    }

    bool LibraryView::wantsTextInput() const
    {
        return search_field.focused() || cell_editor.focused() ||
               std::ranges::any_of(filters, [](const TextField &filter) { return filter.focused(); });
    }

    void LibraryView::keyboard(const KeyboardInput &input)
    {
        resolveFocus();
        if (edit && cell_editor.focused())
        {
            if (const auto *key = std::get_if<EditKey>(&input); key != nullptr && *key == EditKey::Enter)
            {
                commitEdit();
                return;
            }
            if (const auto *key = std::get_if<EditKey>(&input); key != nullptr && *key == EditKey::Escape)
            {
                cancelEdit();
                return;
            }
            static_cast<void>(cell_editor.input(input));
            return;
        }
        static_cast<void>(search_field.input(input));
        for (TextField &filter : filters)
        {
            static_cast<void>(filter.input(input));
        }
    }

    std::optional<std::size_t> LibraryView::rowOf(std::int64_t track_id) const
    {
        const auto &rows  = list.tracks();
        const auto  found = std::ranges::find(rows, track_id, &core::api::LibraryTrack::id);
        return found != rows.end() ? std::optional{static_cast<std::size_t>(found - rows.begin())} : std::nullopt;
    }

    void LibraryView::startEdit(const TrackList::Cell &cell)
    {
        const core::api::LibraryTrack &track = list.tracks()[cell.row];
        const auto                     value = editableValue(track, cell.column);
        if (!value)
        {
            return;
        }
        if (in_use && in_use(track.path))
        {
            edit_message = "the track is loaded in a deck, its tags are not changed while it plays";
            return;
        }
        edit_message.clear();
        edit = CellEdit{.track_id = track.id, .column = cell.column, .original = *value};
        cell_editor.setText(*value);
        search_field.setFocused(false);
        for (TextField &filter : filters)
        {
            filter.setFocused(false);
        }
        cell_editor.setFocused(true);
        placeEditor();
    }

    void LibraryView::cancelEdit()
    {
        edit.reset();
        cell_editor.setFocused(false);
        cell_editor.setBounds({});
    }

    void LibraryView::commitEdit()
    {
        if (!edit)
        {
            return;
        }
        const CellEdit    done  = *edit;
        const std::string value = cell_editor.text();
        cancelEdit();
        if (value == done.original)
        {
            return;
        }

        core::api::LibraryTagChange change;
        switch (done.column)
        {
            case core::api::LibraryColumn::Artist:
                change.artist = value;
                break;
            case core::api::LibraryColumn::Title:
                change.title = value;
                break;
            case core::api::LibraryColumn::Genre:
                change.genre = value;
                break;
            case core::api::LibraryColumn::Year:
                change.year = parseYear(value);
                if (!change.year)
                {
                    edit_message = "the year must be a number of up to four digits";
                    return;
                }
                break;
            case core::api::LibraryColumn::Duration:
                return;
        }

        // Shown at once; the library writes the file in the background and the next refresh confirms it
        if (const auto row = rowOf(done.track_id))
        {
            core::api::LibraryTrack track = list.tracks()[*row];
            track.artist                  = change.artist.value_or(track.artist);
            track.title                   = change.title.value_or(track.title);
            track.genre                   = change.genre.value_or(track.genre);
            track.year                    = change.year.value_or(track.year);
            list.replaceTrack(*row, std::move(track));
        }
        edit_message.clear();
        library->updateTags(done.track_id, std::move(change));
    }

    void LibraryView::placeEditor()
    {
        if (!edit)
        {
            return;
        }
        const auto row  = rowOf(edit->track_id);
        const auto area = row ? list.cellArea({*row, edit->column}) : std::nullopt;
        if (!area)
        {
            commitEdit(); // scrolled away or gone from the list
            return;
        }
        cell_editor.setBounds(*area);
    }

    void LibraryView::applySortTap()
    {
        const auto tapped = header.takeTapped();
        if (!tapped)
        {
            return;
        }
        commitEdit();
        descending = *tapped == sort_by && !descending;
        sort_by    = *tapped;
        header.setSort(sort_by, descending);
    }

    core::api::LibraryQuery LibraryView::currentQuery() const
    {
        return {
            .text         = search_field.text(),
            .artist       = filters[0].text(),
            .title        = filters[1].text(),
            .genre        = filters[2].text(),
            .year         = filters[3].text(),
            .only_missing = only_missing,
            .sort_by      = sort_by,
            .descending   = descending,
            .limit        = result_limit,
        };
    }

    void LibraryView::update()
    {
        resolveFocus();
        if (list.takeTouched() && edit)
        {
            commitEdit();
        }
        if (const auto cell = list.takeLongPress())
        {
            startEdit(*cell);
        }
        applySortTap();
        const core::api::LibraryStatus status = library->status();
        updateMissingButtons(status);

        const core::api::LibraryQuery query            = currentQuery();
        const bool                    query_changed    = !has_searched || query != searched;
        const bool                    revision_changed = status.revision != seen_revision;
        const bool                    scan_refresh     = status.scanning && clock() - last_search >= refresh_interval;
        if (query_changed || revision_changed || scan_refresh)
        {
            search(query, !query_changed);
        }
        seen_revision = status.revision;
        updateStatus(status);
        placeEditor();

        if (const auto row = list.takeActivated(); row && load)
        {
            load(list.tracks()[*row]);
        }
    }

    void LibraryView::updateMissingButtons(const core::api::LibraryStatus &status)
    {
        if (missing_button.takePress())
        {
            only_missing = !only_missing;
            confirm_remove_until.reset();
        }
        if (remove_button.takePress())
        {
            if (confirm_remove_until && clock() <= *confirm_remove_until)
            {
                library->removeMissing();
                confirm_remove_until.reset();
                only_missing = false;
            }
            else
            {
                confirm_remove_until = clock() + confirm_time;
            }
        }
        if (confirm_remove_until && clock() > *confirm_remove_until)
        {
            confirm_remove_until.reset();
        }
        if (status.missing_total == 0)
        {
            only_missing = false; // nothing left to show there
            confirm_remove_until.reset();
        }

        const auto selected = list.selected();
        if (add_button.takePress() && selected && add_to_playlist)
        {
            add_to_playlist(list.tracks()[*selected]);
        }

        // Right-aligned in the status line: add, then missing, then remove
        const bool  show_add     = add_to_playlist && selected.has_value();
        const bool  show_missing = status.missing_total > 0;
        const bool  show_remove  = show_missing && only_missing;
        const float right_end    = status_area.x + status_area.width;
        const Rect  add_area{right_end - add_button_width, status_area.y, add_button_width, status_area.height};
        add_button.setBounds(show_add ? add_area : Rect{});
        const float missing_right = show_add ? add_area.x - gap : right_end;
        const Rect  missing_area{missing_right - missing_button_width, status_area.y, missing_button_width,
                                 status_area.height};
        missing_button.setBounds(show_missing ? missing_area : Rect{});
        missing_button.setLabel("Missing: " + std::to_string(status.missing_total));
        missing_button.setHighlighted(only_missing);
        remove_button.setBounds(show_remove ? Rect{missing_area.x - remove_button_width - gap, status_area.y,
                                                   remove_button_width, status_area.height}
                                            : Rect{});
        remove_button.setLabel(confirm_remove_until ? "Tap again to remove " + std::to_string(status.missing_total)
                                                    : std::string{"Remove missing"});
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

    void LibraryView::updateStatus(const core::api::LibraryStatus &status)
    {
        status_text = std::to_string(status.tracks_total) + " tracks";
        if (status.scanning)
        {
            status_text += " - scanning, " + std::to_string(status.files_found) + " files";
        }
        if (status.pending_writes > 0)
        {
            status_text += " - saving tags";
        }
        const std::array<const std::string *, 4> errors{&search_error, &edit_message, &status.write_error,
                                                        &status.scan_error};
        for (const std::string *error : errors)
        {
            if (!error->empty())
            {
                status_text += " - " + *error;
                break;
            }
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
        if (edit)
        {
            cell_editor.draw(canvas);
        }
        // The status text leaves room for the buttons right of it
        float buttons_left = status_area.x + status_area.width;
        for (const TextButton *button : {&add_button, &missing_button, &remove_button})
        {
            if (button->bounds().width > 0.0F)
            {
                buttons_left = std::min(buttons_left, button->bounds().x);
            }
        }
        canvas.textElided({status_area.x, status_area.y + ((status_area.height - canvas.textHeight()) / 2)},
                          status_text, status_color, std::max(0.0F, buttons_left - gap - status_area.x));
        missing_button.draw(canvas);
        remove_button.draw(canvas);
        add_button.draw(canvas);
    }

    void LibraryView::wheel(Point position, float dy)
    {
        if (list.bounds().contains(position))
        {
            list.scrollBy(dy * TrackList::row_height);
        }
    }
} // namespace djw::player_ui::sdl
