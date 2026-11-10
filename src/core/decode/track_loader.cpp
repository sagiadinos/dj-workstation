// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/track_loader.hpp"

#include "decode/memory_track.hpp"
#include "decode/miniaudio_decoder.hpp"
#include "decode/streaming_source.hpp"

namespace djw::core::decode
{
    std::unique_ptr<engine::AudioSource> makeTrack(std::unique_ptr<Decoder> decoder, const TrackLoadConfig &config)
    {
        const std::uint64_t length = decoder->lengthFrames();
        if (length > 0 && length <= config.memory_limit_frames)
        {
            auto track = MemoryTrack::create(std::move(decoder), length);
            if (track)
            {
                return std::move(*track);
            }
            decoder = std::move(track.error());
        }
        return std::make_unique<StreamingSource>(std::move(decoder), config.stream_buffer_frames);
    }

    std::expected<std::unique_ptr<engine::AudioSource>, std::string> openTrack(const std::filesystem::path &file,
                                                                               const TrackLoadConfig       &config)
    {
        auto decoder = MiniaudioDecoder::open(file, config.sample_rate);
        if (!decoder)
        {
            return std::unexpected{decoder.error()};
        }
        return makeTrack(std::move(*decoder), config);
    }
} // namespace djw::core::decode
