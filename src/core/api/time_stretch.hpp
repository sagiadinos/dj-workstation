// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_TIME_STRETCH_HPP
#define DJW_CORE_API_TIME_STRETCH_HPP

#include <cstdint>

namespace djw::core::api
{
    /// The library that does keylock: tempo changes without pitch changes (Architecture.md, the deck)
    enum class TimeStretch : std::uint8_t
    {
        SoundTouch,
        Signalsmith
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_TIME_STRETCH_HPP
