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

    SdlText::SdlText(TTF_TextEngine *engine, TTF_Font *normal, TTF_Font *large) noexcept
        : engine{engine}, normal_font{normal}, large_font{large}, current{&scaled(step)}
    {
    }

    SdlText::~SdlText()
    {
        dropScales(); // the shaped lines and font copies before the engine and the fonts they come from
        TTF_DestroyRendererTextEngine(engine);
        TTF_CloseFont(normal_font);
        TTF_CloseFont(large_font);
    }

    SdlText::Scaled &SdlText::scaled(int wanted)
    {
        if (const auto found = scales.find(wanted); found != scales.end())
        {
            return *found->second;
        }
        if (scales.size() >= max_scales)
        {
            // The oldest one goes, never the one in use
            const auto oldest =
                std::ranges::find_if(scale_order, [this](int candidate)
                                     { return current == nullptr || scales.at(candidate).get() != current; });
            if (oldest != scale_order.end())
            {
                Scaled &gone = *scales.at(*oldest);
                for (Face *which : {&gone.normal, &gone.large})
                {
                    clearCache(*which);
                    TTF_CloseFont(which->font);
                }
                scales.erase(*oldest);
                scale_order.erase(oldest);
            }
        }
        const float factor  = static_cast<float>(wanted) / 20.0F;
        auto        made    = std::make_unique<Scaled>();
        made->normal.font   = TTF_CopyFont(normal_font);
        made->normal.points = font_points;
        made->large.font    = TTF_CopyFont(large_font);
        made->large.points  = large_font_points;
        for (const Face *which : {&made->normal, &made->large})
        {
            if (which->font != nullptr)
            {
                TTF_SetFontSize(which->font, which->points * density * factor);
            }
        }
        Scaled &added = *made;
        scales.emplace(wanted, std::move(made));
        scale_order.push_back(wanted);
        return added;
    }

    void SdlText::dropScales()
    {
        for (auto &[key, fonts] : scales)
        {
            for (Face *which : {&fonts->normal, &fonts->large})
            {
                clearCache(*which);
                TTF_CloseFont(which->font);
            }
        }
        scales.clear();
        scale_order.clear();
        current = nullptr;
    }

    void SdlText::setDensity(float pixel_density)
    {
        if (pixel_density == density || pixel_density <= 0.0F)
        {
            return;
        }
        density = pixel_density;
        dropScales(); // every size changes
        current = &scaled(step);
    }

    void SdlText::setScale(float new_scale)
    {
        // Rounded, so a window being dragged to size does not make a font for every pixel
        const int wanted = std::max(5, static_cast<int>(std::lround(new_scale * 20.0F)));
        if (wanted == step && current != nullptr)
        {
            return;
        }
        step    = wanted;
        current = &scaled(step);
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
        TTF_MeasureString(face(Size::Normal).font, text.data(), text.size(), static_cast<int>(room), &measured_width,
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
