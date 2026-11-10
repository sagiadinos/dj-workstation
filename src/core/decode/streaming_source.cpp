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
        : decoder{std::move(decoder)}, length{this->decoder->lengthFrames()}, ring{buffer_frames * stereo},
          chunk(std::clamp(ring.capacity() / stereo / 4, std::size_t{1}, chunk_frames) * stereo)
    {
        while (fillChunk())
        {
        }
        // Started even if everything is decoded already: a seek needs the worker
        worker = std::thread{[this] { run(); }};
    }

    StreamingSource::~StreamingSource()
    {
        stop_requested.store(true, std::memory_order_relaxed);
        if (worker.joinable())
        {
            worker.join();
        }
    }

    std::size_t StreamingSource::read(std::span<float> output) noexcept // [audio-thread]
    {
        if (!seekSettled())
        {
            return 0;
        }
        // The worker writes whole frames, so the fill level is always a whole number of frames
        return ring.read(output.first(output.size() / stereo * stereo)) / stereo;
    }

    void StreamingSource::seek(std::uint64_t frame) noexcept // [audio-thread]
    {
        seek_frame.store(length > 0 ? std::min(frame, length) : frame, std::memory_order_relaxed);
        seek_generation.store(++requested_generation, std::memory_order_release);
        ring.discard(ring.readAvailable());
    }

    bool StreamingSource::atEnd() const noexcept // [audio-thread]
    {
        return answered_generation.load(std::memory_order_acquire) == requested_generation &&
               decoder_done.load(std::memory_order_acquire) && ring.readAvailable() == 0;
    }

    bool StreamingSource::seekSettled() noexcept // [audio-thread]
    {
        if (answered_generation.load(std::memory_order_acquire) != requested_generation)
        {
            ring.discard(ring.readAvailable());
            return false;
        }
        // Audio written before the worker moved the decoder is from the old position
        const std::size_t stale = stale_until.load(std::memory_order_relaxed);
        if (ring.totalRead() < stale)
        {
            ring.discard(stale - ring.totalRead());
        }
        return ring.totalRead() >= stale;
    }

    void StreamingSource::handleSeek()
    {
        const std::uint32_t generation = seek_generation.load(std::memory_order_acquire);
        if (generation == handled_generation)
        {
            return;
        }
        const auto moved = decoder->seek(seek_frame.load(std::memory_order_relaxed));
        decode_failed.store(!moved, std::memory_order_relaxed);
        decoder_done.store(!moved, std::memory_order_relaxed);
        stale_until.store(ring.totalWritten(), std::memory_order_relaxed);
        handled_generation = generation;
        answered_generation.store(generation, std::memory_order_release);
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
        // Fits: there was room for a whole chunk and only this thread writes
        static_cast<void>(ring.write(std::span{chunk}.first(*frames * stereo)));
        return true;
    }

    void StreamingSource::run()
    {
        while (!stop_requested.load(std::memory_order_relaxed))
        {
            handleSeek();
            if (!fillChunk())
            {
                std::this_thread::sleep_for(poll_interval);
            }
        }
    }
} // namespace djw::core::decode
