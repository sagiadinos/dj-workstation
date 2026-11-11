// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_PLAYLIST_VIEW_HPP
#define DJW_PLAYER_UI_SDL_PLAYLIST_VIEW_HPP

#include "api/library.hpp"
#include "api/playlists.hpp"
#include "clock.hpp"
#include "timed_state.hpp"
#include "file_dialogs.hpp"
#include "i18n/translator.hpp"
#include "text_button.hpp"
#include "text_field.hpp"
#include "track_list.hpp"
#include "view.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * The playlist window: one saved playlist at a time with its tracks in order.
     *
     * Top: previous/next playlist around the name (tap to rename, Enter or touching anything else saves), then New,
     * Delete (second tap within confirm_time), Import and Export (M3U/M3U8 through the file dialogs). Bottom: move the
     * selected entry up or down, remove it, and a status line. A double tap loads a track into the deck.
     *
     * All changes go to the library, which writes them on its thread; the view reads the playlists again when the
     * library's revision changes; the selection keeps its row, so it follows an entry moved with Up and Down.
     */
    class PlaylistView final : public View
    {
        public:
            using Load = std::function<void(const core::api::LibraryTrack &track)>;

            static constexpr std::chrono::milliseconds confirm_time{3'000};

            PlaylistView(core::api::Library &library, FileDialogs &dialogs, const Translator &translator, Load load,
                         Clock clock = steadyClock());

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;
            void update() override;
            void draw(Canvas &canvas) const override;

            [[nodiscard]] bool wantsTextInput() const override { return name_field.focused(); }
            void               keyboard(const KeyboardInput &input) override;
            void               wheel(Point position, float dy) override;

            /// Appends a track to the playlist shown, e.g. from the library window
            void addTrack(const core::api::LibraryTrack &track);

            /// What the New, Import and Export buttons do; also for the menu of the library window
            void createNew();
            void importFile();
            void exportShown();

            /// The playlist shown, std::nullopt when there are none
            [[nodiscard]] std::optional<core::api::Playlist> current() const;
            [[nodiscard]] const TrackList                   &trackList() const noexcept { return list; }
            [[nodiscard]] const TextField                   &nameField() const noexcept { return name_field; }
            [[nodiscard]] const std::string                 &statusText() const noexcept { return status_text; }

            enum class Button : std::uint8_t
            {
                Previous,
                Next,
                New,
                Delete,
                Import,
                Export,
                Up,
                Down,
                Remove
            };
            [[nodiscard]] const TextButton &button(Button which) const { return buttons.at(index(which)); }

        private:
            static constexpr std::size_t button_count = 9;
            static constexpr std::size_t index(Button which) { return static_cast<std::size_t>(which); }
            TextButton                  &control(Button which) { return buttons.at(index(which)); }

            void                      applyTranslations();
            void                      reload();
            void                      select(std::optional<std::int64_t> playlist_id);
            void                      handleButtons();
            void                      commitRename();
            void                      step(int direction);
            void                      move(int direction);
            void                      createPlaylist();
            [[nodiscard]] std::string freeName() const;
            [[nodiscard]] std::size_t shownIndex() const;

            core::api::Library                  *library;
            core::api::Playlists                *playlists;
            FileDialogs                         *dialogs;
            const Translator                    *translator;
            Load                                 load;
            Clock                                clock;
            ConfirmTap                           confirm_delete{clock, confirm_time};
            std::array<TextButton, button_count> buttons{TextButton{"<"}, TextButton{">"}, TextButton{},
                                                         TextButton{},    TextButton{},    TextButton{},
                                                         TextButton{},    TextButton{},    TextButton{}};
            TextField                            name_field;
            TrackList                            list;
            Rect                                 status_area;
            std::vector<core::api::Playlist>     all;
            std::optional<std::int64_t>          shown;
            std::optional<std::string>           select_by_name; // after a create or import, once it is there

            /// Why the playlists could not be read, as the library says; empty if there is no problem
            std::string message;
            /// Key of a message of the view itself for the status line, empty if none; translated when shown
            std::string_view                         message_key;
            std::string                              status_text;
            std::optional<std::uint64_t>             seen_revision;
            std::optional<core::api::PlaylistImport> seen_import;
            /// An import of this session finished; its outcome shows in the status line
            bool show_import = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_PLAYLIST_VIEW_HPP
