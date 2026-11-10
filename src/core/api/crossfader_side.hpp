// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_CROSSFADER_SIDE_HPP
#define DJW_CORE_API_CROSSFADER_SIDE_HPP

#include <cstdint>

namespace djw::core::api
{
    /// Crossfader side of a deck. A deck on None is not affected by the crossfader.
    enum class CrossfaderSide : std::uint8_t
    {
        None,
        A,
        B
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_CROSSFADER_SIDE_HPP
