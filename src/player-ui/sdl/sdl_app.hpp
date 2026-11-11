// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_APP_HPP
#define DJW_PLAYER_UI_SDL_SDL_APP_HPP

#include "api/core.hpp"
#include "api/library.hpp"
#include "deck_view.hpp"
#include "i18n/translator.hpp"
#include "layout.hpp"
#include "library_view.hpp"
#include "mixer_view.hpp"
#include "pane_frame.hpp"
#include "playlist_view.hpp"
#include "sdl_file_dialogs.hpp"
#include "sdl_window.hpp"

#include <SDL3/SDL_video.h>

#include <cstddef>
#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * The SDL3 player: a window for each deck (the player), and, if there is a library, the library window, which is
     * the main window. The playlist and the crossfader window open from the View menu (the playlist also from the
     * Playlist menu) and are closed again with their own close button. Owns SDL and SDL_ttf and runs the event loop.
     *
     * Players: there is no upper limit. View > New player or Ctrl+N adds a deck with its own window; closing a player
     * window removes that deck. A track chosen in the library (Enter or a double tap) or the playlist goes to the
     * first player that does not play, in the order they were opened; the Load button of a player loads the selected
     * track of the library into that player. A playing player is never loaded. Closing the library window ends the
     * program, as does closing the last player window when there is no library.
     *
     * Window modes (View menu, from the next start on): each part in a window of its own as above, or all in one
     * window that covers the main display (MDI). There every part has a title bar, players and the crossfader and
     * playlist with a close button, and the parts are arranged automatically without overlapping (see arrangeMdi).
     * Closing that window ends the program. Each mode keeps its own layout in the layout file.
     *
     * Declaration order is destruction order in reverse: windows before the views they show, SDL itself last.
     */
    class SdlApp
    {
        public:
            /// Called with the new language when the user picks one in the Language menu, to keep it for the next start
            using LanguageChanged = std::function<void(Language language)>;

            /// The windows that come and go on request
            enum class Tool : std::uint8_t
            {
                Playlist,
                Mixer
            };

            /// library may be nullptr: then there is no library window. The texts start in language.
            static std::expected<std::unique_ptr<SdlApp>, std::string>
            create(core::api::Core &core, core::api::DeckId deck, core::api::Library *library, Language language,
                   LanguageChanged language_changed = {}, std::filesystem::path layout_file = {});

            /// Runs until the program is to end (see above)
            void run();
            /// One pass of the event loop: handles the events, updates and draws the windows. False when the program
            /// is to end. run() is this in a loop; tests call it to step through events.
            bool runFrame();

            /// What the first deck shows as its track, for a track loaded from outside the library
            void setDeckTrack(std::filesystem::path file, std::string title);

            /// A file or folder dropped from the file manager onto the window with the SDL id window: a file is loaded
            /// into that player (not while it plays); a folder dropped on the library window is added to the library.
            /// In the one window of the MDI mode the part at position decides; without a position nothing happens.
            /// Anything else is ignored. The event loop calls this; tests call it directly.
            void dropped(SDL_WindowID window, const std::filesystem::path &path,
                         std::optional<Point> position = std::nullopt);

            /// Loads a track into the first player that does not play (what Enter and a double tap do). With no
            /// player yet, one is opened for it; with all of them playing nothing is loaded and the library says so.
            void load(const core::api::LibraryTrack &track);

            /// Number of decks with a window
            [[nodiscard]] std::size_t deckCount() const noexcept { return decks.size(); }
            /// The SDL id of the window of the deck at index (in the order they were opened); for tests
            [[nodiscard]] SDL_WindowID deckWindowId(std::size_t index) const { return decks.at(index)->window->id(); }
            /// The deck at index; for tests
            [[nodiscard]] core::api::DeckId deckId(std::size_t index) const { return decks.at(index)->id; }
            /// The SDL id of the library window (in the MDI mode the one window); for tests
            [[nodiscard]] SDL_WindowID libraryWindowId() const
            {
                return library_window != nullptr ? library_window->id() : host_window->id();
            }
            /// The mode the program runs in; it changes only with a new start
            [[nodiscard]] WindowMode windowMode() const noexcept { return mode; }
            /// The mode the next start uses (View menu)
            [[nodiscard]] WindowMode nextWindowMode() const noexcept { return next_mode; }
            /// Keeps mode for the next start in the layout file, leaving the layouts saved there as they are; false
            /// without a layout file or if writing fails
            bool chooseWindowMode(WindowMode mode);
            /// The frame of the player at index in the MDI mode, nullptr in the other; for tests
            [[nodiscard]] const PaneFrame *deckFrame(std::size_t index) const { return decks.at(index)->frame.get(); }
            /// Where the part with the frame lies in the one window of the MDI mode; for tests
            [[nodiscard]] std::optional<Rect> paneArea(const PaneFrame &frame) const
            {
                return host_window != nullptr ? host_window->views().areaOf(frame) : std::nullopt;
            }
            /// The SDL id of the window of a tool window that is open; for tests
            [[nodiscard]] SDL_WindowID toolWindowId(Tool tool) const { return tools[index(tool)].window->id(); }
            /// The file loaded into the deck at index, empty if none; for tests
            [[nodiscard]] const std::filesystem::path &deckFile(std::size_t index) const
            {
                return decks.at(index)->loaded_file;
            }
            /// The Cue-Load option of the deck at index; for tests
            void setDeckCueLoad(std::size_t index, bool on) { decks.at(index)->view->setCueLoad(on); }
            /// Whether the deck at index plays, as of the last frame; for tests
            [[nodiscard]] bool deckPlaying(std::size_t index) const { return decks.at(index)->view->playing(); }
            [[nodiscard]] const DeckView &deckView(std::size_t index) const { return *decks.at(index)->view; }

            [[nodiscard]] bool toolWindowOpen(Tool tool) const noexcept
            {
                return tools[index(tool)].window != nullptr || tools[index(tool)].in_host;
            }
            /// Opens or closes a tool window at the end of the next frame
            void requestToolWindow(Tool tool, bool open);

            /**
             * The windows as they are now: players with their crossfader sides, the library, the crossfader and the
             * playlist window. What View > Save layout writes.
             */
            [[nodiscard]] Layout currentLayout() const;
            /// Writes currentLayout() to the layout file; false without a layout file or if writing fails
            bool saveLayout();

        private:
            /// SDL_Init and TTF_Init, undone in reverse
            struct SdlLibraries
            {
                    SdlLibraries();
                    SdlLibraries(const SdlLibraries &)            = delete;
                    SdlLibraries &operator=(const SdlLibraries &) = delete;
                    SdlLibraries(SdlLibraries &&)                 = delete;
                    SdlLibraries &operator=(SdlLibraries &&)      = delete;
                    ~SdlLibraries();

                    bool sdl_initialized = false;
                    bool ttf_initialized = false;
            };

            /// A deck and its window
            struct DeckWindow
            {
                    core::api::DeckId         id{};
                    int                       number = 0; ///< shown in the title
                    std::unique_ptr<DeckView> view;
                    /// MDI mode: the view with its title bar, as the one window shows it
                    std::unique_ptr<PaneFrame> frame;
                    SdlWindow                 *window = nullptr; ///< owned by windows; in the MDI mode the one window
                    /// The file loaded into the deck; its tags are not changed while it is loaded
                    std::filesystem::path loaded_file;
                    /// The library track loaded, 0 for a file from outside the library; its cue point is stored
                    std::int64_t track_id = 0;
                    /// The tempo and waveform of the track are in the view (the analysis was done when it was read)
                    bool analysis_shown = false;
            };

            enum class Request : std::uint8_t
            {
                None,
                Open,
                Close
            };

            /// A window that opens and closes on request; its view lives on
            struct ToolWindow
            {
                    View            *view   = nullptr;
                    SdlWindow       *window = nullptr;
                    std::string_view title_key;
                    int              width   = 0;
                    int              height  = 0;
                    Request          request = Request::None;
                    /// Where the window was or is to be; std::nullopt: next to the library window
                    std::optional<Placement> placement{};
                    /// MDI mode: the view with its title bar, made when it first opens
                    std::unique_ptr<PaneFrame> frame{};
                    /// MDI mode: shown in the one window
                    bool in_host = false;
            };

            static constexpr std::size_t index(Tool tool) noexcept { return static_cast<std::size_t>(tool); }

            explicit SdlApp(core::api::Core &core);

            /// Opens the window of a deck the core has; false (with a message in the log) if that fails
            /// where: the placement the window opens at; without it the window is cascaded from the one before
            bool openDeckWindow(core::api::DeckId deck, std::optional<Placement> where = std::nullopt);
            /// The saved layout, or the default one when there is none (or it is damaged)
            [[nodiscard]] Layout readLayout() const;
            /// The layout in the layout file; std::nullopt if there is none or it is damaged
            [[nodiscard]] std::optional<Layout> readSavedLayout() const;
            [[nodiscard]] bool                  writeLayout(const Layout &layout) const;
            /// MDI mode: opens the one window, on the main display
            bool openHost();
            /// MDI mode: gives every part shown its area in the one window, for the window's size now
            void arrangeHost();
            /// The placement moved inside the work area of the display it lies on
            [[nodiscard]] Placement fitToDisplay(const Placement &place) const;
            /// Adds a deck to the core and opens its window
            void addDeck(std::optional<Placement> where = std::nullopt);
            /// Closes the window of a deck, then removes the deck from the core
            void                      closeDeck(core::api::DeckId deck);
            [[nodiscard]] DeckWindow *deckWindow(core::api::DeckId deck) noexcept;
            [[nodiscard]] DeckWindow *deckOfWindow(SDL_WindowID window) noexcept;
            void                      setWindowTitles();
            [[nodiscard]] std::string deckTitle(int number) const;

            /// The Load button of a player: the track selected in the library goes into that player
            void loadSelectedInto(core::api::DeckId deck);
            /// Loads into a player that does not play; false (the library says why) if not
            bool loadInto(DeckWindow &target, const core::api::LibraryTrack &track);
            /// Takes the tempo and waveform of the deck's track from the library, if it is analyzed
            void showAnalysis(DeckWindow &target);
            /// Once the library's revision changed: shows analyses that finished since
            void                                 refreshAnalyses();
            [[nodiscard]] std::vector<MixerDeck> mixerDecks() const;

            void setLanguage(Language language);
            void runPlaylistCommand(LibraryView::PlaylistCommand command);
            void addToPlaylist(const core::api::LibraryTrack &track);
            /// Tool windows open and close between frames, never while the windows are being drawn
            void openTool(Tool tool);
            void closeTool(Tool tool);
            void applyWindowRequests();
            void processEvents(bool &running);

            SdlLibraries          libraries;
            SdlFileDialogs        dialogs;
            Translator            translator;
            LanguageChanged       language_changed;
            core::api::Core      *core;
            core::api::Library   *library = nullptr;
            std::filesystem::path layout_file; ///< where View > Save layout writes; empty: no layout is kept
            std::uint64_t         seen_library_revision = 0;
            /// Views come before the windows that show them: destroyed after them
            std::vector<std::unique_ptr<DeckWindow>> decks;
            int                                      deck_counter = 0;
            std::unique_ptr<LibraryView>             library_view;
            std::unique_ptr<PaneFrame>               library_frame; ///< MDI mode only
            std::unique_ptr<PlaylistView>            playlist_view;
            std::unique_ptr<MixerView>               mixer_view;
            std::vector<std::unique_ptr<SdlWindow>>  windows;
            /// Window titles are set again when the language changes
            SdlWindow *library_window = nullptr;
            /// MDI mode: the one window with all parts; nullptr in the other mode
            SdlWindow                *host_window = nullptr;
            std::array<ToolWindow, 2> tools;
            WindowMode                mode      = WindowMode::Separate;
            WindowMode                next_mode = WindowMode::Separate;
            /// The layout read at the start: what is not shown now (the other mode) is saved as it was
            std::optional<Layout> stored_layout;
            /// Players whose close button was pressed (MDI mode); they go between frames
            std::vector<core::api::DeckId> decks_to_close;

            /// Asked for from a menu or the keyboard; done between frames
            int new_decks_requested = 0;
            /// A track that was chosen with no player open: it goes into the player opened for it
            std::optional<core::api::LibraryTrack> load_after_new_deck;
            std::filesystem::path                  font;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_APP_HPP
