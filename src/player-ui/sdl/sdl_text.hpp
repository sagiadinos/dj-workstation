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
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * Text drawing with SDL3_ttf on one renderer: one font in two sizes (normal, and large for the time display),
     * shaped with HarfBuzz, so any UTF-8 text the font covers shows correctly.
     *
     * Sizes are in points. The font is rasterized at the window's pixel density, so text stays sharp on HiDPI
     * screens. Shaped lines are cached, as the same lines are drawn every frame.
     *
     * Each text scale in use has fonts of its own (copies of the two opened fonts), kept with their caches: views of
     * different sizes in one window switch the scale several times per frame, and switching must not rebuild
     * anything. Scales are rounded to steps of 0.05; more than max_scales of them and the oldest ones go.
     */
    class SdlText
    {
        public:
            static constexpr float font_points       = 16.0F;
            static constexpr float large_font_points = 40.0F;

            enum class Size : unsigned char
            {
                Normal,
                Large
            };

            static std::expected<std::unique_ptr<SdlText>, std::string> open(SDL_Renderer                &renderer,
                                                                             const std::filesystem::path &font_file);

            SdlText(const SdlText &)            = delete;
            SdlText &operator=(const SdlText &) = delete;
            SdlText(SdlText &&)                 = delete;
            SdlText &operator=(SdlText &&)      = delete;
            ~SdlText();

            /// Pixels per point of the window; call before drawing when it may have changed
            void setDensity(float density);

            /// Makes the text this many times as big (1: normal); the player window scales with its size
            void setScale(float new_scale);

            /// Draws at position (points); the renderer works in pixels with a render scale of 1
            void draw(Point position, std::string_view text, Color color, Size size = Size::Normal);
            /// Draws at most max_width points of text, cut with "…"
            void drawElided(Point position, std::string_view text, Color color, float max_width);

            [[nodiscard]] float lineHeight(Size size = Size::Normal) const noexcept;
            [[nodiscard]] float width(std::string_view text, Size size = Size::Normal) const;

        private:
            /// More cached lines than this and the cache starts over
            static constexpr std::size_t cache_limit = 4'096;
            /// Text scales kept at once
            static constexpr std::size_t max_scales = 8;

            /// One size of the font with its shaped lines and measured widths
            struct Face
            {
                    TTF_Font                                      *font   = nullptr;
                    float                                          points = 0.0F;
                    std::unordered_map<std::string, TTF_Text *>    cache{};
                    mutable std::unordered_map<std::string, float> widths{};
            };

            /// The two sizes of the font at one scale
            struct Scaled
            {
                    Face normal;
                    Face large;
            };

            SdlText(TTF_TextEngine *engine, TTF_Font *normal, TTF_Font *large) noexcept;

            [[nodiscard]] Face &face(Size size) noexcept
            {
                return size == Size::Large ? current->large : current->normal;
            }
            [[nodiscard]] const Face &face(Size size) const noexcept
            {
                return size == Size::Large ? current->large : current->normal;
            }
            TTF_Text *line(Face &which, std::string_view text);
            void      clearCache(Face &which);
            /// The fonts of the scale step (scale * 20), made when first used
            Scaled &scaled(int step);
            /// Closes the fonts of every scale
            void dropScales();

            TTF_TextEngine                        *engine;
            TTF_Font                              *normal_font; ///< as opened, copied for every scale
            TTF_Font                              *large_font;
            float                                  density = 1.0F;
            int                                    step    = 20; ///< the scale in use, in steps of 0.05
            std::map<int, std::unique_ptr<Scaled>> scales;
            std::vector<int>                       scale_order; ///< oldest first, for dropping
            Scaled                                *current = nullptr;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_TEXT_HPP
