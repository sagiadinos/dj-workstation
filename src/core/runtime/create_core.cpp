// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/core.hpp"
#include "audio_io/miniaudio_device.hpp"
#include "runtime/runtime_core.hpp"

namespace djw::core::api
{
    std::expected<std::unique_ptr<Core>, std::string> createCore(const CoreConfig &config)
    {
        auto device = audio_io::MiniaudioDevice::open({
            .sample_rate      = config.sample_rate,
            .buffer_frames    = config.buffer_frames,
            .backend          = config.silent_output ? audio_io::AudioBackend::Null : audio_io::AudioBackend::System,
            .headphone_output = config.headphone_output,
        });
        if (!device)
        {
            return std::unexpected{device.error()};
        }
        return std::make_unique<runtime::RuntimeCore>(config, std::move(*device));
    }
} // namespace djw::core::api
