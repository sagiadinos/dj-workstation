// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_FIRST_SOUND_HPP
#define DJW_CORE_DECODE_FIRST_SOUND_HPP

#include "decode/decoder.hpp"

#include <cstdint>
#include <optional>

namespace djw::core::decode
{
    /// Samples below this level (about -54 dBFS) count as silence; encoder padding and dither stay below it
    inline constexpr float silence_threshold = 0.002F;

    /**
     * The first frame at which the decoder's audio is audible: either channel reaches threshold. Reads from the
     * start of the stream and looks at most at max_frames; the decoder is left somewhere in the stream, seek it
     * before using it again. std::nullopt if there is only silence within max_frames, the stream ends before a sound
     * or the decoder fails, i.e. when the first sound is not known.
     */
    std::optional<std::uint64_t> findFirstSound(Decoder &decoder, std::uint64_t max_frames,
                                                float threshold = silence_threshold);
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_FIRST_SOUND_HPP
