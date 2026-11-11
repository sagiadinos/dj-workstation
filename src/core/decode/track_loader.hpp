// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_TRACK_LOADER_HPP
#define DJW_CORE_DECODE_TRACK_LOADER_HPP

#include "decode/decoder.hpp"
#include "engine/audio_source.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace djw::core::decode
{
    struct TrackLoadConfig
    {
            std::uint32_t sample_rate = 48'000;
            /// Tracks up to this length are held in memory, longer ones are streamed (Architecture.md, Track loading)
            std::uint64_t memory_limit_frames = 600 * std::uint64_t{48'000};
            /// Ring buffer of a streamed track
            std::size_t stream_buffer_frames = 48'000;
            /// How far into a file findFirstSound looks for the first audible sound
            std::uint64_t first_sound_search_frames = 30 * std::uint64_t{48'000};
    };

    /// Turns an opened decoder into a track: in memory if its length is known and within the limit (and the memory
    /// can be allocated), streamed otherwise.
    std::unique_ptr<engine::AudioSource> makeTrack(std::unique_ptr<Decoder> decoder, const TrackLoadConfig &config);

    /// Opens an audio file and makes a track of it, see makeTrack.
    std::expected<std::unique_ptr<engine::AudioSource>, std::string> openTrack(const std::filesystem::path &file,
                                                                               const TrackLoadConfig       &config);

    /// The first audible frame of an audio file (see findFirstSound), on a decoder of its own; std::nullopt if the
    /// file cannot be opened or the first sound is not known.
    std::optional<std::uint64_t> findFirstSound(const std::filesystem::path &file, const TrackLoadConfig &config);
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_TRACK_LOADER_HPP
