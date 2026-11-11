// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_app.hpp"

#include "mdi_arrangement.hpp"
#include "player_number.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr Uint32 frame_ms = 16; // fallback pacing without vsync

        /// Ctrl+N makes a new deck; the Command key on a Mac
#ifdef __APPLE__
        constexpr SDL_Keymod new_deck_modifier = SDL_KMOD_GUI;
#else
        constexpr SDL_Keymod new_deck_modifier = SDL_KMOD_CTRL;
#endif

        bool initSdl()
        {
            // Touch is handled as touch; no extra mouse events for it
            SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
            return SDL_Init(SDL_INIT_VIDEO);
        }

        std::string sdlError(const char *what)
        {
            return std::string{what} + ": " + SDL_GetError();
        }

        /// The UI font, copied next to the binary at build time
        std::filesystem::path fontFile()
        {
            const char *base = SDL_GetBasePath();
            return std::filesystem::path{base != nullptr ? base : ""} / "fonts" / "NotoSans-Regular.ttf";
        }

        constexpr int player_width  = static_cast<int>(DeckView::design_width);
        constexpr int player_height = static_cast<int>(DeckView::design_height);

        /// The size a window opens at when nothing else is known
        struct WindowSize
        {
                int width  = 0;
                int height = 0;
        };
        constexpr WindowSize library_size{720, 640};
        constexpr WindowSize mixer_size{440, 280};
        constexpr WindowSize playlist_size{560, 640};

        /// The part of the display the windows may use (without panels and docks); std::nullopt if unknown
        std::optional<Placement> usableBounds(SDL_DisplayID display)
        {
            SDL_Rect usable{};
            if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable) || usable.w <= 0 || usable.h <= 0)
            {
                return std::nullopt;
            }
            return Placement{usable.x, usable.y, usable.w, usable.h};
        }

        /// A window title as the title bar of a part of the one window: without the program name in front, which the
        /// window shows once
        std::string paneTitle(std::string title)
        {
            constexpr std::string_view program = "djw - ";
            if (title.starts_with(program))
            {
                title.erase(0, program.size());
            }
            return title;
        }

        /// An error worth a line in the log, e.g. a window that cannot be opened
        void logError(const std::string &message)
        {
            SDL_Log("djw: %s", message.c_str()); // NOLINT(cppcoreguidelines-pro-type-vararg): SDL logs printf-style
        }
    } // namespace

    SdlApp::SdlLibraries::SdlLibraries() : sdl_initialized{initSdl()}, ttf_initialized{sdl_initialized && TTF_Init()} {}

    SdlApp::SdlLibraries::~SdlLibraries()
    {
        if (ttf_initialized)
        {
            TTF_Quit();
        }
        if (sdl_initialized)
        {
            SDL_Quit();
        }
    }

    std::expected<std::unique_ptr<SdlApp>, std::string> SdlApp::create(core::api::Core &core, core::api::DeckId deck,
                                                                       core::api::Library *library, Language language,
                                                                       LanguageChanged       language_changed,
                                                                       std::filesystem::path layout_file)
    {
        if (core.deckSnapshot(deck) == nullptr)
        {
            return std::unexpected{std::string{"unknown deck"}};
        }

        std::unique_ptr<SdlApp> app{new SdlApp{core}};
        app->translator.setLanguage(language);
        app->language_changed = std::move(language_changed);
        if (!app->libraries.sdl_initialized)
        {
            return std::unexpected{sdlError("cannot initialize SDL")};
        }
        if (!app->libraries.ttf_initialized)
        {
            return std::unexpected{sdlError("cannot initialize SDL_ttf")};
        }

        const std::filesystem::path &font = app->font = fontFile();
        // With a layout file the program opens as the user left it, or with two players and the crossfader window
        app->layout_file = std::move(layout_file);
        std::optional<Layout> layout;
        if (!app->layout_file.empty())
        {
            layout = app->readLayout();
        }
        app->stored_layout = layout;
        // Without a layout file nothing could be kept, not even the mode: then the separate windows, which need no
        // layout
        app->mode = app->next_mode = layout ? layout->mode : WindowMode::Separate;
        const bool      mdi        = app->mode == WindowMode::Mdi;
        const MdiLayout mdi_start  = mdi && layout ? layout->mdi.value_or(defaultMdiLayout()) : MdiLayout{};
        if (mdi && !app->openHost(mdi_start.fullscreen))
        {
            return std::unexpected{std::string{"cannot open the window"}};
        }
        app->hand_places = mdi_start.places;
        if (!app->openDeckWindow(deck, layout && !mdi ? std::optional{app->fitToDisplay(layout->players.front().place)}
                                                      : std::nullopt))
        {
            return std::unexpected{std::string{"cannot open the window of the deck"}};
        }

        SdlApp *owner = app.get();
        app->library  = library;
        // The crossfader window works without a library
        app->mixer_view = std::make_unique<MixerView>(core.commandQueue(), core.mixerSnapshot(), app->translator,
                                                      [owner] { return owner->mixerDecks(); });
        app->tools[index(Tool::Mixer)] = {.view      = app->mixer_view.get(),
                                          .window    = nullptr,
                                          .title_key = "window.mixer",
                                          .width     = mixer_size.width,
                                          .height    = mixer_size.height};

        if (library != nullptr)
        {
            app->library_view = std::make_unique<LibraryView>(
                *library, app->dialogs, app->translator,
                [owner](const core::api::LibraryTrack &track) { owner->load(track); },
                [owner](const std::filesystem::path &file)
                {
                    return std::ranges::any_of(owner->decks, [&file](const auto &deck_window)
                                               { return deck_window->loaded_file == file; });
                });
            app->library_view->setOnNewDeck([owner] { ++owner->new_decks_requested; });
            app->library_view->setOnLanguage([owner](Language chosen) { owner->setLanguage(chosen); });
            app->library_view->setPlaylistCommands([owner](LibraryView::PlaylistCommand command)
                                                   { owner->runPlaylistCommand(command); },
                                                   [owner] { return owner->toolWindowOpen(Tool::Playlist); });
            app->library_view->setMixerWindow(
                [owner] { owner->requestToolWindow(Tool::Mixer, !owner->toolWindowOpen(Tool::Mixer)); },
                [owner] { return owner->toolWindowOpen(Tool::Mixer); });
            if (!app->layout_file.empty())
            {
                app->library_view->setWindowMode([owner](WindowMode chosen) { return owner->chooseWindowMode(chosen); },
                                                 [owner] { return owner->nextWindowMode(); });
            }
            if (mdi)
            {
                app->library_frame = std::make_unique<PaneFrame>(
                    *app->library_view, [owner] { return paneTitle(owner->translator.tr("window.library")); });
                app->host_window->views().add(*app->library_frame);
                // The menus go to the top of the window, over everything, and drop down over the whole window
                app->menu_strip = std::make_unique<MenuStrip>(app->library_view->moveMenuBarOut(),
                                                              [owner] { return owner->host_window->area(); });
                app->host_window->views().add(*app->menu_strip, Rect{}, true);
                app->library_view->setFullscreen([owner] { owner->setFullscreen(!owner->fullscreen()); },
                                                 [owner] { return owner->fullscreen(); });
                app->library_view->setLayoutEditing([owner] { owner->startArranging(); },
                                                    [owner] { return owner->arranging(); },
                                                    [owner] { owner->arrangeAutomatically(); });
            }
            else
            {
                auto library_window = SdlWindow::create({.title  = app->translator.tr("window.library"),
                                                         .width  = library_size.width,
                                                         .height = library_size.height},
                                                        *app->library_view, font);
                if (!library_window)
                {
                    return std::unexpected{library_window.error()};
                }
                if (layout)
                {
                    (*library_window)->place(app->fitToDisplay(layout->library));
                }
                else
                {
                    (*library_window)->placeRightOf(*app->decks.front()->window);
                }
                app->library_window = library_window->get();
                app->windows.push_back(std::move(*library_window));
            }

            // The view lives on; only its window comes and goes
            app->playlist_view =
                std::make_unique<PlaylistView>(*library, app->dialogs, app->translator,
                                               [owner](const core::api::LibraryTrack &track) { owner->load(track); });
            app->tools[index(Tool::Playlist)] = {.view      = app->playlist_view.get(),
                                                 .window    = nullptr,
                                                 .title_key = "window.playlist",
                                                 .width     = playlist_size.width,
                                                 .height    = playlist_size.height};
            app->library_view->setAddToPlaylist([owner](const core::api::LibraryTrack &track)
                                                { owner->addToPlaylist(track); });
            app->dialogs.setParent(mdi ? app->host_window->handle() : app->library_window->handle());
            app->library_view->setSaveLayout([owner] { return owner->saveLayout(); });
        }

        if (layout)
        {
            // The other players, the crossfader sides and the tool windows, as the mode in use left them
            std::vector<core::api::CrossfaderSide> sides = mdi_start.players;
            if (!mdi)
            {
                sides.clear();
                for (std::size_t player = 0; player < layout->players.size(); ++player)
                {
                    sides.push_back(layout->players[player].side);
                    if (player > 0)
                    {
                        app->addDeck(app->fitToDisplay(layout->players[player].place));
                    }
                }
                app->tools[index(Tool::Mixer)].placement    = app->fitToDisplay(layout->mixer.place);
                app->tools[index(Tool::Playlist)].placement = app->fitToDisplay(layout->playlist.place);
            }
            for (std::size_t player = 1; mdi && player < sides.size(); ++player)
            {
                app->addDeck();
            }
            for (std::size_t player = 0; player < sides.size() && player < app->decks.size(); ++player)
            {
                const core::api::MixerAssignCrossfader assign{app->decks[player]->id, sides[player]};
                static_cast<void>(core.commandQueue().tryPush(assign));
            }
            if (mdi ? mdi_start.mixer_open : layout->mixer.open)
            {
                app->openTool(Tool::Mixer);
            }
            if (mdi ? mdi_start.playlist_open : layout->playlist.open)
            {
                app->openTool(Tool::Playlist);
            }
        }
        return app;
    }

    SdlApp::SdlApp(core::api::Core &core) : core{&core} {}

    SdlApp::DeckWindow *SdlApp::deckWindow(core::api::DeckId deck) noexcept
    {
        const auto found = std::ranges::find(decks, deck, &DeckWindow::id);
        return found != decks.end() ? found->get() : nullptr;
    }

    SdlApp::DeckWindow *SdlApp::deckOfWindow(SDL_WindowID window) noexcept
    {
        const auto found =
            std::ranges::find_if(decks, [window](const auto &deck) { return deck->window->id() == window; });
        return found != decks.end() ? found->get() : nullptr;
    }

    std::string SdlApp::deckTitle(int number) const
    {
        return translator.tr("window.deck", {{"number", playerNumber(number)}});
    }

    bool SdlApp::openHost(bool fullscreen)
    {
        auto created = SdlWindow::create({.title      = "djw",
                                          .width      = library_size.width,
                                          .height     = library_size.height,
                                          .vsync      = true,
                                          .fullscreen = fullscreen,
                                          .maximized  = !fullscreen},
                                         font);
        if (!created)
        {
            logError("cannot open the window: " + created.error());
            return false;
        }
        host_window     = created->get();
        host_fullscreen = fullscreen;
        windows.push_back(std::move(*created));
        return true;
    }

    void SdlApp::setFullscreen(bool on)
    {
        if (host_window == nullptr)
        {
            return;
        }
        host_fullscreen = on;
        host_window->setFullscreen(on);
    }

    Rect SdlApp::workArea() const
    {
        Rect area = host_window->area();
        if (menu_strip)
        {
            const float bar = std::min(MenuBar::bar_height, area.height);
            area.y += bar;
            area.height -= bar;
        }
        return area;
    }

    void SdlApp::arrangeHost()
    {
        // The menu bar across the top, the parts below it
        ViewHost &host = host_window->views();
        if (menu_strip)
        {
            const Rect window = host_window->area();
            host.place(*menu_strip,
                       Rect{window.x, window.y, window.width, std::min(MenuBar::bar_height, window.height)});
        }
        host.setWorkArea(workArea());
        if (hand_places)
        {
            arrangeByHand();
            return;
        }
        const ToolWindow    &mixer    = tools[index(Tool::Mixer)];
        const ToolWindow    &playlist = tools[index(Tool::Playlist)];
        const MdiArrangement placed   = arrangeMdi(workArea(),
                                                   {.players  = decks.size(),
                                                    .mixer    = mixer.in_host,
                                                    .playlist = playlist.in_host,
                                                    .library  = library_frame != nullptr},
                                                   {.player_width   = DeckView::design_width,
                                                    .player_height  = DeckView::design_height + PaneFrame::title_height,
                                                    .mixer_width    = static_cast<float>(mixer_size.width),
                                                    .playlist_width = static_cast<float>(playlist_size.width)});
        ViewHost            &views    = host_window->views();
        for (std::size_t deck = 0; deck < decks.size(); ++deck)
        {
            views.place(*decks[deck]->frame, placed.players[deck]);
        }
        if (mixer.in_host)
        {
            views.place(*mixer.frame, placed.mixer);
        }
        if (playlist.in_host)
        {
            views.place(*playlist.frame, placed.playlist);
        }
        if (library_frame)
        {
            views.place(*library_frame, placed.library);
        }
    }

    namespace
    {
        Rect rectOf(const Placement &place)
        {
            return {static_cast<float>(place.x), static_cast<float>(place.y), static_cast<float>(place.width),
                    static_cast<float>(place.height)};
        }

        Placement placementOf(const Rect &area)
        {
            return {static_cast<int>(std::lround(area.x)), static_cast<int>(std::lround(area.y)),
                    static_cast<int>(std::lround(area.width)), static_cast<int>(std::lround(area.height))};
        }

        /// Between the places set by hand (for the window size they were made in) and the window as it is now
        class PlaceScale
        {
            public:
                /// Places without a window size get the window's
                PlaceScale(MdiPlaces &places, const Rect &window) : window{window}
                {
                    if (places.area.width <= 0 || places.area.height <= 0)
                    {
                        places.area = {0, 0, static_cast<int>(std::lround(window.width)),
                                       static_cast<int>(std::lround(window.height))};
                    }
                    x_scale = window.width / static_cast<float>(places.area.width);
                    y_scale = window.height / static_cast<float>(places.area.height);
                }

                [[nodiscard]] Rect toWindow(const Placement &place) const
                {
                    const Rect area = rectOf(place);
                    return {window.x + (area.x * x_scale), window.y + (area.y * y_scale), area.width * x_scale,
                            area.height * y_scale};
                }

                [[nodiscard]] Placement fromWindow(const Rect &area) const
                {
                    return placementOf({(area.x - window.x) / x_scale, (area.y - window.y) / y_scale,
                                        area.width / x_scale, area.height / y_scale});
                }

            private:
                Rect  window;
                float x_scale = 1.0F;
                float y_scale = 1.0F;
        };
    } // namespace

    void SdlApp::arrangeByHand()
    {
        if (!hand_places)
        {
            return;
        }
        ViewHost        &views  = host_window->views();
        const Rect       window = workArea();
        MdiPlaces       &places = hand_places.value();
        const PlaceScale scale{places, window};
        places.players.resize(decks.size());
        const ToolWindow &mixer    = tools[index(Tool::Mixer)];
        const ToolWindow &playlist = tools[index(Tool::Playlist)];

        // Every part shown, with its place (if it has one) and the size it is made for
        struct Part
        {
                const View               *frame;
                std::optional<Placement> *place;
                float                     width;
                float                     height;
        };
        std::vector<Part> parts;
        parts.reserve(decks.size() + 3);
        for (std::size_t deck = 0; deck < decks.size(); ++deck)
        {
            parts.push_back({decks[deck]->frame.get(), &places.players[deck], DeckView::design_width,
                             DeckView::design_height + PaneFrame::title_height});
        }
        std::optional<Placement> library_place;
        if (library_frame)
        {
            library_place = places.library;
            parts.push_back({library_frame.get(), &library_place, static_cast<float>(library_size.width),
                             static_cast<float>(library_size.height)});
        }
        // The tools last: one that opens again gives way if its place was taken while it was closed
        if (mixer.in_host)
        {
            parts.push_back({mixer.frame.get(), &places.mixer, static_cast<float>(mixer_size.width),
                             static_cast<float>(mixer_size.height) + PaneFrame::title_height});
        }
        if (playlist.in_host)
        {
            parts.push_back({playlist.frame.get(), &places.playlist, static_cast<float>(playlist_size.width),
                             static_cast<float>(playlist_size.height)});
        }

        // The parts with a place first, kept inside the window; one whose place is taken by a part before it loses
        // it. Then a free place for the others, which they keep.
        std::vector<Rect> taken;
        for (const Part &part : parts)
        {
            if (const std::optional<Placement> stored = *part.place; stored)
            {
                const Rect area = keepInside(scale.toWindow(*stored), window);
                // Scaled, edges that touched may be a hair apart or over each other: only more counts
                if (std::ranges::any_of(taken, [&area](const Rect &other) { return overlap(area.inset(0.5F), other); }))
                {
                    part.place->reset();
                    continue;
                }
                views.place(*part.frame, area);
                taken.push_back(area);
            }
        }
        for (const Part &part : parts)
        {
            if (!*part.place)
            {
                // No room left anywhere: on top at the upper left, for the user to make room
                const Rect area = freePlace(part.width, part.height, window, taken)
                                      .value_or(keepInside({window.x, window.y, part.width, part.height}, window));
                *part.place     = scale.fromWindow(area);
                views.place(*part.frame, area);
                taken.push_back(area);
            }
        }
        if (library_place)
        {
            places.library = *library_place;
        }
    }

    void SdlApp::paneMoved(const View &frame, const Rect &area)
    {
        ViewHost  &views  = host_window->views();
        const Rect window = workArea();
        if (!hand_places)
        {
            // The first part moved by hand: all the others stay where the automatic arrangement put them, in a
            // window of the size it has now
            MdiPlaces        places;
            const PlaceScale now{places, window};
            const auto       where = [&views, &now](const PaneFrame *pane) -> std::optional<Placement>
            {
                if (pane == nullptr || !views.contains(*pane))
                {
                    return std::nullopt;
                }
                const auto shown = views.areaOf(*pane);
                return shown ? std::optional{now.fromWindow(*shown)} : std::nullopt;
            };
            for (const auto &deck : decks)
            {
                places.players.push_back(where(deck->frame.get()));
            }
            places.mixer    = where(tools[index(Tool::Mixer)].frame.get());
            places.playlist = where(tools[index(Tool::Playlist)].frame.get());
            places.library  = where(library_frame.get()).value_or(Placement{});
            hand_places     = std::move(places);
        }
        MdiPlaces &places = *hand_places;
        places.players.resize(decks.size());
        const Placement moved = PlaceScale{places, window}.fromWindow(area);
        for (std::size_t deck = 0; deck < decks.size(); ++deck)
        {
            if (decks[deck]->frame.get() == &frame)
            {
                places.players[deck] = moved;
            }
        }
        if (tools[index(Tool::Mixer)].frame.get() == &frame)
        {
            places.mixer = moved;
        }
        if (tools[index(Tool::Playlist)].frame.get() == &frame)
        {
            places.playlist = moved;
        }
        if (library_frame.get() == &frame)
        {
            places.library = moved;
        }
    }

    void SdlApp::startArranging()
    {
        if (host_window == nullptr)
        {
            return;
        }
        host_window->views().startArranging(
            {.moved      = [this](const View &frame, const Rect &area) { paneMoved(frame, area); },
             .done       = [this] { stopArranging(); },
             .done_label = [this] { return translator.tr("layout.done"); }});
    }

    void SdlApp::stopArranging()
    {
        if (host_window != nullptr)
        {
            host_window->views().stopArranging();
        }
    }

    void SdlApp::arrangeAutomatically()
    {
        hand_places.reset();
    }

    bool SdlApp::openDeckWindow(core::api::DeckId deck, std::optional<Placement> where)
    {
        core::api::DeckSnapshotBuffer *snapshot = core->deckSnapshot(deck);
        if (snapshot == nullptr)
        {
            return false;
        }
        auto opened    = std::make_unique<DeckWindow>();
        opened->id     = deck;
        opened->number = ++deck_counter;
        opened->view   = std::make_unique<DeckView>(deck, core->commandQueue(), *snapshot, translator);
        opened->view->setOnLoad([this, deck] { loadSelectedInto(deck); });
        // The cue point the user sets goes into the library, where the next load finds it
        opened->view->setOnCueSet(
            [this, deck](double seconds)
            {
                const DeckWindow *source = deckWindow(deck);
                if (library != nullptr && source != nullptr && source->track_id != 0)
                {
                    library->setCuePoint(source->track_id, seconds);
                }
            });
        if (host_window != nullptr)
        {
            // MDI mode: a part of the one window; the close button asks for it to go between frames
            opened->frame = std::make_unique<PaneFrame>(
                *opened->view, [this, number = opened->number] { return paneTitle(deckTitle(number)); },
                [this, deck] { decks_to_close.push_back(deck); });
            host_window->views().add(*opened->frame);
            opened->window = host_window;
            decks.push_back(std::move(opened));
            return true;
        }
        // One window paces the frames with the display: the first one, or the next if that one is gone
        const bool pace    = std::ranges::none_of(windows, [](const auto &window) { return window->paced(); });
        auto       created = SdlWindow::create(
            {.title = deckTitle(opened->number), .width = player_width, .height = player_height, .vsync = pace},
            *opened->view, font);
        if (!created)
        {
            logError("cannot open a deck window: " + created.error());
            return false;
        }
        if (where)
        {
            (*created)->place(*where);
        }
        else if (!decks.empty())
        {
            (*created)->cascadeFrom(*decks.back()->window); // not exactly on top of the other deck
        }
        opened->window = created->get();
        windows.push_back(std::move(*created));
        decks.push_back(std::move(opened));
        return true;
    }

    void SdlApp::addDeck(std::optional<Placement> where)
    {
        const core::api::DeckId deck = core->addDeck();
        if (!openDeckWindow(deck, where))
        {
            if (auto removed = core->removeDeck(deck); !removed)
            {
                logError(removed.error());
            }
        }
    }

    Placement SdlApp::fitToDisplay(const Placement &place) const
    {
        // The display the window's center lies on, else the main one
        const SDL_Point center{place.x + (place.width / 2), place.y + (place.height / 2)};
        SDL_DisplayID   display = SDL_GetDisplayForPoint(&center);
        if (display == 0)
        {
            display = SDL_GetPrimaryDisplay();
        }
        const auto usable = usableBounds(display);
        if (!usable)
        {
            return place; // no information about the screens: leave the window where it was
        }
        return fitInto(place, *usable);
    }

    std::optional<Layout> SdlApp::readSavedLayout() const
    {
        const std::ifstream file{layout_file};
        std::ostringstream  text;
        text << file.rdbuf();
        return parseLayout(text.str());
    }

    bool SdlApp::writeLayout(const Layout &layout) const
    {
        std::ofstream file{layout_file, std::ios::trunc};
        file << formatLayout(layout);
        return static_cast<bool>(file);
    }

    Layout SdlApp::readLayout() const
    {
        if (auto saved = readSavedLayout())
        {
            return std::move(*saved);
        }
        // Nothing saved, or damaged: the one window with two players and the crossfader between them; the separate
        // windows, should the user switch to them, the same on the main display
        const Placement   usable = usableBounds(SDL_GetPrimaryDisplay()).value_or(Placement{0, 0, 1920, 1080});
        const ToolWindow &mixer  = tools[index(Tool::Mixer)];
        Layout            first  = defaultLayout(
            usable, {player_width, player_height, mixer.width > 0 ? mixer.width : mixer_size.width,
                     mixer.height > 0 ? mixer.height : mixer_size.height, library_size.width, library_size.height});
        first.mode = WindowMode::Mdi;
        return first;
    }

    Layout SdlApp::currentLayout() const
    {
        // The mode not in use keeps what was read at the start
        Layout layout = stored_layout.value_or(Layout{});
        layout.mode   = next_mode;
        if (mode == WindowMode::Mdi)
        {
            MdiLayout shown{.players       = {},
                            .mixer_open    = toolWindowOpen(Tool::Mixer),
                            .playlist_open = toolWindowOpen(Tool::Playlist),
                            .fullscreen    = host_fullscreen,
                            .places        = hand_places};
            for (const auto &deck : decks)
            {
                shown.players.push_back(deck->view->crossfaderSide());
            }
            layout.mdi = std::move(shown);
            return layout;
        }
        layout.players.clear();
        for (const auto &deck : decks)
        {
            layout.players.push_back({deck->window->placement(), deck->view->crossfaderSide()});
        }
        layout.library  = library_window != nullptr ? library_window->placement()
                                                    : Placement{0, 0, library_size.width, library_size.height};
        const auto tool = [this](Tool which, int default_width, int default_height)
        {
            const ToolWindow &entry = tools[index(which)];
            if (entry.window != nullptr)
            {
                return ToolPlacement{.open = true, .place = entry.window->placement()};
            }
            return ToolPlacement{.open  = false,
                                 .place = entry.placement.value_or(Placement{0, 0, std::max(entry.width, default_width),
                                                                             std::max(entry.height, default_height)})};
        };
        layout.mixer    = tool(Tool::Mixer, mixer_size.width, mixer_size.height);
        layout.playlist = tool(Tool::Playlist, playlist_size.width, playlist_size.height);
        return layout;
    }

    bool SdlApp::saveLayout()
    {
        return !layout_file.empty() && writeLayout(currentLayout());
    }

    bool SdlApp::chooseWindowMode(WindowMode chosen)
    {
        next_mode = chosen;
        if (layout_file.empty())
        {
            return false;
        }
        // Only the mode changes; without a saved layout what is shown now is saved with it
        Layout layout = readSavedLayout().value_or(currentLayout());
        layout.mode   = chosen;
        return writeLayout(layout);
    }

    bool SdlApp::anyPlaying() const
    {
        return std::ranges::any_of(decks, [](const auto &candidate) { return candidate->view->playing(); });
    }

    void SdlApp::closeDeck(core::api::DeckId deck)
    {
        const DeckWindow *closing = deckWindow(deck);
        if (closing == nullptr || closing->view->playing())
        {
            return; // a player that plays is not closed
        }
        if (host_window != nullptr)
        {
            // MDI mode: the part goes, the one window stays; the places set by hand stay with the other players
            if (hand_places)
            {
                const auto at =
                    static_cast<std::size_t>(std::ranges::find(decks, deck, &DeckWindow::id) - decks.begin());
                if (at < hand_places->players.size())
                {
                    hand_places->players.erase(hand_places->players.begin() + static_cast<std::ptrdiff_t>(at));
                }
            }
            host_window->views().remove(*closing->frame);
            std::erase_if(decks, [deck](const auto &candidate) { return candidate->id == deck; });
            if (auto removed = core->removeDeck(deck); !removed)
            {
                logError(removed.error());
            }
            return;
        }
        // The window and the view go first: the view reads the deck's snapshot, which the core frees with the deck
        const SdlWindow *window = closing->window;
        std::erase_if(windows, [window](const auto &candidate) { return candidate.get() == window; });
        std::erase_if(decks, [deck](const auto &candidate) { return candidate->id == deck; });
        if (auto removed = core->removeDeck(deck); !removed)
        {
            logError(removed.error());
        }
        // The window that paced the frames may be gone
        if (std::ranges::none_of(windows, [](const auto &candidate) { return candidate->paced(); }) && !decks.empty())
        {
            decks.front()->window->setPaced(true);
        }
    }

    void SdlApp::setWindowTitles()
    {
        // In the MDI mode the title bars ask for their texts themselves
        for (const auto &deck : decks)
        {
            if (deck->window != host_window)
            {
                deck->window->setTitle(deckTitle(deck->number));
            }
        }
        if (library_window != nullptr)
        {
            library_window->setTitle(translator.tr("window.library"));
        }
        for (const ToolWindow &tool : tools)
        {
            if (tool.window != nullptr)
            {
                tool.window->setTitle(translator.tr(tool.title_key));
            }
        }
    }

    void SdlApp::setDeckTrack(std::filesystem::path file, std::string title)
    {
        if (decks.empty())
        {
            return;
        }
        decks.front()->loaded_file = std::move(file);
        decks.front()->track_id    = 0;
        decks.front()->view->setTrack({}, std::move(title));
    }

    bool SdlApp::loadInto(DeckWindow &target, const core::api::LibraryTrack &track)
    {
        if (target.view->playing())
        {
            target.view->setNotice("player.notice.playing");
            if (library_view)
            {
                library_view->setNotice("library.notice.player_playing");
            }
            return false; // what plays is not replaced
        }
        // Cue-Load: start at the cue point stored for the track, else at its first sound, else at the beginning
        core::api::LoadOptions options;
        if (target.view->cueLoad())
        {
            options = {.cue_seconds = track.cue_seconds, .cue_to_first_sound = true};
        }
        if (auto loaded = core->loadTrack(target.id, track.path, options); !loaded)
        {
            target.view->setMessage(translator.tr("deck.cannot_load", {{"error", loaded.error()}}));
            return false;
        }
        target.loaded_file = track.path;
        target.track_id    = track.id;
        target.view->setWaveform({});
        target.view->setBpm(std::nullopt);
        target.analysis_shown = false;
        if (library != nullptr && track.id != 0)
        {
            library->analyze(track.id); // tempo and waveform, unless the track was analyzed before
            showAnalysis(target);
        }
        // A file without tags shows its name
        const std::string title = track.title.empty() ? track.path.stem().string() : track.title;
        target.view->setTrack(track.artist, title);
        return true;
    }

    void SdlApp::showAnalysis(DeckWindow &target)
    {
        const auto track = library->track(target.track_id);
        if (!track || !track->analyzed)
        {
            return;
        }
        target.view->setBpm(track->bpm);
        target.view->setWaveform(library->waveform(track->id));
        target.analysis_shown = true;
    }

    void SdlApp::refreshAnalyses()
    {
        // The analysis of a track just loaded finishes in the background: the library's revision tells when to look
        const std::uint64_t revision = library->status().revision;
        if (revision == seen_library_revision)
        {
            return;
        }
        seen_library_revision = revision;
        for (const auto &deck : decks)
        {
            if (deck->track_id != 0 && !deck->analysis_shown)
            {
                showAnalysis(*deck);
            }
        }
    }

    void SdlApp::dropped(SDL_WindowID window, const std::filesystem::path &path, std::optional<Point> position)
    {
        DeckWindow *deck       = nullptr;
        bool        on_library = false;
        if (host_window != nullptr && window == host_window->id())
        {
            // The one window of the MDI mode: the part under the position
            if (!position)
            {
                return;
            }
            const auto under = [this, &position](const View &part)
            {
                const auto area = host_window->views().areaOf(part);
                return area && area->contains(*position);
            };
            const auto found =
                std::ranges::find_if(decks, [&under](const auto &candidate) { return under(*candidate->frame); });
            deck       = found != decks.end() ? found->get() : nullptr;
            on_library = library_frame && under(*library_frame);
        }
        else
        {
            deck       = deckOfWindow(window);
            on_library = library_window != nullptr && window == library_window->id();
        }
        if (deck != nullptr)
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error))
            {
                return;
            }
            // A file from outside the library, shown with its name; the tags are not read
            loadInto(*deck, core::api::LibraryTrack{.path = path});
            return;
        }
        if (library != nullptr && on_library)
        {
            std::error_code error;
            if (std::filesystem::is_directory(path, error))
            {
                if (auto added = library->addFolder(path); !added)
                {
                    logError(added.error());
                }
            }
        }
    }

    void SdlApp::load(const core::api::LibraryTrack &track)
    {
        if (decks.empty())
        {
            // Opening a window now could disturb the frame in progress: the track follows the new player
            load_after_new_deck = track;
            ++new_decks_requested;
            return;
        }
        const auto free_deck = std::ranges::find_if(decks, [](const auto &deck) { return !deck->view->playing(); });
        if (free_deck == decks.end())
        {
            if (library_view)
            {
                library_view->setNotice("library.notice.all_playing");
            }
            return;
        }
        loadInto(**free_deck, track);
    }

    void SdlApp::loadSelectedInto(core::api::DeckId deck)
    {
        if (!library_view)
        {
            return;
        }
        DeckWindow *target = deckWindow(deck);
        if (target == nullptr)
        {
            return;
        }
        const auto track = library_view->selectedTrack();
        if (!track)
        {
            library_view->setNotice("library.notice.select_first");
            return;
        }
        loadInto(*target, *track);
    }

    std::vector<MixerDeck> SdlApp::mixerDecks() const
    {
        std::vector<MixerDeck> result;
        result.reserve(decks.size());
        for (const auto &deck : decks)
        {
            result.push_back({deck->id, deck->number, deck->view->crossfaderSide()});
        }
        return result;
    }

    void SdlApp::setLanguage(Language language)
    {
        if (language == translator.language())
        {
            return;
        }
        translator.setLanguage(language);
        setWindowTitles();
        if (language_changed)
        {
            language_changed(language);
        }
    }

    void SdlApp::openTool(Tool tool)
    {
        ToolWindow &which = tools[index(tool)];
        if (which.view == nullptr || which.window != nullptr || which.in_host)
        {
            return;
        }
        if (host_window != nullptr)
        {
            // MDI mode: a part of the one window
            if (!which.frame)
            {
                which.frame = std::make_unique<PaneFrame>(
                    *which.view, [this, key = which.title_key] { return paneTitle(translator.tr(key)); },
                    [this, tool] { requestToolWindow(tool, false); });
            }
            host_window->views().add(*which.frame);
            which.in_host = true;
            return;
        }
        auto created = SdlWindow::create(
            {.title = translator.tr(which.title_key), .width = which.width, .height = which.height}, *which.view, font);
        if (!created)
        {
            logError("cannot open a window: " + created.error());
            return;
        }
        if (which.placement)
        {
            (*created)->place(*which.placement);
        }
        else if (library_window != nullptr)
        {
            (*created)->placeRightOf(*library_window);
        }
        which.window = created->get();
        windows.push_back(std::move(*created));
    }

    void SdlApp::closeTool(Tool tool)
    {
        ToolWindow &which = tools[index(tool)];
        if (which.in_host)
        {
            host_window->views().remove(*which.frame);
            which.in_host = false;
            return;
        }
        if (which.window == nullptr)
        {
            return;
        }
        const SdlWindow *window = which.window;
        which.placement         = window->placement(); // it opens there again
        std::erase_if(windows, [window](const auto &candidate) { return candidate.get() == window; });
        which.window = nullptr;
    }

    void SdlApp::requestToolWindow(Tool tool, bool open)
    {
        tools[index(tool)].request = open ? Request::Open : Request::Close;
    }

    void SdlApp::setSettingsFile(std::filesystem::path file)
    {
        settings_file = std::move(file);
        if (library_view && !settings_file.empty())
        {
            library_view->setDevicesDialog([this] { openDevicesDialog(); });
        }
    }

    void SdlApp::openDevicesDialog()
    {
        SdlWindow *over = host_window != nullptr ? host_window : library_window;
        if (devices_dialog || over == nullptr || settings_file.empty())
        {
            return;
        }
        const std::ifstream file{settings_file};
        std::ostringstream  text;
        text << file.rdbuf();
        devices_dialog = std::make_unique<DevicesDialog>(translator, parseSettings(text.str()), core->sampleRate());
        dialog_window  = over;
        over->views().add(*devices_dialog, std::nullopt, true); // over everything, modal
        over->views().setFocus(*devices_dialog);
    }

    void SdlApp::applyWindowRequests()
    {
        if (devices_dialog)
        {
            if (const auto outcome = devices_dialog->takeOutcome())
            {
                if (*outcome == DevicesDialog::Outcome::Accepted)
                {
                    std::ofstream file{settings_file, std::ios::trunc};
                    file << formatSettings(devices_dialog->chosen());
                    if (!file)
                    {
                        logError("cannot write the settings to " + settings_file.string());
                    }
                }
                dialog_window->views().remove(*devices_dialog);
                devices_dialog.reset();
                dialog_window = nullptr;
            }
        }
        for (const core::api::DeckId deck : std::exchange(decks_to_close, {}))
        {
            closeDeck(deck);
        }
        for (const Tool tool : {Tool::Playlist, Tool::Mixer})
        {
            switch (std::exchange(tools[index(tool)].request, Request::None))
            {
                case Request::Open:
                    openTool(tool);
                    break;
                case Request::Close:
                    closeTool(tool);
                    break;
                case Request::None:
                    break;
            }
        }
        for (int count = std::exchange(new_decks_requested, 0); count > 0; --count)
        {
            addDeck();
        }
        // A track chosen with no player open goes into the one just opened
        if (load_after_new_deck && !decks.empty())
        {
            const core::api::LibraryTrack track = std::move(*load_after_new_deck);
            load_after_new_deck.reset();
            loadInto(*decks.back(), track);
        }
    }

    void SdlApp::addToPlaylist(const core::api::LibraryTrack &track)
    {
        if (!playlist_view)
        {
            return;
        }
        if (!toolWindowOpen(Tool::Playlist))
        {
            playlist_view->update(); // reads the playlists, which the window would do on its first frame
            requestToolWindow(Tool::Playlist, true);
        }
        playlist_view->addTrack(track);
    }

    void SdlApp::runPlaylistCommand(LibraryView::PlaylistCommand command)
    {
        if (!playlist_view)
        {
            return;
        }
        if (command == LibraryView::PlaylistCommand::ToggleWindow)
        {
            requestToolWindow(Tool::Playlist, !toolWindowOpen(Tool::Playlist));
            return;
        }
        // The commands show their result in the playlist window, so it opens if it is not
        if (!toolWindowOpen(Tool::Playlist))
        {
            playlist_view->update();
            requestToolWindow(Tool::Playlist, true);
        }
        switch (command)
        {
            case LibraryView::PlaylistCommand::ToggleWindow:
                break;
            case LibraryView::PlaylistCommand::New:
                playlist_view->createNew();
                break;
            case LibraryView::PlaylistCommand::Import:
                playlist_view->importFile();
                break;
            case LibraryView::PlaylistCommand::Export:
                playlist_view->exportShown();
                break;
        }
    }

    void SdlApp::run()
    {
        while (runFrame())
        {
            if (std::ranges::none_of(windows, [](const auto &window) { return window->paced(); }))
            {
                SDL_Delay(frame_ms);
            }
        }
    }

    bool SdlApp::runFrame()
    {
        bool running = true;
        processEvents(running);
        if (!running || (decks.empty() && !library_view))
        {
            return false; // a quit, or the last deck window was closed and there is no library to work in
        }
        dialogs.deliver();
        if (library != nullptr)
        {
            refreshAnalyses();
        }
        if (host_window != nullptr)
        {
            arrangeHost(); // the parts follow the window's size and what is open
        }
        for (const auto &window : windows)
        {
            window->frame();
        }
        applyWindowRequests(); // not inside the loop above, which would invalidate it
        return true;
    }

    void SdlApp::processEvents(bool &running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                (host_window == nullptr || event.window.windowID != host_window->id()))
            {
                if (const DeckWindow *closing = deckOfWindow(event.window.windowID))
                {
                    closeDeck(closing->id); // only this deck; the program goes on
                    continue;
                }
                const auto tool = std::ranges::find_if(
                    tools, [&event](const ToolWindow &candidate)
                    { return candidate.window != nullptr && candidate.window->id() == event.window.windowID; });
                if (tool != tools.end())
                {
                    closeTool(static_cast<Tool>(tool - tools.begin())); // only this window; the program goes on
                    continue;
                }
            }
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                // The main window (or a quit) ends the program, but not while a player plays
                if (!anyPlaying())
                {
                    running = false;
                }
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_N &&
                (event.key.mod & new_deck_modifier) != 0)
            {
                ++new_decks_requested; // Ctrl+N in any window
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F &&
                (event.key.mod & SDL_KMOD_CTRL) != 0 && host_window != nullptr)
            {
                setFullscreen(!host_fullscreen); // Ctrl+F: the one window between fullscreen and a normal window
                continue;
            }
            if (event.type == SDL_EVENT_DROP_FILE)
            {
                if (event.drop.data != nullptr)
                {
                    dropped(event.drop.windowID, std::filesystem::path{event.drop.data},
                            Point{event.drop.x, event.drop.y});
                }
                continue;
            }
            SDL_Window *target = SDL_GetWindowFromEvent(&event);
            if (target == nullptr)
            {
                continue;
            }
            const SDL_WindowID id = SDL_GetWindowID(target);
            const auto found = std::ranges::find_if(windows, [id](const auto &window) { return window->id() == id; });
            if (found != windows.end())
            {
                (*found)->handle(event);
            }
        }
    }
} // namespace djw::player_ui::sdl
