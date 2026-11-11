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

        /// Ids of the menu entries; the languages follow language_base in the order of languages
        namespace menu_id
        {
            constexpr int add_folder             = 1;
            constexpr int rescan                 = 2;
            constexpr int analyze_all            = 3;
            constexpr int context_edit_tags      = 4;
            constexpr int context_rescan_track   = 5;
            constexpr int playlist_new           = 6;
            constexpr int playlist_import        = 7;
            constexpr int playlist_export        = 8;
            constexpr int playlist_add_track     = 9;
            constexpr int clear_filters          = 10;
            constexpr int missing_only           = 11;
            constexpr int toggle_playlist_window = 12;
            constexpr int new_deck_entry         = 13;
            constexpr int toggle_mixer_window    = 14;
            constexpr int save_layout_entry      = 15;
            constexpr int separate_windows       = 16;
            constexpr int one_window             = 17;
            constexpr int edit_layout            = 18;
            constexpr int arrange_automatically  = 19;
            constexpr int language_base          = 100;
        } // namespace menu_id

        constexpr std::array<std::string_view, 6> column_keys{"column.artist", "column.title", "column.genre",
                                                              "column.year",   "column.bpm",   "column.time"};

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
                case core::api::LibraryColumn::Bpm:
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

    LibraryView::LibraryView(core::api::Library &library, FileDialogs &dialogs, const Translator &translator, Load load,
                             InUse in_use, Clock clock)
        : library{&library}, dialogs{&dialogs}, translator{&translator}, load{std::move(load)},
          in_use{std::move(in_use)}, clock{std::move(clock)}, list{this->clock}
    {
    }

    void LibraryView::layout(const Rect &area)
    {
        const float bar_height = std::min(MenuBar::bar_height, area.height);
        menu.place({area.x, area.y, area.width, bar_height}, area);
        context_menu.place(area);
        const Rect below{area.x, area.y + bar_height, area.width, area.height - bar_height};
        const Rect inner = below.inset(margin);
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
        router.add(cell_editor);  // on top of the list; without an edit it has no area and catches nothing
        router.add(context_menu); // covers the window while open
        router.add(menu);         // on top of everything: an open menu covers the window
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
        if (const auto *key = std::get_if<EditKey>(&input); menu.isOpen() && key != nullptr && *key == EditKey::Escape)
        {
            menu.close();
            return;
        }
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
        if (const auto *key = std::get_if<EditKey>(&input); key != nullptr && *key == EditKey::Enter)
        {
            // A track chosen in the list goes into a player, as with a double tap
            if (const auto row = list.selected(); row && load)
            {
                load(list.tracks()[*row]);
            }
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
            edit_message = "library.error.in_use";
            return;
        }
        edit_message = {};
        edit         = CellEdit{.track_id = track.id, .column = cell.column, .original = *value};
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
                    edit_message = "library.error.year";
                    return;
                }
                break;
            case core::api::LibraryColumn::Bpm:
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
        edit_message = {};
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
        applyTranslations();
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
        updateMenu(status);
        updateContextMenu();

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
            confirm_remove.cancel();
        }
        if (remove_button.takePress() && confirm_remove.tap())
        {
            library->removeMissing();
            only_missing = false;
        }
        if (status.missing_total == 0)
        {
            only_missing = false; // nothing left to show there
            confirm_remove.cancel();
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
        missing_button.setLabel(translator->tr("library.missing", {{"count", status.missing_total}}));
        missing_button.setHighlighted(only_missing);
        remove_button.setBounds(show_remove ? Rect{missing_area.x - remove_button_width - gap, status_area.y,
                                                   remove_button_width, status_area.height}
                                            : Rect{});
        remove_button.setLabel(confirm_remove.armed()
                                   ? translator->tr("library.remove_confirm", {{"count", status.missing_total}})
                                   : translator->tr("library.remove_missing"));
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
        status_text = translator->tr("tracks.count", {{"count", status.tracks_total}});
        if (status.scanning)
        {
            status_text +=
                " - " + translator->tr("library.scanning",
                                       {{"files", translator->tr("files.count", {{"count", status.files_found}})}});
        }
        if (status.pending_writes > 0)
        {
            status_text += " - " + translator->tr("library.saving_tags");
        }
        if (status.analysis_pending > 0)
        {
            status_text += " - " + translator->tr("library.analyzing", {{"count", status.analysis_pending}});
        }
        if (const std::string_view key = notice.current(); !key.empty())
        {
            status_text += " - " + translator->tr(key);
        }
        const std::string edit_text = edit_message.empty() ? std::string{} : translator->tr(edit_message);
        const std::array<const std::string *, 5> errors{&search_error, &edit_text, &folder_error, &status.write_error,
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

    void LibraryView::applyTranslations()
    {
        search_field.setPlaceholder(translator->tr("search.placeholder"));
        std::array<std::string, track_columns.size()> names;
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            names[index] = translator->tr(column_keys[index]);
        }
        for (std::size_t index = 0; index < filters.size(); ++index)
        {
            filters[index].setPlaceholder(names[columnIndex(filter_columns[index])]);
        }
        header.setNames(std::move(names));
        add_button.setLabel(translator->tr("library.add_to_playlist"));
    }

    void LibraryView::updateMenu(const core::api::LibraryStatus &status)
    {
        const auto tr            = [this](std::string_view key) { return translator->tr(key); };
        const bool has_playlists = static_cast<bool>(playlist_action);
        const bool can_add_track = has_playlists && add_to_playlist && list.selected().has_value();
        const bool has_mode      = mode_chosen && mode_shown;

        std::vector<MenuItem> language_items;
        language_items.reserve(languages.size());
        for (std::size_t index = 0; index < languages.size(); ++index)
        {
            language_items.push_back({.id      = menu_id::language_base + static_cast<int>(index),
                                      .label   = std::string{languageName(languages[index])},
                                      .enabled = true,
                                      .checked = languages[index] == translator->language()});
        }

        std::vector<Menu> menus;
        menus.push_back({tr("menu.database"),
                         {{.id = menu_id::add_folder, .label = tr("menu.database.add_folder")},
                          {.id = menu_id::rescan, .label = tr("menu.database.rescan")},
                          {.id = menu_id::analyze_all, .label = tr("menu.database.analyze_all")}}});
        menus.push_back(
            {tr("menu.playlist"),
             {{.id = menu_id::playlist_new, .label = tr("menu.playlist.new"), .enabled = has_playlists},
              {.id = menu_id::playlist_import, .label = tr("menu.playlist.import"), .enabled = has_playlists},
              {.id = menu_id::playlist_export, .label = tr("menu.playlist.export"), .enabled = has_playlists},
              {.id = menu_id::playlist_add_track, .label = tr("menu.playlist.add_track"), .enabled = can_add_track}}});
        // No settings yet but the language, which has its own menu
        menus.push_back({tr("menu.settings"), {{.id = 0, .label = tr("menu.settings.none"), .enabled = false}}});
        menus.push_back({tr("menu.language"), std::move(language_items)});
        menus.push_back({tr("menu.view"),
                         {{.id       = menu_id::new_deck_entry,
                           .label    = tr("menu.view.new_deck"),
                           .enabled  = static_cast<bool>(new_deck_requested),
                           .shortcut = "Ctrl+N"},
                          {.id      = menu_id::toggle_mixer_window,
                           .label   = tr("menu.view.mixer_window"),
                           .enabled = static_cast<bool>(mixer_toggle),
                           .checked = mixer_shown && mixer_shown()},
                          {.id      = menu_id::toggle_playlist_window,
                           .label   = tr("menu.view.playlist_window"),
                           .enabled = has_playlists,
                           .checked = playlist_shown && playlist_shown()},
                          {.id = menu_id::clear_filters, .label = tr("menu.view.clear_filters")},
                          {.id      = menu_id::missing_only,
                           .label   = tr("menu.view.missing_only"),
                           .enabled = status.missing_total > 0 || only_missing,
                           .checked = only_missing},
                          {.id      = menu_id::save_layout_entry,
                           .label   = tr("menu.view.save_layout"),
                           .enabled = static_cast<bool>(save_layout)},
                          {.id      = menu_id::separate_windows,
                           .label   = tr("menu.view.separate_windows"),
                           .enabled = has_mode,
                           .checked = has_mode && mode_shown() == WindowMode::Separate},
                          {.id      = menu_id::one_window,
                           .label   = tr("menu.view.one_window"),
                           .enabled = has_mode,
                           .checked = has_mode && mode_shown() == WindowMode::Mdi},
                          {.id      = menu_id::edit_layout,
                           .label   = tr("menu.view.edit_layout"),
                           .enabled = static_cast<bool>(edit_layout),
                           .checked = layout_editing && layout_editing()},
                          {.id      = menu_id::arrange_automatically,
                           .label   = tr("menu.view.arrange_automatically"),
                           .enabled = static_cast<bool>(arrange_automatic)}}});
        menu.setMenus(std::move(menus));
        menu.setRightToLeft(translator->rightToLeft());

        if (const auto chosen = menu.takeChosen())
        {
            chooseMenuEntry(*chosen);
        }
    }

    void LibraryView::chooseMenuEntry(int id)
    {
        if (id >= menu_id::language_base)
        {
            const auto index = static_cast<std::size_t>(id - menu_id::language_base);
            if (index < languages.size() && language_chosen)
            {
                language_chosen(languages[index]);
            }
            return;
        }
        switch (id)
        {
            case menu_id::add_folder:
                dialogs->openFolder(
                    [this](const std::filesystem::path &folder)
                    {
                        auto added   = library->addFolder(folder);
                        folder_error = added ? std::string{} : added.error();
                    });
                break;
            case menu_id::rescan:
                library->rescan();
                break;
            case menu_id::analyze_all:
                library->analyzeAll();
                break;
            case menu_id::save_layout_entry:
                if (save_layout)
                {
                    setNotice(save_layout() ? "library.notice.layout_saved" : "library.notice.layout_failed");
                }
                break;
            case menu_id::separate_windows:
            case menu_id::one_window:
                if (mode_chosen)
                {
                    const WindowMode mode = id == menu_id::one_window ? WindowMode::Mdi : WindowMode::Separate;
                    setNotice(mode_chosen(mode) ? "library.notice.next_start" : "library.notice.layout_failed");
                }
                break;
            case menu_id::edit_layout:
                if (edit_layout)
                {
                    edit_layout();
                }
                break;
            case menu_id::arrange_automatically:
                if (arrange_automatic)
                {
                    arrange_automatic();
                }
                break;
            case menu_id::toggle_mixer_window:
                if (mixer_toggle)
                {
                    mixer_toggle();
                }
                break;
            case menu_id::new_deck_entry:
                if (new_deck_requested)
                {
                    new_deck_requested();
                }
                break;
            case menu_id::toggle_playlist_window:
                if (playlist_action)
                {
                    playlist_action(PlaylistCommand::ToggleWindow);
                }
                break;
            case menu_id::playlist_new:
            case menu_id::playlist_import:
            case menu_id::playlist_export:
                if (playlist_action)
                {
                    playlist_action(id == menu_id::playlist_new      ? PlaylistCommand::New
                                    : id == menu_id::playlist_import ? PlaylistCommand::Import
                                                                     : PlaylistCommand::Export);
                }
                break;
            case menu_id::playlist_add_track:
                if (const auto selected = list.selected(); selected && add_to_playlist)
                {
                    add_to_playlist(list.tracks()[*selected]);
                }
                break;
            case menu_id::clear_filters:
                search_field.setText({});
                for (TextField &filter : filters)
                {
                    filter.setText({});
                }
                only_missing = false;
                break;
            case menu_id::missing_only:
                only_missing = !only_missing;
                confirm_remove.cancel();
                break;
            default:
                break;
        }
    }

    void LibraryView::secondaryClick(Point position)
    {
        if (menu.isOpen() || edit)
        {
            return;
        }
        const auto row = list.rowAt(position);
        if (!row)
        {
            context_menu.close();
            return;
        }
        list.select(*row);
        context_track = list.tracks()[*row].id;
        updateContextMenu();
        context_menu.open(position);
    }

    void LibraryView::updateContextMenu()
    {
        context_menu.setItems({{.id = menu_id::context_edit_tags, .label = translator->tr("context.edit_tags")},
                               {.id = menu_id::context_rescan_track, .label = translator->tr("context.rescan_track")}});
        if (const auto chosen = context_menu.takeChosen())
        {
            if (*chosen == menu_id::context_edit_tags)
            {
                if (const auto row = rowOf(context_track))
                {
                    startEdit({.row = *row, .column = core::api::LibraryColumn::Artist});
                }
            }
            else if (*chosen == menu_id::context_rescan_track)
            {
                library->rescanTrack(context_track);
            }
        }
    }

    void LibraryView::setNotice(std::string_view key)
    {
        notice.show(key);
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
        context_menu.draw(canvas);
        menu.draw(canvas); // last, so an open menu lies over everything
    }

    void LibraryView::wheel(Point position, float dy)
    {
        if (list.bounds().contains(position))
        {
            list.scrollBy(dy * TrackList::row_height);
        }
    }
} // namespace djw::player_ui::sdl
