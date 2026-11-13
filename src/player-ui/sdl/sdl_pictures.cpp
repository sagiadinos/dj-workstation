// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_pictures.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

// nanosvg is header only; its implementation is compiled here, once
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

namespace djw::player_ui::sdl
{
    namespace
    {
        struct ImageDeleter
        {
                void operator()(NSVGimage *image) const noexcept { nsvgDelete(image); }
        };
        using ImagePtr = std::unique_ptr<NSVGimage, ImageDeleter>;

        struct RasterizerDeleter
        {
                void operator()(NSVGrasterizer *rasterizer) const noexcept { nsvgDeleteRasterizer(rasterizer); }
        };

        ImagePtr parse(Picture picture)
        {
            // nanosvg changes the text while it reads it, and needs it ended by a zero
            std::string text{svgOf(picture)};
            return ImagePtr{nsvgParse(text.data(), "px", 96.0F)};
        }
    } // namespace

    std::optional<PictureSize> pictureSize(Picture picture)
    {
        const ImagePtr image = parse(picture);
        if (!image || image->width <= 0.0F || image->height <= 0.0F)
        {
            return std::nullopt;
        }
        return PictureSize{.width = image->width, .height = image->height};
    }

    SurfacePtr pictureSurface(Picture picture, int width, int height)
    {
        const ImagePtr image = parse(picture);
        if (!image || image->width <= 0.0F || image->height <= 0.0F || width <= 0 || height <= 0)
        {
            return nullptr;
        }
        const std::unique_ptr<NSVGrasterizer, RasterizerDeleter> rasterizer{nsvgCreateRasterizer()};
        if (!rasterizer)
        {
            return nullptr;
        }
        const auto                 stride = static_cast<std::size_t>(width) * 4;
        std::vector<unsigned char> pixels(stride * static_cast<std::size_t>(height));
        const float                scale =
            std::min(static_cast<float>(width) / image->width, static_cast<float>(height) / image->height);
        const float x = (static_cast<float>(width) - (image->width * scale)) / 2.0F;
        const float y = (static_cast<float>(height) - (image->height * scale)) / 2.0F;
        nsvgRasterize(rasterizer.get(), image.get(), x, y, scale, pixels.data(), width, height,
                      static_cast<int>(stride));

        // Bytes red, green, blue, alpha, not premultiplied: SDL's RGBA32 on every byte order
        SurfacePtr surface{SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32)};
        if (!surface)
        {
            return nullptr;
        }
        auto *target = static_cast<unsigned char *>(surface->pixels);
        for (int row = 0; row < height; ++row)
        {
            std::memcpy(target + (static_cast<std::size_t>(row) * static_cast<std::size_t>(surface->pitch)),
                        pixels.data() + (static_cast<std::size_t>(row) * stride), stride);
        }
        return surface;
    }

    SDL_Texture *SdlPictures::texture(Picture picture, int width, int height)
    {
        auto found = std::ranges::find(made, picture, &Made::picture);
        if (found != made.end() && found->width == width && found->height == height)
        {
            return found->texture.get();
        }
        const SurfacePtr surface = pictureSurface(picture, width, height);
        if (!surface)
        {
            return nullptr;
        }
        std::unique_ptr<SDL_Texture, TextureDeleter> texture{SDL_CreateTextureFromSurface(renderer, surface.get())};
        if (!texture)
        {
            return nullptr;
        }
        SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
        if (found == made.end())
        {
            made.push_back({.picture = picture, .width = 0, .height = 0, .texture = nullptr});
            found = std::prev(made.end());
        }
        found->width   = width;
        found->height  = height;
        found->texture = std::move(texture);
        return found->texture.get();
    }
} // namespace djw::player_ui::sdl
