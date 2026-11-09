// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_GOLDEN_HPP
#define DJW_TESTS_SUPPORT_GOLDEN_HPP

#include "wav_file.hpp"

#include <filesystem>
#include <string>

namespace djw::tests
{
    /// Default per-sample tolerance, see Architecture.md (about -100 dBFS)
    inline constexpr float golden_tolerance = 1e-5F;

    struct GoldenResult
    {
            bool        matches = false;
            std::string message;
    };

    /**
     * Compares rendered audio with a golden WAV file, allowing a maximum absolute deviation per sample.
     *
     * With the environment variable DJW_UPDATE_GOLDEN=1 the golden file is (re)written from the rendered audio
     * instead; do that on the reference platform only and commit the file with the change that caused it.
     */
    GoldenResult compareWithGolden(const StereoAudio &rendered, const std::filesystem::path &golden_file,
                                   float tolerance = golden_tolerance);
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_GOLDEN_HPP
