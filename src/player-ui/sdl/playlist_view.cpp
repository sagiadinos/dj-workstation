// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "playlist_view.hpp"

#include <algorithm>
#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin     = 12.0F;
        constexpr float gap        = 8.0F;
        constexpr float row_height = 44.0F;
        constexpr float step_width = 48.0F;

        constexpr Color status_color{160, 160, 160};

        /// Lays out buttons side by side over area, equally wide
        void placeInRow(std::initializer_list<TextButton *> row, const Rect &area)
        {
            const auto  count = static_cast<float>(row.size());
            const float width = std::max(0.0F, (area.width - (gap * (count - 1))) / count);
            float       x     = area.x;
            for (TextButton *button : row)
            {
                button->setBounds({x, area.y, width, area.height});
                x += width + gap;
            }
        }
    } // namespace

    PlaylistView::PlaylistView(core::api::Library &library, FileDialogs &dialogs, const Translator &translator,
                               Load load, Clock clock)
        : library{&library}, playlists{&library.playlists()}, dialogs{&dialogs}, translator{&translator},
          load{std::move(load)}, clock{std::move(clock)}, list{this->clock}
    {
    }

    void PlaylistView::layout(const Rect &area)
    {
        const Rect inner = area.inset(margin);
        float      y     = inner.y;

        control(Button::Previous).setBounds({inner.x, y, step_width, row_height});
        control(Button::Next).setBounds({inner.x + inner.width - step_width, y, step_width, row_height});
        name_field.setBounds(
            {inner.x + step_width + gap, y, std::max(0.0F, inner.width - (2 * (step_width + gap))), row_height});
        y += row_height + gap;

        placeInRow(
            {&control(Button::New), &control(Button::Delete), &control(Button::Import), &control(Button::Export)},
            {inner.x, y, inner.width, row_height});
        y += row_height + gap;

        const float bottom = inner.y + inner.height;
        status_area        = {inner.x, bottom - 28.0F, inner.width, 28.0F};
        const float tools  = status_area.y - gap - row_height;
        placeInRow({&control(Button::Up), &control(Button::Down), &control(Button::Remove)},
                   {inner.x, tools, inner.width, row_height});

        list.setBounds({inner.x, y, inner.width, std::max(0.0F, tools - gap - y)});
        list.scrollBy(0.0F);
    }

    void PlaylistView::addTo(PointerRouter &router)
    {
        for (TextButton &each : buttons)
        {
            router.add(each);
        }
        router.add(name_field);
        router.add(list);
    }

    std::optional<core::api::Playlist> PlaylistView::current() const
    {
        if (all.empty() || !shown)
        {
            return std::nullopt;
        }
        const core::api::Playlist &found = all[shownIndex()];
        return found.id == *shown ? std::optional{found} : std::nullopt;
    }

    void PlaylistView::reload()
    {
        auto listed = playlists->list();
        if (!listed)
        {
            message = listed.error();
            return;
        }
        all = std::move(*listed);

        std::optional<std::int64_t> next = shown;
        if (select_by_name)
        {
            const auto named = std::ranges::find(all, *select_by_name, &core::api::Playlist::name);
            if (named != all.end())
            {
                next = named->id;
                select_by_name.reset();
            }
        }
        if (!std::ranges::any_of(all, [&next](const auto &playlist) { return next == playlist.id; }))
        {
            next = all.empty() ? std::nullopt : std::optional{all.front().id};
        }
        select(next);
    }

    void PlaylistView::select(std::optional<std::int64_t> playlist_id)
    {
        const bool other = playlist_id != shown;
        shown            = playlist_id;
        if (!name_field.focused() || other)
        {
            name_field.setText(current() ? current()->name : std::string{});
            name_field.setFocused(false);
        }
        if (!shown)
        {
            list.setTracks({});
            return;
        }
        auto tracks = playlists->tracks(*shown);
        if (!tracks)
        {
            message = tracks.error();
            return;
        }
        list.setTracks(std::move(*tracks), !other);
    }

    std::size_t PlaylistView::shownIndex() const
    {
        const auto found = std::ranges::find_if(all, [this](const auto &playlist) { return shown == playlist.id; });
        return found == all.end() ? 0 : static_cast<std::size_t>(found - all.begin());
    }

    void PlaylistView::step(int direction)
    {
        if (all.empty())
        {
            return;
        }
        commitRename();
        const auto count = static_cast<std::ptrdiff_t>(all.size());
        const auto next  = ((static_cast<std::ptrdiff_t>(shownIndex()) + direction) % count + count) % count;
        select(all[static_cast<std::size_t>(next)].id);
    }

    std::string PlaylistView::freeName() const
    {
        for (int number = 1;; ++number)
        {
            std::string name = translator->tr("playlist.default_name", {{"number", number}});
            if (std::ranges::none_of(all, [&name](const auto &playlist) { return playlist.name == name; }))
            {
                return name;
            }
        }
    }

    void PlaylistView::createNew()
    {
        commitRename();
        createPlaylist();
    }

    void PlaylistView::importFile()
    {
        dialogs->openPlaylist([this](const std::filesystem::path &file) { playlists->importM3u(file); });
    }

    void PlaylistView::exportShown()
    {
        if (const auto playlist = current())
        {
            const std::int64_t id = playlist->id;
            dialogs->savePlaylist(playlist->name + ".m3u8",
                                  [this, id](const std::filesystem::path &file) { playlists->exportM3u(id, file); });
        }
    }

    void PlaylistView::applyTranslations()
    {
        name_field.setPlaceholder(translator->tr("playlist.name_placeholder"));
        control(Button::New).setLabel(translator->tr("playlist.new"));
        control(Button::Delete)
            .setLabel(translator->tr(confirm_delete.armed() ? "playlist.delete_confirm" : "playlist.delete"));
        control(Button::Import).setLabel(translator->tr("playlist.import"));
        control(Button::Export).setLabel(translator->tr("playlist.export"));
        control(Button::Up).setLabel(translator->tr("playlist.up"));
        control(Button::Down).setLabel(translator->tr("playlist.down"));
        control(Button::Remove).setLabel(translator->tr("playlist.remove"));
    }

    void PlaylistView::createPlaylist()
    {
        std::string name = freeName();
        playlists->create(name);
        select_by_name = std::move(name);
    }

    void PlaylistView::commitRename()
    {
        const auto playlist = current();
        const bool editing  = name_field.focused();
        name_field.setFocused(false);
        if (!editing || !playlist || name_field.text() == playlist->name)
        {
            return;
        }
        if (name_field.text().empty())
        {
            name_field.setText(playlist->name);
            return;
        }
        playlists->rename(playlist->id, name_field.text());
    }

    void PlaylistView::move(int direction)
    {
        const auto row = list.selected();
        if (!shown || !row)
        {
            return;
        }
        const auto target = static_cast<std::ptrdiff_t>(*row) + direction;
        if (target < 0 || std::cmp_greater_equal(target, list.tracks().size()))
        {
            return;
        }
        playlists->moveEntry(*shown, *row, static_cast<std::size_t>(target));
        // Shown at once; the reload after the write confirms it
        auto tracks = list.tracks();
        std::swap(tracks[*row], tracks[static_cast<std::size_t>(target)]);
        list.setTracks(std::move(tracks), true);
        list.select(static_cast<std::size_t>(target));
    }

    void PlaylistView::handleButtons()
    {
        if (control(Button::Previous).takePress())
        {
            step(-1);
        }
        if (control(Button::Next).takePress())
        {
            step(1);
        }
        if (control(Button::New).takePress())
        {
            createNew();
        }
        if (control(Button::Delete).takePress() && shown && confirm_delete.tap())
        {
            playlists->remove(*shown);
        }
        applyTranslations();

        if (control(Button::Import).takePress())
        {
            importFile();
        }
        if (control(Button::Export).takePress())
        {
            exportShown();
        }
        if (control(Button::Up).takePress())
        {
            move(-1);
        }
        if (control(Button::Down).takePress())
        {
            move(1);
        }
        const auto selected = list.selected();
        if (control(Button::Remove).takePress() && shown && selected)
        {
            playlists->removeEntry(*shown, *selected);
            auto tracks = list.tracks();
            tracks.erase(tracks.begin() + static_cast<std::ptrdiff_t>(*selected));
            list.setTracks(std::move(tracks), true);
        }
    }

    void PlaylistView::addTrack(const core::api::LibraryTrack &track)
    {
        if (!shown)
        {
            message_key = "playlist.error.create_first";
            return;
        }
        message_key = {};
        message.clear();
        playlists->add(*shown, {track.id});
    }

    void PlaylistView::update()
    {
        if (name_field.takeFocusRequest())
        {
            name_field.setFocused(true);
        }
        if (list.takeTouched())
        {
            commitRename();
        }
        handleButtons();

        const core::api::LibraryStatus status       = library->status();
        const bool                     first_update = !seen_revision.has_value();
        if (status.last_import != seen_import)
        {
            // An import of this session: show the playlist it made (an older one from the status is just noted)
            if (!first_update && status.last_import)
            {
                select_by_name = status.last_import->playlist;
                show_import    = true;
            }
            seen_import = status.last_import;
        }
        if (status.revision != seen_revision || select_by_name)
        {
            seen_revision = status.revision;
            reload();
        }

        status_text = current() ? translator->tr("tracks.count", {{"count", current()->track_count}})
                                : translator->tr("playlist.none");
        if (status.pending_writes > 0)
        {
            status_text += " - " + translator->tr("playlist.saving");
        }
        if (!message_key.empty())
        {
            status_text += " - " + translator->tr(message_key);
        }
        else if (!message.empty())
        {
            status_text += " - " + message;
        }
        else if (!status.write_error.empty())
        {
            status_text += " - " + status.write_error;
        }
        else if (show_import && seen_import)
        {
            status_text +=
                " - " + translator->tr("playlist.imported",
                                       {{"name", seen_import->playlist},
                                        {"tracks", translator->tr("tracks.count", {{"count", seen_import->tracks}})},
                                        {"not_found", seen_import->not_found}});
        }

        if (const auto row = list.takeActivated(); row && load)
        {
            load(list.tracks()[*row]);
        }
    }

    void PlaylistView::keyboard(const KeyboardInput &input)
    {
        if (!name_field.focused())
        {
            // The list: cursor keys and pages move the selection, Enter loads the track selected
            if (const auto *key = std::get_if<EditKey>(&input); key != nullptr)
            {
                if (*key == EditKey::Enter)
                {
                    if (const auto row = list.selected(); row && load)
                    {
                        load(list.tracks()[*row]);
                    }
                    return;
                }
                static_cast<void>(list.navigate(*key));
            }
            return;
        }
        if (const auto *key = std::get_if<EditKey>(&input); key != nullptr && *key == EditKey::Enter)
        {
            commitRename();
            return;
        }
        if (const auto *key = std::get_if<EditKey>(&input); key != nullptr && *key == EditKey::Escape)
        {
            name_field.setFocused(false);
            name_field.setText(current() ? current()->name : std::string{});
            return;
        }
        static_cast<void>(name_field.input(input));
    }

    void PlaylistView::wheel(Point position, float dy)
    {
        if (list.bounds().contains(position))
        {
            list.scrollBy(dy * TrackList::row_height);
        }
    }

    void PlaylistView::draw(Canvas &canvas) const
    {
        for (const TextButton &each : buttons)
        {
            each.draw(canvas);
        }
        name_field.draw(canvas);
        list.draw(canvas);
        canvas.textElided({status_area.x, status_area.y + ((status_area.height - canvas.textHeight()) / 2)},
                          status_text, status_color, status_area.width);
    }
} // namespace djw::player_ui::sdl
