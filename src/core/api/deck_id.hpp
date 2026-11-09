// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_DECK_ID_HPP
#define DJW_CORE_API_DECK_ID_HPP

#include <cstdint>

namespace djw::core::api
{
    /// Identifies a deck. Decks are added and removed at runtime, so this is an id, not a fixed slot.
    enum class DeckId : std::uint32_t
    {
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_DECK_ID_HPP
