// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_text.hpp"

#include <SDL3/SDL_error.h>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr std::string_view ellipsis = "…";
    } // namespace

    std::expected<std::unique_ptr<SdlText>, std::string> SdlText::open(SDL_Renderer                &renderer,
                                                                       const std::filesystem::path &font_file)
    {
        TTF_Font *font = TTF_OpenFont(font_file.string().c_str(), font_points);
        if (font == nullptr)
        {
            return std::unexpected{"cannot open font " + font_file.string() + ": " + SDL_GetError()};
        }
        TTF_TextEngine *engine = TTF_CreateRendererTextEngine(&renderer);
        if (engine == nullptr)
        {
            TTF_CloseFont(font);
            return std::unexpected{std::string{"cannot create text engine: "} + SDL_GetError()};
        }
        return std::unique_ptr<SdlText>{new SdlText{engine, font}};
    }

    SdlText::SdlText(TTF_TextEngine *engine, TTF_Font *font) noexcept : engine{engine}, font{font} {}

    SdlText::~SdlText()
    {
        clearCache();
        TTF_DestroyRendererTextEngine(engine);
        TTF_CloseFont(font);
    }

    void SdlText::setDensity(float pixel_density)
    {
        if (pixel_density == density || pixel_density <= 0.0F)
        {
            return;
        }
        density = pixel_density;
        TTF_SetFontSize(font, font_points * density);
        clearCache();
    }

    void SdlText::clearCache()
    {
        for (const auto &entry : cache)
        {
            TTF_DestroyText(entry.second);
        }
        cache.clear();
        widths.clear();
    }

    TTF_Text *SdlText::line(std::string_view text)
    {
        const std::string key{text};
        if (const auto found = cache.find(key); found != cache.end())
        {
            return found->second;
        }
        if (cache.size() >= cache_limit)
        {
            clearCache();
        }
        TTF_Text *created = TTF_CreateText(engine, font, key.data(), key.size());
        if (created != nullptr)
        {
            cache.emplace(key, created);
        }
        return created;
    }

    void SdlText::draw(Point position, std::string_view text, Color color)
    {
        if (text.empty())
        {
            return;
        }
        TTF_Text *shaped = line(text);
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
        TTF_MeasureString(font, text.data(), text.size(), static_cast<int>(room), &measured_width, &measured_length);
        draw(position, std::string{text.substr(0, measured_length)} + std::string{ellipsis}, color);
    }

    float SdlText::lineHeight() const noexcept
    {
        return static_cast<float>(TTF_GetFontHeight(font)) / density;
    }

    float SdlText::width(std::string_view text) const
    {
        std::string key{text};
        if (const auto found = widths.find(key); found != widths.end())
        {
            return found->second;
        }
        int pixels = 0;
        int height = 0;
        TTF_GetStringSize(font, key.data(), key.size(), &pixels, &height);
        const float points = static_cast<float>(pixels) / density;
        if (widths.size() < cache_limit)
        {
            widths.emplace(std::move(key), points);
        }
        return points;
    }
} // namespace djw::player_ui::sdl
