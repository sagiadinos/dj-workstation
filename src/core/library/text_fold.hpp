// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_TEXT_FOLD_HPP
#define DJW_CORE_LIBRARY_TEXT_FOLD_HPP

#include <string>
#include <string_view>

namespace djw::core::library
{
    /**
     * UTF-8 text folded for matching and sorting: lower case and without accents for Latin script, so "Ärger",
     * "ärger" and "arger" fold alike; ß becomes ss, æ ae, œ oe. Covers ASCII, Latin-1 Supplement and Latin
     * Extended-A (the Western, Central and Northern European languages); other characters stay as they are.
     */
    std::string foldText(std::string_view text);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_TEXT_FOLD_HPP
