// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_CANVAS_HPP
#define DJW_PLAYER_UI_SDL_SDL_CANVAS_HPP

#include "canvas.hpp"

#include <SDL3/SDL_render.h>

namespace djw::player_ui::sdl
{
    /// Canvas on an SDL renderer. Text uses SDL's built-in debug font until a real font comes.
    class SdlCanvas final : public Canvas
    {
        public:
            explicit SdlCanvas(SDL_Renderer &renderer) noexcept : renderer{&renderer} {}

            void                fillRect(const Rect &rect, Color color) override;
            void                fillTriangle(Point first, Point second, Point third, Color color) override;
            void                text(Point position, std::string_view text, Color color) override;
            [[nodiscard]] float textHeight() const noexcept override;
            [[nodiscard]] float textWidth(std::string_view text) const noexcept override;

        private:
            SDL_Renderer *renderer;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_CANVAS_HPP
