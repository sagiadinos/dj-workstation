// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_WAV_FILE_HPP
#define DJW_TESTS_SUPPORT_WAV_FILE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace djw::tests
{
    /// Interleaved stereo float32 audio, as rendered by the engine
    struct StereoAudio
    {
            std::uint32_t      sample_rate = 0;
            std::vector<float> samples;
    };

    /// Writes 32-bit float stereo WAV (the format of the golden files)
    std::expected<void, std::string> writeWav(const std::filesystem::path &path, const StereoAudio &audio);

    /// Reads a WAV file written by writeWav; other formats are rejected
    std::expected<StereoAudio, std::string> readWav(const std::filesystem::path &path);
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_WAV_FILE_HPP
