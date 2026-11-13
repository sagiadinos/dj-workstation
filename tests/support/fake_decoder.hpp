// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_FAKE_DECODER_HPP
#define DJW_TESTS_SUPPORT_FAKE_DECODER_HPP

#include "decode/decoder.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <thread>

namespace djw::tests
{
    struct FakeDecoderOptions
    {
            std::uint64_t total_frames = 0;
            /// Length the decoder announces; 0 means total_frames. Lets a test announce more than there is.
            std::uint64_t announced_frames = 0;
            /// Fails on the first read at or after this frame; 0 never fails
            std::uint64_t fail_at_frame = 0;
            /// Delay per read, to make decoding slow enough to watch
            std::chrono::microseconds read_delay{0};
            /// Frames one read gives at most, like a real decoder that works in packets; 0 means as many as fit
            std::size_t max_frames_per_read = 0;
            /// Every seek fails, the position stays
            bool fail_seeks = false;
    };

    /**
     * Decoder producing a counting ramp: frame n has the value n on both channels (exact in float up to 2^24).
     */
    class FakeDecoder final : public core::decode::Decoder
    {
        public:
            explicit FakeDecoder(std::uint64_t total_frames)
                : FakeDecoder{FakeDecoderOptions{.total_frames = total_frames}}
            {
            }

            explicit FakeDecoder(const FakeDecoderOptions &options) : options{options} {}

            std::expected<std::size_t, std::string> read(std::span<float> output) override
            {
                if (options.read_delay.count() > 0)
                {
                    std::this_thread::sleep_for(options.read_delay);
                }
                if (options.fail_at_frame != 0 && next_frame >= options.fail_at_frame)
                {
                    return std::unexpected{std::string{"fake decoder error"}};
                }
                std::uint64_t end = options.total_frames;
                if (options.fail_at_frame != 0)
                {
                    end = std::min(end, options.fail_at_frame);
                }
                std::size_t room = output.size() / 2;
                if (options.max_frames_per_read != 0)
                {
                    room = std::min(room, options.max_frames_per_read);
                }
                const auto frames =
                    static_cast<std::size_t>(std::min<std::uint64_t>(room, end - std::min(end, next_frame)));
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    output[frame * 2]     = static_cast<float>(next_frame);
                    output[frame * 2 + 1] = static_cast<float>(next_frame);
                    ++next_frame;
                }
                return frames;
            }

            std::expected<void, std::string> seek(std::uint64_t frame) override
            {
                seeks->fetch_add(1);
                if (options.fail_seeks)
                {
                    return std::unexpected{std::string{"fake seek error"}};
                }
                next_frame = std::min(frame, options.total_frames);
                return {};
            }

            std::uint64_t lengthFrames() const noexcept override
            {
                return options.announced_frames != 0 ? options.announced_frames : options.total_frames;
            }

            /// Number of seeks, readable after the decoder has been handed over
            std::shared_ptr<std::atomic<int>> seekCounter() const { return seeks; }

        private:
            FakeDecoderOptions                options;
            std::uint64_t                     next_frame = 0;
            std::shared_ptr<std::atomic<int>> seeks      = std::make_shared<std::atomic<int>>(0);
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_FAKE_DECODER_HPP
