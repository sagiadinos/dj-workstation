// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_CONTROLLERS_HPP
#define DJW_CORE_API_CONTROLLERS_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace djw::core::api
{
    /// What a control of a controller does; the frontend carries it out (it knows the players and the library)
    enum class ControlAction : std::uint8_t
    {
        Shift,              ///< held: the controls with shift = true apply instead of the others
        DeckPlay,           ///< play/pause
        DeckCue,            ///< CUE, acts while held
        DeckLoad,           ///< loads the track selected in the library
        DeckKeylock,        ///< keylock on/off
        DeckHeadphones,     ///< headphone cue on/off
        DeckBendDown,       ///< pitch bend down while held
        DeckBendUp,         ///< pitch bend up while held
        DeckPitch,          ///< pitch fader, absolute
        DeckVolume,         ///< channel fader, absolute
        DeckHeadphoneLevel, ///< headphone level of the deck, absolute
        DeckSeek,           ///< steps back or ahead (relative), like the cursor keys of a player
        Crossfader,         ///< absolute, 0 side A to 1 side B
        LibraryMove,        ///< moves the selection in the library (relative)
    };

    /// A control of a controller was used
    struct ControlEvent
    {
            ControlAction              action = ControlAction::DeckPlay;
            std::optional<std::size_t> deck; ///< the player, from 0
            /// Button: pressed (true) or released; the other modes: true
            bool pressed = true;
            /// Absolute modes: 0 to 1
            float value = 0.0F;
            /// Relative mode: steps, negative back
            int steps = 0;

            bool operator==(const ControlEvent &) const = default;
    };

    /// What a player's lights on a controller show
    struct DeckLights
    {
            bool playing    = false; ///< the play light
            bool cue        = false; ///< the CUE light, already blinking where a CDJ blinks it
            bool headphones = false; ///< the headphone cue light

            bool operator==(const DeckLights &) const = default;
    };

    /**
     * The MIDI controllers: their ports, the mappings found and which port uses which, and the events of the
     * connected ones. Messages are turned into events on MIDI threads; the frontend takes them once per frame with
     * poll() and carries them out. All other methods are called from the frontend's thread.
     */
    class Controllers
    {
        public:
            /// Which mapping a port uses, by the port's stable name: a mapping's name, or empty for none
            using Assignments = std::map<std::string, std::string>;

            /// A port in use (by its stable name) and the mapping it uses
            struct Connection
            {
                    std::string port;
                    std::string mapping;
            };

            /// What an import of a mapping from another program made of it
            struct Imported
            {
                    std::string name;         ///< of the new mapping
                    std::size_t taken    = 0; ///< controls taken over
                    std::size_t scripted = 0; ///< controls done by the other program's script, left out
                    std::size_t unknown  = 0; ///< controls of functions djw does not have, left out
            };

            Controllers()                               = default;
            Controllers(const Controllers &)            = delete;
            Controllers &operator=(const Controllers &) = delete;
            Controllers(Controllers &&)                 = delete;
            Controllers &operator=(Controllers &&)      = delete;
            virtual ~Controllers()                      = default;

            /// Opens every port with a mapping (the one assigned, else the first that matches); the errors
            virtual std::vector<std::string> connect(const Assignments &assigned = {}) = 0;
            /// The events since the last call, in the order they came
            [[nodiscard]] virtual std::vector<ControlEvent> poll() = 0;
            /// The MIDI input ports there are now, by their stable names (without the numbers ALSA adds)
            [[nodiscard]] virtual std::vector<std::string> ports() const = 0;
            /// The names of all mappings found
            [[nodiscard]] virtual std::vector<std::string> mappingNames() const = 0;
            /// The mapping a port gets without an assignment; empty if none matches
            [[nodiscard]] virtual std::string mappingFor(const std::string &port) const = 0;
            /// The ports in use and their mappings
            [[nodiscard]] virtual std::vector<Connection> connections() const = 0;
            /// The sound card of the connected controllers (part of its name, from their mappings); empty if none has
            /// one, e.g. "DJControl Starlight" when a Starlight is connected
            [[nodiscard]] virtual std::string controllerAudio() const = 0;
            /// Turns a Mixxx mapping (.midi.xml) into one of ours, saves it in the user's mapping folder (the last
            /// folder given) and reads all mappings again; connect() then puts it to use
            virtual std::expected<Imported, std::string> importMixxx(const std::filesystem::path &file) = 0;
            /// The players' state for the controllers' lights, the first player first; a light is sent only when it
            /// changes. Called once per frame from the frontend's thread.
            virtual void setLights(const std::vector<DeckLights> &decks) = 0;
    };

    /// The controllers of the system (RtMidi) with the mappings (*.toml) of the folders, first folder first. Files
    /// that cannot be read are skipped and go to errors. Nothing is connected yet (connect()).
    [[nodiscard]] std::unique_ptr<Controllers>
    openControllers(const std::vector<std::filesystem::path> &mapping_folders, std::vector<std::string> &errors);
} // namespace djw::core::api

#endif // DJW_CORE_API_CONTROLLERS_HPP
