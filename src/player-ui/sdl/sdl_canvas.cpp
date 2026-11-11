// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_canvas.hpp"

#include <algorithm>
#include <array>
#include <vector>

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

    void SdlCanvas::fillRoundedRect(const Rect &rect, float radius, Color top, Color bottom)
    {
        if (rect.width <= 0.0F || rect.height <= 0.0F)
        {
            return;
        }
        radius = std::clamp(radius, 0.0F, std::min(rect.width, rect.height) / 2);

        // The outline: four corner arcs, clockwise from the top left corner's start
        constexpr int      arc_steps = 6;
        constexpr float    half_pi   = 1.5707963F;
        std::vector<Point> outline;
        outline.reserve(static_cast<std::size_t>(4) * (arc_steps + 1));
        const std::array<Point, 4> centers{{{rect.x + radius, rect.y + radius},
                                            {rect.x + rect.width - radius, rect.y + radius},
                                            {rect.x + rect.width - radius, rect.y + rect.height - radius},
                                            {rect.x + radius, rect.y + rect.height - radius}}};
        for (int corner = 0; corner < 4; ++corner)
        {
            const float start = (static_cast<float>(corner) + 2.0F) * half_pi; // 180, 270, 0, 90 degrees
            for (int step = 0; step <= arc_steps; ++step)
            {
                const float angle = start + (half_pi * static_cast<float>(step) / arc_steps);
                outline.push_back({centers.at(static_cast<std::size_t>(corner)).x + (radius * SDL_cosf(angle)),
                                   centers.at(static_cast<std::size_t>(corner)).y + (radius * SDL_sinf(angle))});
            }
        }

        // A fan around the center; the color follows y, so the gradient is exact
        const auto color_at = [&](float y)
        {
            const float t   = std::clamp((y - rect.y) / rect.height, 0.0F, 1.0F);
            const auto  mix = [t](unsigned char from, unsigned char to)
            { return static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t; };
            return SDL_FColor{mix(top.red, bottom.red) / 255.0F, mix(top.green, bottom.green) / 255.0F,
                              mix(top.blue, bottom.blue) / 255.0F, mix(top.alpha, bottom.alpha) / 255.0F};
        };
        std::vector<SDL_Vertex> vertices;
        vertices.reserve(outline.size() + 1);
        const float center_y = rect.y + (rect.height / 2);
        vertices.push_back(
            {{(rect.x + (rect.width / 2)) * density, center_y * density}, color_at(center_y), {0.0F, 0.0F}});
        for (const Point &point : outline)
        {
            vertices.push_back({{point.x * density, point.y * density}, color_at(point.y), {0.0F, 0.0F}});
        }
        std::vector<int> indices;
        indices.reserve(outline.size() * 3);
        for (std::size_t index = 0; index < outline.size(); ++index)
        {
            indices.push_back(0);
            indices.push_back(static_cast<int>(index + 1));
            indices.push_back(static_cast<int>(((index + 1) % outline.size()) + 1));
        }
        SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(),
                           static_cast<int>(indices.size()));
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

    void SdlCanvas::largeText(Point position, std::string_view text, Color color)
    {
        text_renderer->draw(position, text, color, SdlText::Size::Large);
    }

    float SdlCanvas::largeTextHeight() const noexcept
    {
        return text_renderer->lineHeight(SdlText::Size::Large);
    }

    float SdlCanvas::largeTextWidth(std::string_view text) const noexcept
    {
        return text_renderer->width(text, SdlText::Size::Large);
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
