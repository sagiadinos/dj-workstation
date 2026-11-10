// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_TEXT_HPP
#define DJW_PLAYER_UI_SDL_SDL_TEXT_HPP

#include "geometry.hpp"

#include <SDL3/SDL_render.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace djw::player_ui::sdl
{
    /**
     * Text drawing with SDL3_ttf on one renderer: one font, shaped with HarfBuzz, so any UTF-8 text the font covers
     * shows correctly.
     *
     * Sizes are in points. The font is rasterized at the window's pixel density, so text stays sharp on HiDPI
     * screens. Shaped lines are cached, as the same lines are drawn every frame.
     */
    class SdlText
    {
        public:
            static constexpr float font_points = 16.0F;

            static std::expected<std::unique_ptr<SdlText>, std::string> open(SDL_Renderer                &renderer,
                                                                             const std::filesystem::path &font_file);

            SdlText(const SdlText &)            = delete;
            SdlText &operator=(const SdlText &) = delete;
            SdlText(SdlText &&)                 = delete;
            SdlText &operator=(SdlText &&)      = delete;
            ~SdlText();

            /// Pixels per point of the window; call before drawing when it may have changed
            void setDensity(float density);

            /// Draws at position (points); the renderer works in pixels with a render scale of 1
            void draw(Point position, std::string_view text, Color color);
            /// Draws at most max_width points of text, cut with "…"
            void drawElided(Point position, std::string_view text, Color color, float max_width);

            [[nodiscard]] float lineHeight() const noexcept;
            [[nodiscard]] float width(std::string_view text) const;

        private:
            /// More cached lines than this and the cache starts over
            static constexpr std::size_t cache_limit = 4'096;

            SdlText(TTF_TextEngine *engine, TTF_Font *font) noexcept;

            TTF_Text *line(std::string_view text);
            void      clearCache();

            TTF_TextEngine                                *engine;
            TTF_Font                                      *font;
            float                                          density = 1.0F;
            std::unordered_map<std::string, TTF_Text *>    cache;
            mutable std::unordered_map<std::string, float> widths;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_TEXT_HPP
