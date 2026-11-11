// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_text.hpp"

#include <SDL3/SDL_error.h>

#include <algorithm>
#include <cmath>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr std::string_view ellipsis = "…";
    } // namespace

    std::expected<std::unique_ptr<SdlText>, std::string> SdlText::open(SDL_Renderer                &renderer,
                                                                       const std::filesystem::path &font_file)
    {
        TTF_Font *normal = TTF_OpenFont(font_file.string().c_str(), font_points);
        if (normal == nullptr)
        {
            return std::unexpected{"cannot open font " + font_file.string() + ": " + SDL_GetError()};
        }
        TTF_Font *large = TTF_OpenFont(font_file.string().c_str(), large_font_points);
        if (large == nullptr)
        {
            TTF_CloseFont(normal);
            return std::unexpected{"cannot open font " + font_file.string() + ": " + SDL_GetError()};
        }
        TTF_TextEngine *engine = TTF_CreateRendererTextEngine(&renderer);
        if (engine == nullptr)
        {
            TTF_CloseFont(normal);
            TTF_CloseFont(large);
            return std::unexpected{std::string{"cannot create text engine: "} + SDL_GetError()};
        }
        return std::unique_ptr<SdlText>{new SdlText{engine, normal, large}};
    }

    SdlText::SdlText(TTF_TextEngine *engine, TTF_Font *normal_font, TTF_Font *large_font) noexcept
        : engine{engine}, normal{.font = normal_font, .points = font_points},
          large{.font = large_font, .points = large_font_points}
    {
    }

    SdlText::~SdlText()
    {
        clearCache(normal);
        clearCache(large);
        TTF_DestroyRendererTextEngine(engine);
        TTF_CloseFont(normal.font);
        TTF_CloseFont(large.font);
    }

    void SdlText::setDensity(float pixel_density)
    {
        if (pixel_density == density || pixel_density <= 0.0F)
        {
            return;
        }
        density = pixel_density;
        for (Face *which : {&normal, &large})
        {
            TTF_SetFontSize(which->font, which->points * density * scale);
            clearCache(*which);
        }
    }

    void SdlText::setScale(float new_scale)
    {
        // Rounded, so a window being dragged to size does not rebuild the font for every pixel
        new_scale = std::max(0.25F, std::round(new_scale * 20.0F) / 20.0F);
        if (new_scale == scale)
        {
            return;
        }
        scale = new_scale;
        for (Face *which : {&normal, &large})
        {
            TTF_SetFontSize(which->font, which->points * density * scale);
            clearCache(*which);
        }
    }

    void SdlText::clearCache(Face &which)
    {
        for (const auto &entry : which.cache)
        {
            TTF_DestroyText(entry.second);
        }
        which.cache.clear();
        which.widths.clear();
    }

    TTF_Text *SdlText::line(Face &which, std::string_view text)
    {
        const std::string key{text};
        if (const auto found = which.cache.find(key); found != which.cache.end())
        {
            return found->second;
        }
        if (which.cache.size() >= cache_limit)
        {
            clearCache(which);
        }
        TTF_Text *created = TTF_CreateText(engine, which.font, key.data(), key.size());
        if (created != nullptr)
        {
            which.cache.emplace(key, created);
        }
        return created;
    }

    void SdlText::draw(Point position, std::string_view text, Color color, Size size)
    {
        if (text.empty())
        {
            return;
        }
        TTF_Text *shaped = line(face(size), text);
        if (shaped == nullptr)
        {
            return;
        }
        TTF_SetTextColor(shaped, color.red, color.green, color.blue, color.alpha);
        // At whole pixels, so the glyphs stay sharp
        TTF_DrawRendererText(shaped, SDL_roundf(position.x * density), SDL_roundf(position.y * density));
    }

    void SdlText::drawElided(Point position, std::string_view text, Color color, float max_width)
    {
        if (width(text) <= max_width)
        {
            draw(position, text, color);
            return;
        }
        const float room = (max_width - width(ellipsis)) * density;
        if (room <= 0.0F)
        {
            return;
        }
        int         measured_width  = 0;
        std::size_t measured_length = 0;
        TTF_MeasureString(normal.font, text.data(), text.size(), static_cast<int>(room), &measured_width,
                          &measured_length);
        draw(position, std::string{text.substr(0, measured_length)} + std::string{ellipsis}, color);
    }

    float SdlText::lineHeight(Size size) const noexcept
    {
        return static_cast<float>(TTF_GetFontHeight(face(size).font)) / density;
    }

    float SdlText::width(std::string_view text, Size size) const
    {
        // SDL_ttf compares an empty text with its empty cache entry through a null pointer (memcmp of length 0)
        if (text.empty())
        {
            return 0.0F;
        }
        const Face &which = face(size);
        std::string key{text};
        if (const auto found = which.widths.find(key); found != which.widths.end())
        {
            return found->second;
        }
        int pixels = 0;
        int height = 0;
        TTF_GetStringSize(which.font, key.data(), key.size(), &pixels, &height);
        const float points = static_cast<float>(pixels) / density;
        if (which.widths.size() < cache_limit)
        {
            which.widths.emplace(std::move(key), points);
        }
        return points;
    }
} // namespace djw::player_ui::sdl
