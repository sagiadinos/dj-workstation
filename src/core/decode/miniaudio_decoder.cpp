// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/miniaudio_decoder.hpp"

#include <miniaudio.h>

#include <type_traits>

namespace djw::core::decode
{
    namespace
    {
        constexpr ma_uint32 output_channels = 2;

        // Paths are wchar_t on Windows and char elsewhere; only the matching miniaudio call is instantiated.
        template <typename Char>
        ma_result initFile(const Char *file, const ma_decoder_config &config, ma_decoder &decoder)
        {
            if constexpr (std::is_same_v<Char, wchar_t>)
            {
                return ma_decoder_init_file_w(file, &config, &decoder);
            }
            else
            {
                return ma_decoder_init_file(file, &config, &decoder);
            }
        }
    } // namespace

    /// Owns the ma_decoder; uninitialized exactly once, when the decoder is destroyed.
    struct MiniaudioDecoder::State
    {
            State()                         = default;
            State(const State &)            = delete;
            State &operator=(const State &) = delete;
            State(State &&)                 = delete;
            State &operator=(State &&)      = delete;
            ~State()
            {
                if (initialized)
                {
                    ma_decoder_uninit(&decoder);
                }
            }

            ma_decoder    decoder{};
            bool          initialized   = false;
            std::uint64_t length_frames = 0;
    };

    std::expected<std::unique_ptr<MiniaudioDecoder>, std::string>
    MiniaudioDecoder::open(const std::filesystem::path &path, std::uint32_t sample_rate)
    {
        auto                    state  = std::make_unique<State>();
        const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, output_channels, sample_rate);

        const ma_result result = initFile(path.c_str(), config, state->decoder);
        if (result != MA_SUCCESS)
        {
            return std::unexpected{"cannot decode " + path.string() + ": " + ma_result_description(result)};
        }
        state->initialized = true;

        // Exact for WAV and FLAC; for MP3 miniaudio scans the frames once
        ma_uint64 length = 0;
        if (ma_decoder_get_length_in_pcm_frames(&state->decoder, &length) == MA_SUCCESS)
        {
            state->length_frames = length;
        }
        return std::unique_ptr<MiniaudioDecoder>{new MiniaudioDecoder{std::move(state)}};
    }

    MiniaudioDecoder::MiniaudioDecoder(std::unique_ptr<State> state) noexcept : state{std::move(state)} {}

    MiniaudioDecoder::~MiniaudioDecoder() = default;

    std::expected<std::size_t, std::string> MiniaudioDecoder::read(std::span<float> output)
    {
        ma_uint64       frames_read = 0;
        const ma_result result =
            ma_decoder_read_pcm_frames(&state->decoder, output.data(), output.size() / output_channels, &frames_read);
        if (result != MA_SUCCESS && result != MA_AT_END)
        {
            return std::unexpected{std::string{"decoding failed: "} + ma_result_description(result)};
        }
        return static_cast<std::size_t>(frames_read);
    }

    std::expected<void, std::string> MiniaudioDecoder::seek(std::uint64_t frame)
    {
        const ma_result result = ma_decoder_seek_to_pcm_frame(&state->decoder, frame);
        if (result != MA_SUCCESS)
        {
            return std::unexpected{std::string{"seeking failed: "} + ma_result_description(result)};
        }
        return {};
    }

    std::uint64_t MiniaudioDecoder::lengthFrames() const noexcept
    {
        return state->length_frames;
    }
} // namespace djw::core::decode
