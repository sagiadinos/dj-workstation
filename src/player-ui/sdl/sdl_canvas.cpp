// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_canvas.hpp"

#include <array>

namespace djw::player_ui::sdl
{
    namespace
    {
        SDL_FColor toFColor(Color color)
        {
            return {static_cast<float>(color.red) / 255.0F, static_cast<float>(color.green) / 255.0F,
                    static_cast<float>(color.blue) / 255.0F, static_cast<float>(color.alpha) / 255.0F};
        }
    } // namespace

    void SdlCanvas::fillRect(const Rect &rect, Color color)
    {
        SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
        const SDL_FRect area{rect.x * density, rect.y * density, rect.width * density, rect.height * density};
        SDL_RenderFillRect(renderer, &area);
    }

    void SdlCanvas::fillTriangle(Point first, Point second, Point third, Color color)
    {
        const SDL_FColor                fcolor = toFColor(color);
        const std::array<SDL_Vertex, 3> vertices{{
            {{first.x * density, first.y * density}, fcolor, {0.0F, 0.0F}},
            {{second.x * density, second.y * density}, fcolor, {0.0F, 0.0F}},
            {{third.x * density, third.y * density}, fcolor, {0.0F, 0.0F}},
        }};
        SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), nullptr, 0);
    }

    void SdlCanvas::text(Point position, std::string_view text, Color color)
    {
        text_renderer->draw(position, text, color);
    }

    void SdlCanvas::textElided(Point position, std::string_view text, Color color, float max_width)
    {
        text_renderer->drawElided(position, text, color, max_width);
    }

    float SdlCanvas::textHeight() const noexcept
    {
        return text_renderer->lineHeight();
    }

    float SdlCanvas::textWidth(std::string_view text) const noexcept
    {
        return text_renderer->width(text);
    }

    void SdlCanvas::setClip(std::optional<Rect> clip)
    {
        if (!clip)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
            return;
        }
        const SDL_Rect area{
            static_cast<int>(SDL_floorf(clip->x * density)), static_cast<int>(SDL_floorf(clip->y * density)),
            static_cast<int>(SDL_ceilf(clip->width * density)), static_cast<int>(SDL_ceilf(clip->height * density))};
        SDL_SetRenderClipRect(renderer, &area);
    }
} // namespace djw::player_ui::sdl
