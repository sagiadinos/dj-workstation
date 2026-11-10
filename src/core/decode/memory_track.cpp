// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/memory_track.hpp"

#include <algorithm>
#include <new>

namespace djw::core::decode
{
    namespace
    {
        constexpr std::size_t stereo = engine::stereo_channels;
    } // namespace

    std::expected<std::unique_ptr<MemoryTrack>, std::unique_ptr<Decoder>>
    MemoryTrack::create(std::unique_ptr<Decoder> decoder, std::uint64_t length_frames, std::size_t chunk_frames)
    {
        std::vector<float> samples;
        try
        {
            samples.resize(length_frames * stereo);
        }
        catch (const std::bad_alloc &)
        {
            return std::unexpected{std::move(decoder)};
        }
        const Layout layout{.length_frames = length_frames, .chunk_frames = std::max<std::size_t>(chunk_frames, 1)};
        return std::unique_ptr<MemoryTrack>{new MemoryTrack{std::move(decoder), layout, std::move(samples)}};
    }

    MemoryTrack::MemoryTrack(std::unique_ptr<Decoder> decoder, Layout layout, std::vector<float> samples)
        : decoder{std::move(decoder)}, length{layout.length_frames}, chunk_frames{layout.chunk_frames},
          samples{std::move(samples)}, ready(chunkCount())
    {
        if (chunkCount() > 0)
        {
            decodeChunk(0);
        }
        worker = std::thread{[this] { run(); }};
    }

    MemoryTrack::~MemoryTrack()
    {
        stop_requested.store(true, std::memory_order_relaxed);
        if (worker.joinable())
        {
            worker.join();
        }
    }

    std::size_t MemoryTrack::read(std::span<float> output) noexcept // [audio-thread]
    {
        const std::size_t wanted = output.size() / stereo;
        std::size_t       copied = 0;
        while (copied < wanted && read_position < length)
        {
            const auto chunk = static_cast<std::size_t>(read_position / chunk_frames);
            if (!ready[chunk].load(std::memory_order_acquire))
            {
                wanted_chunk.store(chunk, std::memory_order_relaxed);
                break;
            }
            const std::uint64_t chunk_end = std::min<std::uint64_t>((chunk + 1) * std::uint64_t{chunk_frames}, length);
            const auto          frames =
                static_cast<std::size_t>(std::min<std::uint64_t>(wanted - copied, chunk_end - read_position));
            std::copy_n(samples.data() + read_position * stereo, frames * stereo,
                        output.begin() + static_cast<std::ptrdiff_t>(copied * stereo));
            copied += frames;
            read_position += frames;
        }
        return copied;
    }

    void MemoryTrack::seek(std::uint64_t frame) noexcept // [audio-thread]
    {
        read_position = std::min(frame, length);
        if (read_position < length)
        {
            const auto chunk = static_cast<std::size_t>(read_position / chunk_frames);
            if (!ready[chunk].load(std::memory_order_acquire))
            {
                wanted_chunk.store(chunk, std::memory_order_relaxed);
            }
        }
    }

    bool MemoryTrack::isDecoded(std::uint64_t frame) const noexcept
    {
        return frame < length && ready[static_cast<std::size_t>(frame / chunk_frames)].load(std::memory_order_acquire);
    }

    bool MemoryTrack::fullyDecoded() const noexcept
    {
        for (std::size_t chunk = 0; chunk < chunkCount(); ++chunk)
        {
            if (!ready[chunk].load(std::memory_order_acquire))
            {
                return false;
            }
        }
        return true;
    }

    std::size_t MemoryTrack::chunkCount() const noexcept
    {
        return static_cast<std::size_t>((length + chunk_frames - 1) / chunk_frames);
    }

    std::size_t MemoryTrack::chunkLength(std::size_t chunk) const noexcept
    {
        const std::uint64_t start = std::uint64_t{chunk} * chunk_frames;
        return static_cast<std::size_t>(std::min<std::uint64_t>(chunk_frames, length - start));
    }

    std::size_t MemoryTrack::nextChunk() noexcept
    {
        // 1. what the audio thread is waiting for, 2. the chunk the decoder is at (no seek needed),
        // 3. the first gap from the start
        const std::size_t wanted = wanted_chunk.load(std::memory_order_relaxed);
        if (wanted < chunkCount() && !ready[wanted].load(std::memory_order_relaxed))
        {
            return wanted;
        }
        if (decoder_chunk < chunkCount() && !ready[decoder_chunk].load(std::memory_order_relaxed))
        {
            return decoder_chunk;
        }
        while (first_missing < chunkCount() && ready[first_missing].load(std::memory_order_relaxed))
        {
            ++first_missing;
        }
        return first_missing < chunkCount() ? first_missing : no_chunk;
    }

    void MemoryTrack::decodeChunk(std::size_t chunk)
    {
        float            *target = samples.data() + std::uint64_t{chunk} * chunk_frames * stereo;
        const std::size_t frames = chunkLength(chunk);
        std::size_t       filled = 0;

        bool usable = true;
        if (chunk != decoder_chunk)
        {
            usable = decoder->seek(std::uint64_t{chunk} * chunk_frames).has_value();
        }
        while (usable && filled < frames)
        {
            const auto decoded = decoder->read(std::span{target + filled * stereo, (frames - filled) * stereo});
            if (!decoded)
            {
                usable = false;
                break;
            }
            if (*decoded == 0)
            {
                break; // the stream is shorter than its announced length
            }
            filled += *decoded;
        }
        if (!usable)
        {
            decode_failed.store(true, std::memory_order_release);
        }
        std::fill(target + filled * stereo, target + frames * stereo, 0.0F);

        ready[chunk].store(true, std::memory_order_release);
        decoder_chunk = chunk + 1;
    }

    void MemoryTrack::run()
    {
        while (!stop_requested.load(std::memory_order_relaxed))
        {
            const std::size_t chunk = nextChunk();
            if (chunk == no_chunk)
            {
                return;
            }
            decodeChunk(chunk);
        }
    }
} // namespace djw::core::decode
