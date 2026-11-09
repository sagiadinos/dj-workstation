// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "wav_file.hpp"

#include <bit>
#include <cstring>
#include <fstream>
#include <iterator>

namespace djw::tests
{
    namespace
    {
        static_assert(std::endian::native == std::endian::little, "WAV is little-endian, as are all targets");

        constexpr std::uint16_t format_ieee_float = 3;
        constexpr std::uint16_t channels          = 2;
        constexpr std::uint16_t bits_per_sample   = 32;
        constexpr std::uint16_t block_align       = channels * bits_per_sample / 8;

        template <typename T> void put(std::vector<char> &out, T value)
        {
            const auto *bytes = reinterpret_cast<const char *>(&value);
            out.insert(out.end(), bytes, bytes + sizeof(T));
        }

        template <typename T> T get(const std::vector<char> &in, std::size_t offset)
        {
            T value{};
            std::memcpy(&value, in.data() + offset, sizeof(T));
            return value;
        }
    } // namespace

    std::expected<void, std::string> writeWav(const std::filesystem::path &path, const StereoAudio &audio)
    {
        const auto data_size = static_cast<std::uint32_t>(audio.samples.size() * sizeof(float));

        std::vector<char> out;
        out.insert(out.end(), {'R', 'I', 'F', 'F'});
        put<std::uint32_t>(out, 36 + data_size);
        out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
        put<std::uint32_t>(out, 16);
        put<std::uint16_t>(out, format_ieee_float);
        put<std::uint16_t>(out, channels);
        put<std::uint32_t>(out, audio.sample_rate);
        put<std::uint32_t>(out, audio.sample_rate * block_align);
        put<std::uint16_t>(out, block_align);
        put<std::uint16_t>(out, bits_per_sample);
        out.insert(out.end(), {'d', 'a', 't', 'a'});
        put<std::uint32_t>(out, data_size);
        const auto *samples = reinterpret_cast<const char *>(audio.samples.data());
        out.insert(out.end(), samples, samples + data_size);

        std::filesystem::create_directories(path.parent_path());
        std::ofstream file{path, std::ios::binary};
        if (!file.write(out.data(), static_cast<std::streamsize>(out.size())))
        {
            return std::unexpected{"cannot write " + path.string()};
        }
        return {};
    }

    std::expected<StereoAudio, std::string> readWav(const std::filesystem::path &path)
    {
        std::ifstream file{path, std::ios::binary};
        if (!file)
        {
            return std::unexpected{"cannot open " + path.string()};
        }
        const std::vector<char> in{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
        if (in.size() < 12 || std::memcmp(in.data(), "RIFF", 4) != 0 || std::memcmp(in.data() + 8, "WAVE", 4) != 0)
        {
            return std::unexpected{path.string() + " is not a WAV file"};
        }

        StereoAudio audio;
        bool        format_ok = false;
        for (std::size_t offset = 12; offset + 8 <= in.size();)
        {
            const auto size = get<std::uint32_t>(in, offset + 4);
            const auto body = offset + 8;
            if (body + size > in.size())
            {
                return std::unexpected{path.string() + " is truncated"};
            }
            if (std::memcmp(in.data() + offset, "fmt ", 4) == 0 && size >= 16)
            {
                format_ok         = get<std::uint16_t>(in, body) == format_ieee_float &&
                                    get<std::uint16_t>(in, body + 2) == channels &&
                                    get<std::uint16_t>(in, body + 14) == bits_per_sample;
                audio.sample_rate = get<std::uint32_t>(in, body + 4);
            }
            else if (std::memcmp(in.data() + offset, "data", 4) == 0)
            {
                if (!format_ok)
                {
                    return std::unexpected{path.string() + " is not 32-bit float stereo"};
                }
                audio.samples.resize(size / sizeof(float));
                std::memcpy(audio.samples.data(), in.data() + body, audio.samples.size() * sizeof(float));
                return audio;
            }
            offset = body + size + (size % 2);
        }
        return std::unexpected{path.string() + " has no data chunk"};
    }
} // namespace djw::tests
