// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_window.hpp"

#include "sdl_canvas.hpp"
#include "sdl_events.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_keyboard.h>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr Color background{20, 20, 20};

        std::string sdlError(const char *what)
        {
            return std::string{what} + ": " + SDL_GetError();
        }
    } // namespace

    std::expected<std::unique_ptr<SdlWindow>, std::string> SdlWindow::create(const WindowOptions &options, View &view,
                                                                             const std::filesystem::path &font_file)
    {
        std::unique_ptr<SdlWindow> created{new SdlWindow{view}};
        created->window.reset(SDL_CreateWindow(options.title.c_str(), options.width, options.height,
                                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
        if (!created->window)
        {
            return std::unexpected{sdlError("cannot create window")};
        }
        created->renderer.reset(SDL_CreateRenderer(created->window.get(), nullptr));
        if (!created->renderer)
        {
            return std::unexpected{sdlError("cannot create renderer")};
        }
        created->vsync = options.vsync && SDL_SetRenderVSync(created->renderer.get(), 1);

        auto text = SdlText::open(*created->renderer, font_file);
        if (!text)
        {
            return std::unexpected{text.error()};
        }
        created->text = std::move(*text);

        view.addTo(created->router);
        return created;
    }

    void SdlWindow::placeRightOf(const SdlWindow &other)
    {
        constexpr int spacing = 16;
        int           x       = 0;
        int           y       = 0;
        int           width   = 0;
        int           height  = 0;
        SDL_GetWindowPosition(other.window.get(), &x, &y);
        SDL_GetWindowSize(other.window.get(), &width, &height);
        SDL_SetWindowPosition(window.get(), x + width + spacing, y);
    }

    void SdlWindow::cascadeFrom(const SdlWindow &other)
    {
        constexpr int offset = 40;
        int           x      = 0;
        int           y      = 0;
        SDL_GetWindowPosition(other.window.get(), &x, &y);
        SDL_SetWindowPosition(window.get(), x + offset, y + offset);
    }

    void SdlWindow::setPaced(bool paced)
    {
        vsync = paced && SDL_SetRenderVSync(renderer.get(), 1);
        if (!paced)
        {
            SDL_SetRenderVSync(renderer.get(), 0);
        }
    }

    Rect SdlWindow::area() const
    {
        int width  = 0;
        int height = 0;
        SDL_GetWindowSize(window.get(), &width, &height);
        return {0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)};
    }

    void SdlWindow::handle(const SDL_Event &event)
    {
        const Rect size = area();
        if (const auto pointer = toPointerEvent(event, size.width, size.height))
        {
            router.handle(*pointer);
        }
        else if (const auto typed = toKeyboardInput(event))
        {
            view->keyboard(*typed);
        }
        else if (const auto wheel = toWheelEvent(event))
        {
            view->wheel(wheel->position, wheel->dy);
        }
    }

    void SdlWindow::syncTextInput()
    {
        const bool wanted = view->wantsTextInput();
        if (wanted == text_input)
        {
            return;
        }
        text_input = wanted;
        if (wanted)
        {
            SDL_StartTextInput(window.get());
        }
        else
        {
            SDL_StopTextInput(window.get());
        }
    }

    void SdlWindow::frame()
    {
        view->layout(area());
        view->update();
        syncTextInput();

        SDL_SetRenderDrawColor(renderer.get(), background.red, background.green, background.blue, background.alpha);
        SDL_RenderClear(renderer.get());
        SdlCanvas canvas{*renderer, *text, SDL_GetWindowPixelDensity(window.get())};
        view->draw(canvas);
        SDL_RenderPresent(renderer.get());
    }
} // namespace djw::player_ui::sdl
