// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP
#define DJW_PLAYER_UI_SDL_LIBRARY_VIEW_HPP

#include "api/library.hpp"
#include "clock.hpp"
#include "timed_state.hpp"
#include "column_header.hpp"
#include "file_dialogs.hpp"
#include "i18n/translator.hpp"
#include "layout.hpp"
#include "context_menu.hpp"
#include "menu_bar.hpp"
#include "text_button.hpp"
#include "text_field.hpp"
#include "track_list.hpp"
#include "view.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <functional>
#include <string>
#include <string_view>

namespace djw::player_ui::sdl
{
    /**
     * The library window: a search field over all fields, filter fields for artist, title, genre and year right
     * above their columns, the column header (tap to sort, tap again to reverse), the matching tracks and a status
     * line with the track count and the scan progress.
     *
     * Searches as the user types. While a scan runs the list is refreshed once per refresh_interval, so new tracks
     * show up, and again whenever the library's revision changes (scan end, written tag edits). A double tap on a
     * track, or Enter with a track selected, hands it to load. Typed text goes to the field tapped last.
     *
     * Tag editor: a long press on an artist, title, genre or year cell opens an editor in the cell. Enter or touching
     * anything else saves, Escape cancels. The change goes to the library, which writes the file in the background;
     * the row shows the new value at once. Tracks for which in_use is true (loaded in a player) are not changed.
     *
     * Menu bar on top (Database, Playlist, Settings, Language, View): add a music folder (folder dialog) and rescan;
     * the playlist commands, which the owner carries out in the playlist window (opening it if needed); the language,
     * which takes effect at once; new player (Ctrl+N is handled by the app), clear the search, show only missing
     * tracks, show or hide the playlist and crossfader windows, save the layout, and whether the next start shows each
     * part in a window of its own or all in one window. All texts come from the translator on every update.
     *
     * Context menu: the right mouse button on a track selects it and opens a menu with "Edit tags" (the artist cell
     * of the track opens in the editor) and "Scan again" (the library reads the file anew and analyzes it again).
     *
     * Analysis: the Database menu has "Analyze all tracks", which has the library work through the tracks that were not
     * analyzed (tempo, waveform) in the background; the status line shows how many are left.
     *
     * Dead links: when tracks are missing, the status line shows a "Missing" button that lists only those; there a
     * "Remove" button takes them out of the library after a second tap within confirm_time. With setAddToPlaylist,
     * an "Add to playlist" button at the right end of the status line hands the selected track on.
     */
    class LibraryView final : public View
    {
        public:
            using Load  = std::function<void(const core::api::LibraryTrack &track)>;
            using InUse = std::function<bool(const std::filesystem::path &file)>;
            using Add   = std::function<void(const core::api::LibraryTrack &track)>;

            /// What the Playlist menu asks the playlist window to do
            enum class PlaylistCommand : std::uint8_t
            {
                New,
                Import,
                Export,
                /// Shows the playlist window, or hides it if it is shown
                ToggleWindow
            };
            using PlaylistAction = std::function<void(PlaylistCommand command)>;
            using PlaylistShown  = std::function<bool()>;
            using LanguageChosen = std::function<void(Language language)>;
            using NewDeck        = std::function<void()>;
            /// Saves the window layout; true when it was saved
            using SaveLayout = std::function<bool()>;
            using Toggle     = std::function<void()>;
            using Shown      = std::function<bool()>;
            /// Keeps the window mode for the next start; true when it was kept
            using ModeChosen = std::function<bool(WindowMode mode)>;
            /// The window mode the next start uses
            using ModeShown = std::function<WindowMode()>;

            /// How long a notice (see setNotice) stays in the status line
            static constexpr std::chrono::milliseconds notice_time{4'000};

            /// Most tracks listed at once; the search narrows it down
            static constexpr std::size_t               result_limit = 1'000;
            static constexpr std::chrono::milliseconds refresh_interval{1'000};
            static constexpr std::chrono::milliseconds confirm_time{3'000};

            /// The columns with a filter field, in this order
            static constexpr std::array<core::api::LibraryColumn, 4> filter_columns{
                core::api::LibraryColumn::Artist, core::api::LibraryColumn::Title, core::api::LibraryColumn::Genre,
                core::api::LibraryColumn::Year};

            LibraryView(core::api::Library &library, FileDialogs &dialogs, const Translator &translator, Load load,
                        InUse in_use = {}, Clock clock = steadyClock());

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;
            void update() override;
            void draw(Canvas &canvas) const override;

            [[nodiscard]] bool wantsTextInput() const override;
            void               keyboard(const KeyboardInput &input) override;
            void               wheel(Point position, float dy) override;
            void               secondaryClick(Point position) override;

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
            [[nodiscard]] const TextButton              &missingButton() const noexcept { return missing_button; }
            [[nodiscard]] const TextButton              &removeButton() const noexcept { return remove_button; }
            [[nodiscard]] const TextButton              &addButton() const noexcept { return add_button; }
            /// What View > Save layout does; without it the entry is off
            void                         setSaveLayout(SaveLayout save) { save_layout = std::move(save); }
            [[nodiscard]] const MenuBar &menuBar() const noexcept { return menu; }
            /// The menu bar, to be shown elsewhere (the top of the one window, see MenuStrip): from then on the view
            /// neither places, draws nor routes it, and takes no room for it; it still sets its menus and carries out
            /// the entries chosen
            [[nodiscard]] MenuBar &moveMenuBarOut() noexcept
            {
                menu_elsewhere = true;
                return menu;
            }
            [[nodiscard]] const ContextMenu &contextMenu() const noexcept { return context_menu; }

            /// Shows an "Add to playlist" button for the selected track, which hands the track to add
            void setAddToPlaylist(Add add) { add_to_playlist = std::move(add); }
            /// Enables the Playlist menu and the playlist window entry of the View menu: the commands go to action,
            /// shown tells whether the playlist window is open
            void setPlaylistCommands(PlaylistAction action, PlaylistShown shown = {})
            {
                playlist_action = std::move(action);
                playlist_shown  = std::move(shown);
            }
            /// Enables View > Crossfader window: toggle shows or hides it, shown tells whether it is open
            void setMixerWindow(Toggle toggle, Shown shown)
            {
                mixer_toggle = std::move(toggle);
                mixer_shown  = std::move(shown);
            }
            /// Enables the window mode entries of the View menu (separate windows, one window); the choice takes
            /// effect at the next start, which the status line says
            void setWindowMode(ModeChosen chosen, ModeShown shown)
            {
                mode_chosen = std::move(chosen);
                mode_shown  = std::move(shown);
            }
            /// Enables View > Edit layout and View > Arrange automatically (the MDI mode): edit starts the editing,
            /// editing tells whether it is on, automatic goes back to the automatic arrangement
            /// Enables Settings > Devices: open shows the dialog
            void setDevicesDialog(Toggle open) { devices_dialog = std::move(open); }
            /// Enables View > Fullscreen (the MDI mode): toggle switches, shown tells whether the window is fullscreen
            void setFullscreen(Toggle toggle, Shown shown)
            {
                fullscreen_toggle = std::move(toggle);
                fullscreen_shown  = std::move(shown);
            }
            void setLayoutEditing(Toggle edit, Shown editing, Toggle automatic)
            {
                edit_layout       = std::move(edit);
                layout_editing    = std::move(editing);
                arrange_automatic = std::move(automatic);
            }
            /// The track selected in the list, std::nullopt if none
            [[nodiscard]] std::optional<core::api::LibraryTrack> selectedTrack() const
            {
                const auto row = list.selected();
                return row ? std::optional{list.tracks()[*row]} : std::nullopt;
            }
            /// Shows a message (a catalog key) in the status line for notice_time, e.g. why a track was not loaded
            void setNotice(std::string_view key);
            /// Enables View > New deck, which calls new_deck
            void setOnNewDeck(NewDeck new_deck) { new_deck_requested = std::move(new_deck); }
            /// Called when the user picks a language in the Language menu
            void setOnLanguage(LanguageChosen chosen) { language_chosen = std::move(chosen); }

        private:
            /// The cell being edited; the track is kept by id, as a refresh may move its row
            struct CellEdit
            {
                    std::int64_t             track_id = 0;
                    core::api::LibraryColumn column{};
                    std::string              original;
            };

            /// The field tapped last takes the focus from all others
            void resolveFocus();
            /// Sets the texts of fields, header and buttons in the current language
            void applyTranslations();
            /// Builds the menus for the current state and carries out the entry chosen
            void updateMenu(const core::api::LibraryStatus &status);
            void chooseMenuEntry(int id);
            /// Tab: the focus to the next search field (the search, then the filters), round; Shift+Tab backwards
            void focusNextField(bool forward);
            void startEdit(const TrackList::Cell &cell);
            void updateContextMenu();
            void commitEdit();
            void cancelEdit();
            void placeEditor();
            /// Shows, labels and handles the dead link buttons for the current status
            void                                     updateMissingButtons(const core::api::LibraryStatus &status);
            [[nodiscard]] std::optional<std::size_t> rowOf(std::int64_t track_id) const;
            [[nodiscard]] core::api::LibraryQuery    currentQuery() const;
            void                                     applySortTap();
            void                                     search(const core::api::LibraryQuery &query, bool keep_position);
            void                                     updateStatus(const core::api::LibraryStatus &status);

            core::api::Library      *library;
            FileDialogs             *dialogs;
            const Translator        *translator;
            Load                     load;
            InUse                    in_use;
            Clock                    clock;
            TimedNotice              notice{clock, notice_time};
            ConfirmTap               confirm_remove{clock, confirm_time};
            MenuBar                  menu;
            ContextMenu              context_menu;
            SaveLayout               save_layout;
            Toggle                   edit_layout;
            Shown                    layout_editing;
            Toggle                   arrange_automatic;
            Toggle                   fullscreen_toggle;
            bool                     menu_elsewhere = false;
            Toggle                   devices_dialog;
            Shown                    fullscreen_shown;
            std::int64_t             context_track = 0; // the track the context menu is about
            TextField                search_field;
            std::array<TextField, 4> filters;
            ColumnHeader             header;
            TrackList                list;
            TextField                cell_editor;
            TextButton               missing_button;
            TextButton               remove_button;
            TextButton               add_button;
            Add                      add_to_playlist;
            PlaylistAction           playlist_action;
            PlaylistShown            playlist_shown;
            NewDeck                  new_deck_requested;
            Toggle                   mixer_toggle;
            Shown                    mixer_shown;
            ModeChosen               mode_chosen;
            ModeShown                mode_shown;
            LanguageChosen           language_chosen;
            bool                     only_missing = false;
            std::optional<CellEdit>  edit;
            /// Key of the message about the last edit, empty if none; translated when shown
            std::string_view edit_message;
            /// Why the last folder could not be added, as the library says
            std::string                           folder_error;
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
