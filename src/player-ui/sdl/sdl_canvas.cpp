// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_canvas.hpp"

#include <array>
#include <string>

namespace djw::player_ui::sdl
{
    namespace
    {
        // The debug font is 8 px; drawn twice as large to be readable on a touchscreen
        constexpr float text_scale = 2.0F;

        SDL_FColor toFColor(Color color)
        {
            return {static_cast<float>(color.red) / 255.0F, static_cast<float>(color.green) / 255.0F,
                    static_cast<float>(color.blue) / 255.0F, static_cast<float>(color.alpha) / 255.0F};
        }
    } // namespace

    void SdlCanvas::fillRect(const Rect &rect, Color color)
    {
        SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
        const SDL_FRect area{rect.x, rect.y, rect.width, rect.height};
        SDL_RenderFillRect(renderer, &area);
    }

    void SdlCanvas::fillTriangle(Point first, Point second, Point third, Color color)
    {
        const SDL_FColor                fcolor = toFColor(color);
        const std::array<SDL_Vertex, 3> vertices{{
            {{first.x, first.y}, fcolor, {0.0F, 0.0F}},
            {{second.x, second.y}, fcolor, {0.0F, 0.0F}},
            {{third.x, third.y}, fcolor, {0.0F, 0.0F}},
        }};
        SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), nullptr, 0);
    }

    void SdlCanvas::text(Point position, std::string_view text, Color color)
    {
        float scale_x = 1.0F;
        float scale_y = 1.0F;
        SDL_GetRenderScale(renderer, &scale_x, &scale_y);
        SDL_SetRenderScale(renderer, scale_x * text_scale, scale_y * text_scale);
        SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
        const std::string null_terminated{text};
        SDL_RenderDebugText(renderer, position.x / text_scale, position.y / text_scale, null_terminated.c_str());
        SDL_SetRenderScale(renderer, scale_x, scale_y);
    }

    float SdlCanvas::textHeight() const noexcept
    {
        return SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * text_scale;
    }

    float SdlCanvas::textWidth(std::string_view text) const noexcept
    {
        return static_cast<float>(text.size()) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * text_scale;
    }
} // namespace djw::player_ui::sdl
