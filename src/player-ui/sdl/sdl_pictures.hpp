// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_PICTURES_HPP
#define DJW_PLAYER_UI_SDL_SDL_PICTURES_HPP

#include "picture.hpp"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>

#include <memory>
#include <optional>
#include <vector>

namespace djw::player_ui::sdl
{
    struct SurfaceDeleter
    {
            void operator()(SDL_Surface *surface) const noexcept { SDL_DestroySurface(surface); }
    };
    using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

    /// The picture drawn from its SVG into a surface of width x height pixels, keeping its aspect, centered and
    /// transparent around it; nullptr if it cannot be drawn
    [[nodiscard]] SurfacePtr pictureSurface(Picture picture, int width, int height);

    /// Width and height of the picture as its SVG says; std::nullopt if the SVG cannot be read
    struct PictureSize
    {
            float width  = 0.0F;
            float height = 0.0F;
    };
    [[nodiscard]] std::optional<PictureSize> pictureSize(Picture picture);

    /**
     * The pictures of one renderer as textures. Each is drawn from its SVG at the size it is shown, so it stays sharp;
     * it is drawn again only when that size changes. Must be destroyed before the renderer.
     */
    class SdlPictures
    {
        public:
            explicit SdlPictures(SDL_Renderer &renderer) noexcept : renderer{&renderer} {}

            /// The picture as a texture of width x height pixels; nullptr if it cannot be made
            [[nodiscard]] SDL_Texture *texture(Picture picture, int width, int height);

        private:
            struct TextureDeleter
            {
                    void operator()(SDL_Texture *texture) const noexcept { SDL_DestroyTexture(texture); }
            };
            struct Made
            {
                    Picture                                      picture = Picture::Logo;
                    int                                          width   = 0;
                    int                                          height  = 0;
                    std::unique_ptr<SDL_Texture, TextureDeleter> texture;
            };

            SDL_Renderer     *renderer;
            std::vector<Made> made; ///< the last size of each picture
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_PICTURES_HPP
