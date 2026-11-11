// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SETTINGS_HPP
#define DJW_PLAYER_UI_SDL_SETTINGS_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace djw::player_ui::sdl
{
    /// The audio buffer sizes offered as latency, shortest first
    inline constexpr std::array<std::uint32_t, 4> latency_choices{128, 256, 512, 1024};
    /// The buffer size without a setting
    inline constexpr std::uint32_t default_buffer_frames = 256;

    /**
     * What the user set in Settings > Devices, kept for the next start (they take effect when the core is created).
     * Later the audio driver and device go here as well.
     */
    struct Settings
    {
            /// Audio buffer size, one of latency_choices
            std::uint32_t buffer_frames = default_buffer_frames;

            bool operator==(const Settings &) const = default;
    };

    /// The settings as text, one setting per line
    [[nodiscard]] std::string formatSettings(const Settings &settings);
    /// The settings from text written by formatSettings. Whatever cannot be read (no file, another program's text,
    /// a buffer size not offered, lines of a later version) keeps its default, so the program always starts.
    [[nodiscard]] Settings parseSettings(std::string_view text);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SETTINGS_HPP
