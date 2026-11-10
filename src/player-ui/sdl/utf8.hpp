// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_UTF8_HPP
#define DJW_PLAYER_UI_SDL_UTF8_HPP

#include <string>

namespace djw::player_ui::sdl
{
    /// Removes the last UTF-8 code point of text (all its bytes); nothing for empty text
    inline void popCodePoint(std::string &text)
    {
        while (!text.empty())
        {
            const auto byte = static_cast<unsigned char>(text.back());
            text.pop_back();
            if ((byte & 0xC0U) != 0x80U) // not a continuation byte: the start of the code point
            {
                return;
            }
        }
    }
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_UTF8_HPP
