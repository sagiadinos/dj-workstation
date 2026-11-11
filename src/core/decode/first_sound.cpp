// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/first_sound.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace djw::core::decode
{
    namespace
    {
        constexpr std::size_t block_frames    = 4'096;
        constexpr std::size_t stereo_channels = 2;
    } // namespace

    std::optional<std::uint64_t> findFirstSound(Decoder &decoder, std::uint64_t max_frames, float threshold)
    {
        if (const auto moved = decoder.seek(0); !moved)
        {
            return std::nullopt;
        }
        std::array<float, block_frames * stereo_channels> block{};
        std::uint64_t                                     scanned = 0;
        while (scanned < max_frames)
        {
            const std::size_t wanted =
                static_cast<std::size_t>(std::min<std::uint64_t>(block_frames, max_frames - scanned));
            const auto read = decoder.read(std::span{block}.first(wanted * stereo_channels));
            if (!read || *read == 0)
            {
                return std::nullopt; // an error, or the end of the stream with nothing audible
            }
            for (std::size_t frame = 0; frame < *read; ++frame)
            {
                const float left  = std::abs(block[frame * stereo_channels]);
                const float right = std::abs(block[(frame * stereo_channels) + 1]);
                if (std::max(left, right) >= threshold)
                {
                    return scanned + frame;
                }
            }
            scanned += *read;
        }
        return std::nullopt;
    }
} // namespace djw::core::decode
