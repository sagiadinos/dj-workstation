// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/time_stretcher.hpp"

namespace djw::core::engine
{
    std::unique_ptr<TimeStretcher> makeTimeStretcher(api::TimeStretch library, std::uint32_t sample_rate)
    {
        switch (library)
        {
            case api::TimeStretch::Signalsmith:
#ifdef DJW_WITH_SIGNALSMITH_STRETCH
                return makeSignalsmithStretcher(sample_rate);
#else
                break;
#endif
            case api::TimeStretch::SoundTouch:
                break;
        }
        return makeSoundTouchStretcher(sample_rate);
    }

    std::vector<api::TimeStretch> builtTimeStretchers()
    {
        std::vector<api::TimeStretch> built{api::TimeStretch::SoundTouch};
#ifdef DJW_WITH_SIGNALSMITH_STRETCH
        built.push_back(api::TimeStretch::Signalsmith);
#endif
        return built;
    }
} // namespace djw::core::engine
