// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TIMED_STATE_HPP
#define DJW_PLAYER_UI_SDL_TIMED_STATE_HPP

#include "clock.hpp"

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

namespace djw::player_ui::sdl
{
    /// A message (a catalog key) that is shown for a while, e.g. why a track was not loaded
    class TimedNotice
    {
        public:
            TimedNotice(Clock clock, std::chrono::milliseconds duration) : clock{std::move(clock)}, duration{duration}
            {
            }

            /// Shows key from now on for the duration; the key must outlive the notice (a string literal)
            void show(std::string_view key)
            {
                shown = key;
                until = clock() + duration;
            }

            /// The key while it is shown, else empty
            [[nodiscard]] std::string_view current() const
            {
                return !shown.empty() && clock() < until ? shown : std::string_view{};
            }

        private:
            Clock                                 clock;
            std::chrono::milliseconds             duration;
            std::string_view                      shown;
            std::chrono::steady_clock::time_point until;
    };

    /// A tap that has to be repeated within a time to count, for buttons that delete something
    class ConfirmTap
    {
        public:
            ConfirmTap(Clock clock, std::chrono::milliseconds window) : clock{std::move(clock)}, window{window} {}

            /// True if this tap confirms an earlier one within the window (which is then used up); else it arms
            [[nodiscard]] bool tap()
            {
                if (armed())
                {
                    until.reset();
                    return true;
                }
                until = clock() + window;
                return false;
            }

            /// Waits for the confirming tap: armed by a tap, not confirmed, cancelled or timed out yet
            [[nodiscard]] bool armed() const { return until && clock() <= *until; }
            void               cancel() noexcept { until.reset(); }

        private:
            Clock                                                clock;
            std::chrono::milliseconds                            window;
            std::optional<std::chrono::steady_clock::time_point> until;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TIMED_STATE_HPP
