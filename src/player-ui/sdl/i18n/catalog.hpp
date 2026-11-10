// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_I18N_CATALOG_HPP
#define DJW_PLAYER_UI_SDL_I18N_CATALOG_HPP

#include "i18n/language.hpp"

#include <span>
#include <string_view>

namespace djw::player_ui::sdl
{
    /// One text of the user interface: its key and the message in the ICU MessageFormat subset of formatMessage()
    struct CatalogEntry
    {
            std::string_view key;
            std::string_view message;
    };

    /// All texts of a language, compiled into the program. English has every key; the others are checked against it.
    [[nodiscard]] std::span<const CatalogEntry> catalog(Language language);

    namespace catalogs
    {
        std::span<const CatalogEntry> english();
        std::span<const CatalogEntry> german();
        std::span<const CatalogEntry> french();
        std::span<const CatalogEntry> spanish();
        std::span<const CatalogEntry> greek();
        std::span<const CatalogEntry> russian();
    } // namespace catalogs
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_I18N_CATALOG_HPP
