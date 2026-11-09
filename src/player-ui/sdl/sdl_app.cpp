// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_app.hpp"

#include "sdl_canvas.hpp"
#include "sdl_events.hpp"

#include <SDL3/SDL.h>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr int    initial_width  = 360;
        constexpr int    initial_height = 640;
        constexpr Uint32 frame_ms       = 16; // fallback pacing without vsync

        constexpr Color background{20, 20, 20};

        std::string sdlError(const char *what)
        {
            return std::string{what} + ": " + SDL_GetError();
        }
    } // namespace

    SdlApp::SdlLibrary::SdlLibrary()
    {
        // Touch is handled as touch; no extra mouse events for it
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        initialized = SDL_Init(SDL_INIT_VIDEO);
    }

    SdlApp::SdlLibrary::~SdlLibrary()
    {
        if (initialized)
        {
            SDL_Quit();
        }
    }

    std::expected<std::unique_ptr<SdlApp>, std::string> SdlApp::create(core::api::Core &core, core::api::DeckId deck)
    {
        core::api::DeckSnapshotBuffer *snapshot = core.deckSnapshot(deck);
        if (snapshot == nullptr)
        {
            return std::unexpected{std::string{"unknown deck"}};
        }

        std::unique_ptr<SdlApp> app{new SdlApp{core, deck, *snapshot}};
        if (!app->library.initialized)
        {
            return std::unexpected{sdlError("cannot initialize SDL")};
        }

        app->window.reset(SDL_CreateWindow("djw", initial_width, initial_height,
                                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
        if (!app->window)
        {
            return std::unexpected{sdlError("cannot create window")};
        }
        app->renderer.reset(SDL_CreateRenderer(app->window.get(), nullptr));
        if (!app->renderer)
        {
            return std::unexpected{sdlError("cannot create renderer")};
        }
        app->vsync = SDL_SetRenderVSync(app->renderer.get(), 1);
        return app;
    }

    SdlApp::SdlApp(core::api::Core &core, core::api::DeckId deck, core::api::DeckSnapshotBuffer &snapshot)
        : deck_view{deck, core.commandQueue(), snapshot}
    {
        deck_view.addTo(router);
    }

    void SdlApp::run()
    {
        bool running = true;
        while (running)
        {
            deck_view.layout(windowArea());
            processEvents(running);
            deck_view.update();
            drawFrame();
            if (!vsync)
            {
                SDL_Delay(frame_ms);
            }
        }
    }

    Rect SdlApp::windowArea() const
    {
        int width  = 0;
        int height = 0;
        SDL_GetWindowSize(window.get(), &width, &height);
        return {0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)};
    }

    void SdlApp::processEvents(bool &running)
    {
        const Rect area = windowArea();
        SDL_Event  event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            if (const auto pointer = toPointerEvent(event, area.width, area.height))
            {
                router.handle(*pointer);
            }
        }
    }

    void SdlApp::drawFrame()
    {
        // Layout is in points; the renderer works in pixels, which differ on HiDPI screens
        const float density = SDL_GetWindowPixelDensity(window.get());
        SDL_SetRenderScale(renderer.get(), density, density);

        SDL_SetRenderDrawColor(renderer.get(), background.red, background.green, background.blue, background.alpha);
        SDL_RenderClear(renderer.get());
        SdlCanvas canvas{*renderer};
        deck_view.draw(canvas);
        SDL_RenderPresent(renderer.get());
    }
} // namespace djw::player_ui::sdl
