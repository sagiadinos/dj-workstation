// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_DECODER_HPP
#define DJW_CORE_DECODE_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace djw::core::decode
{
    /**
     * Decodes an audio stream into interleaved stereo float32 at the sample rate of the engine.
     *
     * Runs on a decoder thread, never on the audio thread, so it may block and allocate. Conversion of channel count
     * and sample rate is the decoder's job.
     */
    class Decoder
    {
        public:
            Decoder()                           = default;
            Decoder(const Decoder &)            = delete;
            Decoder &operator=(const Decoder &) = delete;
            Decoder(Decoder &&)                 = delete;
            Decoder &operator=(Decoder &&)      = delete;
            virtual ~Decoder()                  = default;

            /// Decodes up to output.size() / 2 frames. Returns the number of frames, 0 at the end of the stream.
            virtual std::expected<std::size_t, std::string> read(std::span<float> output) = 0;

            /// Continues decoding at the given frame (at the output sample rate).
            virtual std::expected<void, std::string> seek(std::uint64_t frame) = 0;

            /// Length in frames at the output sample rate, 0 if unknown
            [[nodiscard]] virtual std::uint64_t lengthFrames() const noexcept = 0;
    };
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_DECODER_HPP
