// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_MINIAUDIO_DECODER_HPP
#define DJW_CORE_DECODE_MINIAUDIO_DECODER_HPP

#include "decode/decoder.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace djw::core::decode
{
    /**
     * Decoder for WAV, FLAC and MP3 based on miniaudio.
     *
     * miniaudio also converts to stereo and resamples to the requested rate. miniaudio.h stays out of this header,
     * the decoder state lives in the .cpp.
     */
    class MiniaudioDecoder final : public Decoder
    {
        public:
            static std::expected<std::unique_ptr<MiniaudioDecoder>, std::string> open(const std::filesystem::path &path,
                                                                                      std::uint32_t sample_rate);

            ~MiniaudioDecoder() override;

            std::expected<std::size_t, std::string> read(std::span<float> output) override;
            std::expected<void, std::string>        seek(std::uint64_t frame) override;
            [[nodiscard]] std::uint64_t             lengthFrames() const noexcept override;

        private:
            struct State;

            explicit MiniaudioDecoder(std::unique_ptr<State> state) noexcept;

            std::unique_ptr<State> state;
    };
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_MINIAUDIO_DECODER_HPP
