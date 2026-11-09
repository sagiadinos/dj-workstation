// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/miniaudio_decoder.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

using djw::core::decode::MiniaudioDecoder;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};

    constexpr std::size_t file_frames = 12'000; // 0.25 s at 48 kHz
    // ffmpeg's sine source has 1/8; making it stereo (-ac 2) lowers it by 3 dB
    constexpr double mono_amplitude   = 0.125;
    constexpr double stereo_amplitude = mono_amplitude / std::numbers::sqrt2;

    std::vector<float> decodeAll(const std::filesystem::path &file, std::uint32_t sample_rate)
    {
        auto decoder = MiniaudioDecoder::open(file, sample_rate);
        REQUIRE_MESSAGE(decoder.has_value(), decoder.error());

        std::vector<float> all;
        std::vector<float> block(1'000 * 2);
        while (true)
        {
            const auto frames = (*decoder)->read(block);
            REQUIRE(frames.has_value());
            if (*frames == 0)
            {
                break;
            }
            all.insert(all.end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(*frames * 2));
        }
        return all;
    }

    bool channelsEqual(const std::vector<float> &samples)
    {
        for (std::size_t i = 0; i < samples.size(); i += 2)
        {
            if (samples[i] != samples[i + 1])
            {
                return false;
            }
        }
        return true;
    }

    double rms(const std::vector<float> &samples)
    {
        double sum = 0.0;
        for (const float value : samples)
        {
            sum += static_cast<double>(value) * value;
        }
        return std::sqrt(sum / static_cast<double>(samples.size()));
    }

    /// RMS of a sine with the given amplitude, within a relative tolerance
    bool rmsMatches(const std::vector<float> &samples, double amplitude, double tolerance)
    {
        const double expected = amplitude / std::numbers::sqrt2;
        return std::abs(rms(samples) - expected) <= tolerance * expected;
    }
} // namespace

TEST_CASE("MiniaudioDecoder: WAV is decoded to the expected samples")
{
    const auto samples = decodeAll(audio_dir / "sine_440hz_48k_stereo.wav", 48'000);

    REQUIRE(samples.size() == file_frames * 2);
    CHECK(channelsEqual(samples));
    // 16 bit quantization: one step is 1/32768
    double max_error = 0.0;
    for (std::size_t frame = 0; frame < file_frames; ++frame)
    {
        const double expected = stereo_amplitude * std::sin(2.0 * std::numbers::pi * 440.0 * frame / 48'000.0);
        max_error             = std::max(max_error, std::abs(samples[frame * 2] - expected));
    }
    CHECK(max_error < 2.0 / 32'768.0);
}

TEST_CASE("MiniaudioDecoder: FLAC gives exactly the samples of the WAV file")
{
    CHECK(decodeAll(audio_dir / "sine_440hz_48k_stereo.flac", 48'000) ==
          decodeAll(audio_dir / "sine_440hz_48k_stereo.wav", 48'000));
}

TEST_CASE("MiniaudioDecoder: MP3 has the length and level of the source")
{
    const auto samples = decodeAll(audio_dir / "sine_440hz_48k_stereo.mp3", 48'000);

    // MP3 adds encoder padding of up to two frames of 1152 samples
    CHECK(samples.size() >= file_frames * 2);
    CHECK(samples.size() <= (file_frames + 2 * 1'152) * 2);
    INFO("rms " << rms(samples) << ", frames " << samples.size() / 2);
    // Lossy: the file is about 5 % below the source level (ffmpeg decodes it to the same RMS as miniaudio)
    CHECK(rmsMatches(samples, stereo_amplitude, 0.1));
}

TEST_CASE("MiniaudioDecoder: mono 44.1 kHz is converted to stereo at the requested rate")
{
    const auto samples = decodeAll(audio_dir / "sine_440hz_44k1_mono.wav", 48'000);

    const auto frames = samples.size() / 2;
    CHECK(frames >= file_frames - 50);
    CHECK(frames <= file_frames + 50);
    CHECK(channelsEqual(samples));
    INFO("rms " << rms(samples));
    CHECK(rmsMatches(samples, mono_amplitude, 0.02));
}

TEST_CASE("MiniaudioDecoder: a missing file is an error naming the file")
{
    const auto decoder = MiniaudioDecoder::open(audio_dir / "does_not_exist.wav", 48'000);
    REQUIRE_FALSE(decoder.has_value());
    CHECK(decoder.error().find("does_not_exist.wav") != std::string::npos);
}

TEST_CASE("MiniaudioDecoder: a file that is not audio is an error")
{
    const auto path = std::filesystem::temp_directory_path() / "djw_test_not_audio.mp3";
    {
        std::ofstream file{path};
        file << "this is not audio\n";
    }
    const auto decoder = MiniaudioDecoder::open(path, 48'000);
    std::filesystem::remove(path);
    CHECK_FALSE(decoder.has_value());
}
