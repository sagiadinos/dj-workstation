// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_READ_HELPERS_HPP
#define DJW_TESTS_SUPPORT_READ_HELPERS_HPP

#include "engine/audio_source.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

namespace djw::tests
{
    /// Reads frames frames like the audio thread would, waiting while the decoder catches up; at most 10 s.
    inline std::vector<float> readFrames(core::engine::AudioSource &source, std::size_t frames,
                                         std::size_t block_frames = 256)
    {
        std::vector<float> all;
        std::vector<float> block(block_frames * 2);
        const auto         deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (all.size() < frames * 2 && !source.atEnd() && std::chrono::steady_clock::now() < deadline)
        {
            const std::size_t wanted = std::min(block_frames, frames - all.size() / 2);
            const std::size_t read   = source.read(std::span{block}.first(wanted * 2));
            all.insert(all.end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(read * 2));
            if (read == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        }
        return all;
    }

    /// Reads until the end of the source
    inline std::vector<float> readAll(core::engine::AudioSource &source)
    {
        return readFrames(source, static_cast<std::size_t>(-1) / 4);
    }

    /// The samples continue the ramp of FakeDecoder from first_frame on
    inline bool isRampFrom(const std::vector<float> &samples, std::size_t first_frame)
    {
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            if (samples[i] != static_cast<float>(first_frame + i / 2))
            {
                return false;
            }
        }
        return true;
    }
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_READ_HELPERS_HPP
