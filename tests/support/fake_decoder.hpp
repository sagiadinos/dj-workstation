// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_FAKE_DECODER_HPP
#define DJW_TESTS_SUPPORT_FAKE_DECODER_HPP

#include "decode/decoder.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace djw::tests
{
    /**
     * Decoder producing a counting ramp: frame n has the value n on both channels (exact in float up to 2^24).
     * Optionally fails after a number of frames, to test error handling.
     */
    class FakeDecoder final : public core::decode::Decoder
    {
        public:
            explicit FakeDecoder(std::size_t total_frames, std::size_t fail_after_frames = 0) noexcept
                : total_frames{total_frames}, fail_after_frames{fail_after_frames}
            {
            }

            std::expected<std::size_t, std::string> read(std::span<float> output) override
            {
                if (fail_after_frames != 0 && next_frame >= fail_after_frames)
                {
                    return std::unexpected{std::string{"fake decoder error"}};
                }
                std::size_t frames = std::min(output.size() / 2, total_frames - next_frame);
                if (fail_after_frames != 0)
                {
                    frames = std::min(frames, fail_after_frames - next_frame);
                }
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    output[frame * 2]     = static_cast<float>(next_frame);
                    output[frame * 2 + 1] = static_cast<float>(next_frame);
                    ++next_frame;
                }
                return frames;
            }

        private:
            std::size_t total_frames;
            std::size_t fail_after_frames;
            std::size_t next_frame = 0;
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_FAKE_DECODER_HPP
