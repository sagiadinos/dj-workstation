// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_CONTROL_MIXXX_IMPORT_HPP
#define DJW_CORE_CONTROL_MIXXX_IMPORT_HPP

#include "control/mapping.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace djw::core::control
{
    /// What an import made of a Mixxx mapping
    struct MixxxImport
    {
            Mapping     mapping;
            std::size_t taken    = 0; ///< controls turned into ours
            std::size_t scripted = 0; ///< controls done by the mapping's script, which cannot be taken
            std::size_t unknown  = 0; ///< controls of functions djw does not have (yet)
    };

    /**
     * Turns a Mixxx controller mapping (the .midi.xml file) into one of ours: every control bound directly
     * (normal, 14-bit, inverted, select knob) to a function djw has: play, cue, headphones, keylock, load, pitch bend,
     * volume, pitch, crossfader, library browsing. Controls done by the mapping's script are counted and left out,
     * as are functions djw does not have. The mapping is named after the Mixxx one with " (Mixxx)" and is for the
     * ports whose name contains the controller's id.
     *
     * Mixxx files are XML with comments in C style between the elements; they are read leniently, element by element.
     */
    [[nodiscard]] std::expected<MixxxImport, std::string> importMixxx(std::string_view xml);

    /// A mapping as TOML text, the way parseMapping reads it
    [[nodiscard]] std::string formatMapping(const Mapping &mapping);
} // namespace djw::core::control

#endif // DJW_CORE_CONTROL_MIXXX_IMPORT_HPP
