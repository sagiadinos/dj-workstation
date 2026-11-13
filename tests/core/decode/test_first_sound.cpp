// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/first_sound.hpp"
#include "decode/track_loader.hpp"

#include "temp_dir.hpp"
#include "wav_file.hpp"

#include "fake_decoder.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using djw::core::decode::Decoder;
using djw::core::decode::findFirstSound;
using djw::core::decode::silence_threshold;
using djw::core::decode::TrackLoadConfig;

namespace
{
    /// Silence (a constant level) up to sound_at, then a loud tone; optionally fails after some frames
    class GapDecoder final : public Decoder
    {
        public:
            GapDecoder(std::uint64_t sound_at, std::uint64_t total, float silence_level = 0.0F,
                       std::uint64_t fail_at = 0)
                : sound_at{sound_at}, total{total}, level{silence_level}, fail_at{fail_at}
            {
            }

            std::expected<std::size_t, std::string> read(std::span<float> output) override
            {
                if (fail_at != 0 && next >= fail_at)
                {
                    return std::unexpected{std::string{"decoder error"}};
                }
                const auto frames =
                    static_cast<std::size_t>(std::min<std::uint64_t>(output.size() / 2, total - std::min(total, next)));
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    const float value =
                        next < sound_at ? level
                                        : static_cast<float>(0.5 * std::sin(0.05 * static_cast<double>(next) + 1.0));
                    output[frame * 2]       = value;
                    output[(frame * 2) + 1] = value;
                    ++next;
                }
                return frames;
            }

            std::expected<void, std::string> seek(std::uint64_t frame) override
            {
                next = frame;
                ++seeks;
                return {};
            }

            [[nodiscard]] std::uint64_t lengthFrames() const noexcept override { return total; }

            std::uint64_t next  = 0;
            int           seeks = 0;

        private:
            std::uint64_t sound_at;
            std::uint64_t total;
            float         level;
            std::uint64_t fail_at;
    };

    constexpr std::uint64_t limit = 1'000'000;
} // namespace

TEST_CASE("findFirstSound: the first frame that is audible, also across the blocks it reads")
{
    for (const std::uint64_t sound_at : {1ULL, 1'000ULL, 4'095ULL, 4'096ULL, 4'097ULL, 12'345ULL, 100'000ULL})
    {
        CAPTURE(sound_at);
        GapDecoder decoder{sound_at, 200'000};
        CHECK(findFirstSound(decoder, limit) == sound_at);
    }
}

TEST_CASE("findFirstSound: sound at the very start is frame 0")
{
    GapDecoder decoder{0, 10'000};
    CHECK(findFirstSound(decoder, limit) == 0);
}

TEST_CASE("findFirstSound: it starts at the beginning wherever the decoder was")
{
    GapDecoder decoder{5'000, 20'000};
    static_cast<void>(decoder.seek(15'000));
    decoder.seeks = 0;
    CHECK(findFirstSound(decoder, limit) == 5'000);
    CHECK(decoder.seeks >= 1);
}

TEST_CASE("findFirstSound: a level below the threshold is silence, at the threshold it is sound")
{
    GapDecoder quiet{8'000, 20'000, silence_threshold * 0.9F};
    CHECK(findFirstSound(quiet, limit) == 8'000);

    GapDecoder at_threshold{20'000, 20'000, silence_threshold}; // the "silence" itself is audible
    CHECK(findFirstSound(at_threshold, limit) == 0);

    GapDecoder negative{3'000, 20'000, -(silence_threshold * 2.0F)};
    CHECK(findFirstSound(negative, limit) == 0); // a negative level counts as well
}

TEST_CASE("findFirstSound: only one channel may carry the sound")
{
    /// Sound from frame 777 on, in one channel only
    class OneChannel final : public Decoder
    {
        public:
            explicit OneChannel(std::size_t channel) : channel{channel} {}

            std::expected<std::size_t, std::string> read(std::span<float> output) override
            {
                const auto frames =
                    std::min<std::size_t>(output.size() / 2, 10'000 - std::min<std::uint64_t>(10'000, next));
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    output[frame * 2]             = 0.0F;
                    output[(frame * 2) + 1]       = 0.0F;
                    output[(frame * 2) + channel] = next >= 777 ? 0.3F : 0.0F;
                    ++next;
                }
                return frames;
            }
            std::expected<void, std::string> seek(std::uint64_t frame) override
            {
                next = frame;
                return {};
            }
            [[nodiscard]] std::uint64_t lengthFrames() const noexcept override { return 10'000; }
            std::uint64_t               next = 0;

        private:
            std::size_t channel;
    };
    OneChannel left{0};
    CHECK(findFirstSound(left, limit) == 777);
    OneChannel right{1};
    CHECK(findFirstSound(right, limit) == 777);
}

TEST_CASE("findFirstSound: the limit counts exactly, also inside a block")
{
    // 50'000 is not a multiple of the block size, so the last block read is a short one
    GapDecoder at_limit{50'000, 100'000};
    CHECK_FALSE(findFirstSound(at_limit, 50'000).has_value());
    GapDecoder before_limit{49'999, 100'000};
    CHECK(findFirstSound(before_limit, 50'000) == 49'999);
}

TEST_CASE("findFirstSound: nothing is known when there is only silence, the sound comes too late or the decoder fails")
{
    GapDecoder silent{1'000'000, 50'000};
    CHECK_FALSE(findFirstSound(silent, limit).has_value());

    GapDecoder late{60'000, 100'000};
    CHECK_FALSE(findFirstSound(late, 50'000).has_value()); // beyond what is searched
    GapDecoder late_again{60'000, 100'000};
    CHECK(findFirstSound(late_again, 60'001) == 60'000);

    GapDecoder failing{40'000, 100'000, 0.0F, 10'000};
    CHECK_FALSE(findFirstSound(failing, limit).has_value());

    GapDecoder empty{0, 0};
    CHECK_FALSE(findFirstSound(empty, limit).has_value());
}

TEST_CASE("findFirstSound: the search stops at the limit and reads no further")
{
    GapDecoder decoder{900'000, 2'000'000};
    CHECK_FALSE(findFirstSound(decoder, 10'000).has_value());
    CHECK(decoder.next <= 10'000 + 4'096); // it did not go through the whole track
}

TEST_CASE("findFirstSound: in a file, on a decoder of its own")
{
    const djw::tests::TempDir folder;
    djw::tests::StereoAudio   audio{.sample_rate = 48'000, .samples = std::vector<float>(48'000 * 2 * 2, 0.0F)};
    for (std::size_t frame = 48'000; frame < 96'000; ++frame) // silence for one second, then a tone
    {
        const auto value =
            static_cast<float>(0.4 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / 48'000.0));
        audio.samples[frame * 2]       = value;
        audio.samples[(frame * 2) + 1] = value;
    }
    const auto file = folder.path() / "gap.wav";
    REQUIRE(djw::tests::writeWav(file, audio).has_value());

    const TrackLoadConfig config{};
    const auto            found = findFirstSound(file, config);
    REQUIRE(found.has_value());
    CHECK(*found >= 48'000);
    CHECK(*found < 48'000 + 200); // the sine starts at zero: the first frames are still below the threshold

    CHECK_FALSE(findFirstSound(folder.path() / "missing.wav", config).has_value());

    TrackLoadConfig short_search{};
    short_search.first_sound_search_frames = 20'000;
    CHECK_FALSE(findFirstSound(file, short_search).has_value());
}

TEST_CASE("findFirstSound: a decoder that cannot go to the start finds nothing")
{
    djw::tests::FakeDecoder decoder{{.total_frames = 10'000, .fail_seeks = true}}; // sound from frame 1 on
    CHECK_FALSE(findFirstSound(decoder, limit).has_value());
}
