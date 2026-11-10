// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_CANVAS_HPP
#define DJW_PLAYER_UI_SDL_SDL_CANVAS_HPP

#include "canvas.hpp"
#include "sdl_text.hpp"

#include <SDL3/SDL_render.h>

namespace djw::player_ui::sdl
{
    /**
     * Canvas on an SDL renderer. The widgets work in points, the renderer in pixels; density (pixels per point, more
     * than 1 on HiDPI screens) converts between them. Text goes through SdlText at the same density.
     */
    class SdlCanvas final : public Canvas
    {
        public:
            SdlCanvas(SDL_Renderer &renderer, SdlText &text, float density) noexcept
                : renderer{&renderer}, text_renderer{&text}, density{density}
            {
                text.setDensity(density);
                // Colors with alpha (the brushed metal, glossy edges) blend with what is below
                SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_BLEND);
            }

            void fillRect(const Rect &rect, Color color) override;
            void fillTriangle(Point first, Point second, Point third, Color color) override;
            void fillRoundedRect(const Rect &rect, float radius, Color top, Color bottom) override;
            void text(Point position, std::string_view text, Color color) override;
            void textElided(Point position, std::string_view text, Color color, float max_width) override;
            [[nodiscard]] float textHeight() const noexcept override;
            [[nodiscard]] float textWidth(std::string_view text) const noexcept override;
            void                largeText(Point position, std::string_view text, Color color) override;
            [[nodiscard]] float largeTextHeight() const noexcept override;
            [[nodiscard]] float largeTextWidth(std::string_view text) const noexcept override;
            void                setClip(std::optional<Rect> clip) override;

        private:
            SDL_Renderer *renderer;
            SdlText      *text_renderer;
            float         density;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_CANVAS_HPP
