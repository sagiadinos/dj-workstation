// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_pictures.hpp"

#include "sdl_canvas.hpp"
#include "sdl_text.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <utility>

using namespace djw::player_ui::sdl;

namespace
{
    unsigned char alphaAt(const SDL_Surface &surface, int x, int y)
    {
        const auto *pixels = static_cast<const unsigned char *>(surface.pixels);
        return pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(surface.pitch)) +
                      (static_cast<std::size_t>(x) * 4) + 3];
    }
} // namespace

TEST_CASE("Pictures: the aspect the layout works with is the one of the SVG")
{
    for (const Picture picture : {Picture::Logo, Picture::Icon})
    {
        const auto size = pictureSize(picture);
        REQUIRE(size.has_value());
        CHECK(aspectOf(picture) == doctest::Approx(size->width / size->height));
    }
}

TEST_CASE("Pictures: the logo is drawn centered, transparent around it; the icon fills its square")
{
    // Twice as high as the logo needs: a transparent band above and below it
    const SurfacePtr logo = pictureSurface(Picture::Logo, 529, 400);
    REQUIRE(logo != nullptr);
    CHECK(logo->w == 529);
    CHECK(logo->h == 400);
    int visible = 0;
    for (int y = 0; y < logo->h; ++y)
    {
        for (int x = 0; x < logo->w; ++x)
        {
            visible += alphaAt(*logo, x, y) > 0 ? 1 : 0;
        }
    }
    CHECK(visible > 529 * 200 / 10); // a good part of the logo's own area
    for (int x = 0; x < logo->w; ++x)
    {
        REQUIRE(alphaAt(*logo, x, 0) == 0);
        REQUIRE(alphaAt(*logo, x, logo->h - 1) == 0);
    }

    const SurfacePtr icon = pictureSurface(Picture::Icon, 64, 64);
    REQUIRE(icon != nullptr);
    CHECK(alphaAt(*icon, 32, 32) == 255);
    CHECK(alphaAt(*icon, 0, 0) < 255); // the rounded corner

    CHECK(pictureSurface(Picture::Logo, 0, 10) == nullptr);
}

namespace
{
    /// SDL's dummy video driver with a software renderer, the UI font next to the test binary
    struct RenderFixture
    {
            RenderFixture()
            {
                SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
                REQUIRE(SDL_Init(SDL_INIT_VIDEO));
                REQUIRE(TTF_Init());
                window = SDL_CreateWindow("pictures", 400, 200, 0);
                REQUIRE(window != nullptr);
                renderer = SDL_CreateRenderer(window, "software");
                REQUIRE(renderer != nullptr);
                const char *base = SDL_GetBasePath();
                auto made = SdlText::open(*renderer, std::filesystem::path{base != nullptr ? base : ""} / "fonts" /
                                                         "NotoSans-Regular.ttf");
                REQUIRE(made.has_value());
                text     = std::move(*made);
                pictures = std::make_unique<SdlPictures>(*renderer);
            }

            ~RenderFixture()
            {
                pictures.reset();
                text.reset();
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                TTF_Quit();
                SDL_Quit();
            }

            RenderFixture(const RenderFixture &)            = delete;
            RenderFixture &operator=(const RenderFixture &) = delete;
            RenderFixture(RenderFixture &&)                 = delete;
            RenderFixture &operator=(RenderFixture &&)      = delete;

            /// Black, then what draw does; the number of pixels that are not black, inside and outside area
            template <typename Draw> std::pair<int, int> litPixels(const Rect &area, Draw draw)
            {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                draw();
                SurfacePtr shot{SDL_RenderReadPixels(renderer, nullptr)};
                REQUIRE(shot != nullptr);
                SurfacePtr rgba{SDL_ConvertSurface(shot.get(), SDL_PIXELFORMAT_RGBA32)};
                REQUIRE(rgba != nullptr);
                int inside  = 0;
                int outside = 0;
                for (int y = 0; y < rgba->h; ++y)
                {
                    for (int x = 0; x < rgba->w; ++x)
                    {
                        const auto *pixel = static_cast<const unsigned char *>(rgba->pixels) +
                                            (static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba->pitch)) +
                                            (static_cast<std::size_t>(x) * 4);
                        const bool  lit   = pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0;
                        (area.contains({static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F}) ? inside
                                                                                                     : outside) +=
                            lit ? 1 : 0;
                    }
                }
                return {inside, outside};
            }

            SDL_Window                  *window   = nullptr;
            SDL_Renderer                *renderer = nullptr;
            std::unique_ptr<SdlText>     text;
            std::unique_ptr<SdlPictures> pictures;
    };
} // namespace

TEST_CASE("SdlCanvas: a picture is drawn in its area only, faint with a low alpha, left out without pictures")
{
    RenderFixture fixture;
    const Rect    area{20, 20, 264, 100};

    SdlCanvas  canvas{*fixture.renderer, *fixture.text, 1.0F, fixture.pictures.get()};
    const auto full = fixture.litPixels(area, [&] { canvas.picture(Picture::Logo, area, 255); });
    CHECK(full.first > 264 * 100 / 10);
    CHECK(full.second == 0);

    const auto none = fixture.litPixels(area, [&] { canvas.picture(Picture::Logo, area, 0); });
    CHECK(none.first == 0);

    SdlCanvas  without{*fixture.renderer, *fixture.text, 1.0F};
    const auto left_out = fixture.litPixels(area, [&] { without.picture(Picture::Logo, area, 255); });
    CHECK(left_out.first == 0);
}
