// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_text.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>

using namespace djw::player_ui::sdl;

namespace
{
    /// SDL's dummy video driver with a software renderer, and the UI font copied next to the test binary
    struct TextFixture
    {
            TextFixture()
            {
                SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
                REQUIRE(SDL_Init(SDL_INIT_VIDEO));
                REQUIRE(TTF_Init());
                window = SDL_CreateWindow("text", 400, 300, 0);
                REQUIRE(window != nullptr);
                renderer = SDL_CreateRenderer(window, "software");
                REQUIRE(renderer != nullptr);
                const char *base = SDL_GetBasePath();
                auto made = SdlText::open(*renderer, std::filesystem::path{base != nullptr ? base : ""} / "fonts" /
                                                         "NotoSans-Regular.ttf");
                REQUIRE(made.has_value());
                text = std::move(*made);
            }

            ~TextFixture()
            {
                text.reset(); // before the renderer it draws with
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                TTF_Quit();
                SDL_Quit();
            }

            TextFixture(const TextFixture &)            = delete;
            TextFixture &operator=(const TextFixture &) = delete;
            TextFixture(TextFixture &&)                 = delete;
            TextFixture &operator=(TextFixture &&)      = delete;

            SDL_Window              *window   = nullptr;
            SDL_Renderer            *renderer = nullptr;
            std::unique_ptr<SdlText> text;
    };
} // namespace

TEST_CASE("SdlText: every scale has its size; switching back and forth gives the same sizes again")
{
    TextFixture fixture;
    SdlText    &text   = *fixture.text;
    const float normal = text.width("00:01:23:40", SdlText::Size::Large);
    const float high   = text.lineHeight();
    REQUIRE(normal > 0.0F);

    text.setScale(2.0F);
    CHECK(text.width("00:01:23:40", SdlText::Size::Large) == doctest::Approx(2 * normal).epsilon(0.05));
    CHECK(text.lineHeight() == doctest::Approx(2 * high).epsilon(0.1));
    text.setScale(1.0F);
    CHECK(text.width("00:01:23:40", SdlText::Size::Large) == doctest::Approx(normal));

    // More scales than are kept: the old ones go and come back when asked for, the sizes stay right
    for (int step = 10; step < 60; step += 3)
    {
        text.setScale(static_cast<float>(step) / 20.0F);
        text.draw({0, 0}, "abc", {255, 255, 255});
    }
    text.setScale(1.0F);
    CHECK(text.width("00:01:23:40", SdlText::Size::Large) == doctest::Approx(normal));
    text.setScale(2.0F);
    CHECK(text.width("00:01:23:40", SdlText::Size::Large) == doctest::Approx(2 * normal).epsilon(0.05));
}
