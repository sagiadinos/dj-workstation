// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/streaming_source.hpp"

#include <algorithm>
#include <chrono>

namespace djw::core::decode
{
    namespace
    {
        constexpr std::size_t stereo        = engine::stereo_channels;
        constexpr std::size_t chunk_frames  = 4'096;
        constexpr auto        poll_interval = std::chrono::milliseconds{2};
    } // namespace

    StreamingSource::StreamingSource(std::unique_ptr<Decoder> decoder, std::size_t buffer_frames)
        : decoder{std::move(decoder)}, ring{buffer_frames * stereo},
          chunk(std::clamp(ring.capacity() / stereo / 4, std::size_t{1}, chunk_frames) * stereo)
    {
        while (fillChunk())
        {
        }
        if (!decoder_done.load(std::memory_order_relaxed))
        {
            worker = std::jthread{[this](const std::stop_token &stop) { run(stop); }};
        }
    }

    StreamingSource::~StreamingSource() = default;

    std::size_t StreamingSource::read(std::span<float> output) noexcept // [audio-thread]
    {
        // The worker writes whole frames, so the fill level is always a whole number of frames
        return ring.read(output.first(output.size() / stereo * stereo)) / stereo;
    }

    bool StreamingSource::endOfStream() const noexcept
    {
        return decoder_done.load(std::memory_order_acquire) && ring.readAvailable() == 0;
    }

    bool StreamingSource::fillChunk()
    {
        if (decoder_done.load(std::memory_order_relaxed) || ring.writeAvailable() < chunk.size())
        {
            return false;
        }

        const auto frames = decoder->read(chunk);
        if (!frames || *frames == 0)
        {
            decode_failed.store(!frames, std::memory_order_release);
            decoder_done.store(true, std::memory_order_release);
            return false;
        }
        ring.write(std::span{chunk}.first(*frames * stereo));
        return true;
    }

    void StreamingSource::run(const std::stop_token &stop)
    {
        while (!stop.stop_requested() && !decoder_done.load(std::memory_order_relaxed))
        {
            if (!fillChunk())
            {
                std::this_thread::sleep_for(poll_interval);
            }
        }
    }
} // namespace djw::core::decode
