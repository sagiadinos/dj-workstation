// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/text_fold.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace djw::core::library
{
    namespace
    {
        constexpr char32_t table_start = 0xC0;
        constexpr char32_t table_end   = 0x180;

        // Folded form of U+00C0 ... U+017F; empty: keep the character (× and ÷). Made from the Unicode
        // decomposition without combining marks, plus the letters that have none (ß, æ, ø, ł, ...).
        constexpr std::array<std::string_view, table_end - table_start> latin_folds{
            "a", "a", "a",  "a",  "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i",  "i",  // U+00C0
            "d", "n", "o",  "o",  "o", "o", "o",  "",  "o", "u", "u", "u", "u", "y", "th", "ss", // U+00D0
            "a", "a", "a",  "a",  "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i",  "i",  // U+00E0
            "d", "n", "o",  "o",  "o", "o", "o",  "",  "o", "u", "u", "u", "u", "y", "th", "y",  // U+00F0
            "a", "a", "a",  "a",  "a", "a", "c",  "c", "c", "c", "c", "c", "c", "c", "d",  "d",  // U+0100
            "d", "d", "e",  "e",  "e", "e", "e",  "e", "e", "e", "e", "e", "g", "g", "g",  "g",  // U+0110
            "g", "g", "g",  "g",  "h", "h", "h",  "h", "i", "i", "i", "i", "i", "i", "i",  "i",  // U+0120
            "i", "i", "ij", "ij", "j", "j", "k",  "k", "k", "l", "l", "l", "l", "l", "l",  "l",  // U+0130
            "l", "l", "l",  "n",  "n", "n", "n",  "n", "n", "n", "n", "n", "o", "o", "o",  "o",  // U+0140
            "o", "o", "oe", "oe", "r", "r", "r",  "r", "r", "r", "s", "s", "s", "s", "s",  "s",  // U+0150
            "s", "s", "t",  "t",  "t", "t", "t",  "t", "u", "u", "u", "u", "u", "u", "u",  "u",  // U+0160
            "u", "u", "u",  "u",  "w", "w", "y",  "y", "y", "z", "z", "z", "z", "z", "z",  "s",  // U+0170
        };

        /// Decodes the UTF-8 sequence at position; length 0 for an invalid sequence
        struct Decoded
        {
                char32_t    code_point = 0;
                std::size_t length     = 0;
        };

        Decoded decode(std::string_view text, std::size_t position)
        {
            const auto lead = static_cast<unsigned char>(text[position]);
            if (lead < 0x80U)
            {
                return {lead, 1};
            }
            // Only two-byte sequences can be in the table; longer ones are copied as they are
            if ((lead & 0xE0U) == 0xC0U && position + 1 < text.size())
            {
                const auto next = static_cast<unsigned char>(text[position + 1]);
                if ((next & 0xC0U) == 0x80U)
                {
                    return {static_cast<char32_t>(((lead & 0x1FU) << 6U) | (next & 0x3FU)), 2};
                }
            }
            return {0, 0};
        }
    } // namespace

    std::string foldText(std::string_view text)
    {
        std::string folded;
        folded.reserve(text.size());
        std::size_t position = 0;
        while (position < text.size())
        {
            const Decoded decoded = decode(text, position);
            if (decoded.length == 1)
            {
                const char character = text[position];
                folded += character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
            }
            else if (decoded.length == 2 && decoded.code_point >= table_start && decoded.code_point < table_end &&
                     !latin_folds[decoded.code_point - table_start].empty())
            {
                folded += latin_folds[decoded.code_point - table_start];
            }
            else
            {
                const std::size_t length = decoded.length == 0 ? 1 : decoded.length;
                folded += text.substr(position, length);
                position += length;
                continue;
            }
            position += decoded.length;
        }
        return folded;
    }
} // namespace djw::core::library
