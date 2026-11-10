// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_PLAYER_UI_SDL_RECORDING_CANVAS_HPP
#define DJW_TESTS_PLAYER_UI_SDL_RECORDING_CANVAS_HPP

#include "canvas.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace djw::tests
{
    /**
     * Canvas that records the text drawn on it, with a fixed-width font: every byte is char_width wide. Elided text
     * is recorded as it would show, cut and ended with "…".
     */
    class RecordingCanvas final : public player_ui::sdl::Canvas
    {
        public:
            static constexpr float char_width  = 10.0F;
            static constexpr float line_height = 20.0F;

            struct DrawnText
            {
                    player_ui::sdl::Point position;
                    std::string           text;
            };

            void fillRect(const player_ui::sdl::Rect & /*rect*/, player_ui::sdl::Color /*color*/) override {}
            void fillTriangle(player_ui::sdl::Point, player_ui::sdl::Point, player_ui::sdl::Point,
                              player_ui::sdl::Color) override
            {
            }

            void text(player_ui::sdl::Point position, std::string_view text, player_ui::sdl::Color) override
            {
                texts.push_back({position, std::string{text}});
            }

            void textElided(player_ui::sdl::Point position, std::string_view text, player_ui::sdl::Color color,
                            float max_width) override
            {
                if (textWidth(text) <= max_width)
                {
                    this->text(position, text, color);
                    return;
                }
                const auto fitting = static_cast<std::size_t>(std::max(0.0F, max_width / char_width - 1));
                this->text(position, std::string{text.substr(0, fitting)} + "…", color);
            }

            [[nodiscard]] float textHeight() const noexcept override { return line_height; }
            [[nodiscard]] float textWidth(std::string_view text) const noexcept override
            {
                return static_cast<float>(text.size()) * char_width;
            }

            // The large text counts like the normal text, three times as big; it is recorded in the same list
            void largeText(player_ui::sdl::Point position, std::string_view text, player_ui::sdl::Color color) override
            {
                texts.push_back({position, std::string{text}});
                static_cast<void>(color);
            }
            [[nodiscard]] float largeTextHeight() const noexcept override { return line_height * 2.0F; }
            [[nodiscard]] float largeTextWidth(std::string_view text) const noexcept override
            {
                return static_cast<float>(text.size()) * char_width * 2.0F;
            }

            void setClip(std::optional<player_ui::sdl::Rect> rect) override
            {
                clip = rect;
                clip_changes.push_back(rect);
            }

            /// True if some drawn text equals text
            [[nodiscard]] bool drew(std::string_view text) const
            {
                return std::ranges::any_of(texts, [text](const DrawnText &drawn) { return drawn.text == text; });
            }

            std::vector<DrawnText>                           texts;
            std::optional<player_ui::sdl::Rect>              clip;
            std::vector<std::optional<player_ui::sdl::Rect>> clip_changes;
    };
} // namespace djw::tests

#endif // DJW_TESTS_PLAYER_UI_SDL_RECORDING_CANVAS_HPP
