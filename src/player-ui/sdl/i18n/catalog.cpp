// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

namespace djw::player_ui::sdl
{
    std::span<const CatalogEntry> catalog(Language language)
    {
        switch (language)
        {
            case Language::German:
                return catalogs::german();
            case Language::French:
                return catalogs::french();
            case Language::Spanish:
                return catalogs::spanish();
            case Language::Greek:
                return catalogs::greek();
            case Language::Russian:
                return catalogs::russian();
            case Language::English:
                break;
        }
        return catalogs::english();
    }
} // namespace djw::player_ui::sdl
