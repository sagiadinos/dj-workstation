// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_PLAYER_UI_SDL_FAKE_DIALOGS_HPP
#define DJW_TESTS_PLAYER_UI_SDL_FAKE_DIALOGS_HPP

#include "file_dialogs.hpp"

#include <doctest/doctest.h>

#include <string>
#include <utility>

namespace djw::tests
{
    /// Remembers what was asked; the test answers with choose()
    class FakeDialogs final : public player_ui::sdl::FileDialogs
    {
        public:
            void openPlaylist(Chosen chosen) override
            {
                opened  = true;
                pending = std::move(chosen);
            }

            void savePlaylist(const std::string &suggested_name, Chosen chosen) override
            {
                suggested = suggested_name;
                pending   = std::move(chosen);
            }

            void openFolder(Chosen chosen) override
            {
                folder_asked = true;
                pending      = std::move(chosen);
            }

            void choose(const std::filesystem::path &file)
            {
                REQUIRE(pending);
                pending(file);
                pending = nullptr;
            }

            bool        opened       = false;
            bool        folder_asked = false;
            std::string suggested;
            Chosen      pending;
    };
} // namespace djw::tests

#endif // DJW_TESTS_PLAYER_UI_SDL_FAKE_DIALOGS_HPP
