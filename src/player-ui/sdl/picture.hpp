// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_PICTURE_HPP
#define DJW_PLAYER_UI_SDL_PICTURE_HPP

#include <cstdint>
#include <string_view>

namespace djw::player_ui::sdl
{
    /// The pictures of the program, from misc/logo
    enum class Picture : std::uint8_t
    {
        Logo, ///< the name with the record, without a background of its own
        Icon  ///< the record on a black square, for the window icon
    };

    /// Width divided by height, as the viewBox of the SVG says (checked by a test)
    [[nodiscard]] constexpr float aspectOf(Picture picture) noexcept
    {
        return picture == Picture::Logo ? 1455.0F / 550.0F : 1.0F;
    }

    /// The SVG text of the picture, built into the program
    [[nodiscard]] std::string_view svgOf(Picture picture) noexcept;
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_PICTURE_HPP
