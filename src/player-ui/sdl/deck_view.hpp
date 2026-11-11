// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_DECK_VIEW_HPP
#define DJW_PLAYER_UI_SDL_DECK_VIEW_HPP

#include "api/commands.hpp"
#include "api/deck_id.hpp"
#include "api/snapshot.hpp"
#include "canvas.hpp"
#include "check_box.hpp"
#include "clock.hpp"
#include "timed_state.hpp"
#include "fader.hpp"
#include "i18n/translator.hpp"
#include "play_button.hpp"
#include "pointer_router.hpp"
#include "text_button.hpp"
#include "view.hpp"
#include "waveform_bar.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace djw::player_ui::sdl
{
    /**
     * The player window of one deck.
     *
     * Top: the Load button (it asks the owner to load the track selected in the library into this player; off while
     * the player plays), pitch bend - and + (held), KEY (keylock) and the vertical pitch fader on the right; a double
     * tap on the fader sets the pitch to 0. In the black display: the Time button (elapsed or remaining time), the
     * pitch, the large time and the tempo (BPM, with the pitch taken into account). Below it the waveform of the whole
     * track with the play position and the cue point, which also seeks when touched, then the state (loaded, cued,
     * paused, playing), CUE (red, acts while held like on a CDJ) and play/pause (green). At the bottom the channel
     * fader, the headphone level fader with its HP (headphone cue) button, the artist and title display and the
     * Cue-Load option: a track loaded into this player starts at its cue point (see cueLoad(); the owner does the
     * loading). When the user sets a cue point with CUE, the owner is told to store it.
     *
     * Turns input into commands for the core and shows the deck's snapshot. Commands that matter in order (play,
     * CUE press and release, bend, ...) wait in a list and go out one after the other as the queue has room, so none
     * is lost; faders send their latest value.
     *
     * The layout is made for design_width x design_height and scales with the window: every element grows with the
     * window in both directions, text included, so the player looks the same at any size.
     */
    class DeckView final : public View
    {
        public:
            /// The size the layout is drawn for; the window's default size
            static constexpr float design_width  = 380.0F;
            static constexpr float design_height = 514.0F;

            using Load   = std::function<void()>;
            using CueSet = std::function<void(double seconds)>;

            /// How long a notice (see setNotice) replaces the state
            static constexpr std::chrono::milliseconds notice_time{3'000};

            DeckView(core::api::DeckId deck, core::api::CommandQueue &queue, core::api::DeckSnapshotBuffer &snapshot,
                     const Translator &translator, Clock clock = steadyClock());

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;

            /// Once per frame: reads the snapshot and sends what the input asked for.
            void update() override;

            void draw(Canvas &canvas) const override;

            /// What the Load button does
            void setOnLoad(Load load) { on_load = std::move(load); }

            /// Called when the user sets the cue point with the CUE button, with the new cue point in seconds (a cue
            /// point that came with a loaded track is no call)
            void setOnCueSet(CueSet cue_set) { on_cue_set = std::move(cue_set); }

            /// A track was loaded: shown in the display
            void setTrack(std::string artist, std::string title);
            /// The overview waveform of the loaded track (values 0 to 255), empty if there is none
            void setWaveform(std::vector<std::uint8_t> peaks) { waveform_bar.setWaveform(std::move(peaks)); }
            /// The tempo of the loaded track from the analysis; std::nullopt if unknown
            void                                setBpm(std::optional<double> bpm) noexcept { track_bpm = bpm; }
            [[nodiscard]] std::optional<double> bpm() const noexcept { return track_bpm; }
            /// The tempo shown: the track's tempo with the pitch applied, std::nullopt if unknown
            [[nodiscard]] std::optional<double> shownBpm() const noexcept;
            /// A line in place of the track in the display, e.g. why loading failed
            void                             setMessage(std::string message);
            [[nodiscard]] const std::string &artist() const noexcept { return track_artist; }
            [[nodiscard]] const std::string &trackTitle() const noexcept { return track_title; }

            /// The crossfader side the deck is on, as of the last update
            [[nodiscard]] core::api::CrossfaderSide crossfaderSide() const noexcept { return state.crossfader_side; }

            /// The deck was playing at the last update
            [[nodiscard]] bool playing() const noexcept { return state.play_state == core::api::PlayState::Playing; }

            /// The pitch the fader stands for, in percent (+ is faster); the top of the fader is the fastest
            [[nodiscard]] static float pitchOf(float fader_value) noexcept;
            [[nodiscard]] static float faderValueOf(float pitch_percent) noexcept;

            [[nodiscard]] const Fader       &fader() const noexcept { return volume_fader; }
            [[nodiscard]] const Fader       &headphoneFader() const noexcept { return headphone_fader; }
            [[nodiscard]] const Fader       &pitchFader() const noexcept { return pitch_fader; }
            [[nodiscard]] const PlayButton  &playButton() const noexcept { return play_button; }
            [[nodiscard]] const TextButton  &loadButton() const noexcept { return load_button; }
            [[nodiscard]] const TextButton  &cueButton() const noexcept { return cue_button; }
            [[nodiscard]] const TextButton  &keylockButton() const noexcept { return keylock_button; }
            [[nodiscard]] const TextButton  &bendDownButton() const noexcept { return bend_down_button; }
            [[nodiscard]] const TextButton  &bendUpButton() const noexcept { return bend_up_button; }
            [[nodiscard]] const TextButton  &timeButton() const noexcept { return time_button; }
            [[nodiscard]] const TextButton  &headphoneButton() const noexcept { return headphone_button; }
            [[nodiscard]] const WaveformBar &waveform() const noexcept { return waveform_bar; }
            /// How much bigger than the design size the window is (the smaller of the two directions)
            [[nodiscard]] float           scale() const noexcept { return text_scale; }
            [[nodiscard]] const CheckBox &cueLoadBox() const noexcept { return cue_load_box; }
            /// The Cue-Load option is on: a track loaded into this player starts at its cue point
            [[nodiscard]] bool cueLoad() const noexcept { return cue_load_box.checked(); }
            void               setCueLoad(bool on) noexcept { cue_load_box.setChecked(on); }
            /// The state shown next to the buttons, e.g. "playing"; a notice while there is one
            [[nodiscard]] std::string stateText() const;
            /// Shows a message (a catalog key) in place of the state for notice_time, e.g. why nothing was loaded
            void setNotice(std::string_view key);

        private:
            [[nodiscard]] std::string_view stateKey() const;
            void                           applyTranslations();
            void                           handleButtons();
            void                           sendFaders();
            void                           flush();

            core::api::DeckId              deck;
            core::api::CommandQueue       *queue;
            core::api::DeckSnapshotBuffer *snapshot;
            const Translator              *translator;
            TimedNotice                    notice;
            core::api::DeckSnapshot        state;
            Load                           on_load;
            CueSet                         on_cue_set;
            std::uint32_t                  seen_cue_sets = 0;

            TextButton  load_button;
            TextButton  bend_down_button{"-"};
            TextButton  bend_up_button{"+"};
            TextButton  keylock_button;
            TextButton  time_button;
            TextButton  cue_button;
            TextButton  headphone_button;
            PlayButton  play_button;
            WaveformBar waveform_bar;
            Fader       volume_fader;
            Fader       headphone_fader;
            Fader       pitch_fader;
            CheckBox    cue_load_box;

            Rect display_area;
            Rect pitch_text_area;
            Rect time_area;
            Rect bpm_area;
            Rect state_area;
            Rect info_area;
            Rect volume_label_area;
            Rect headphone_label_area;

            std::string           track_artist;
            std::string           track_title;
            std::optional<double> track_bpm;
            float                 text_scale     = 1.0F;
            bool                  show_remaining = false;
            float                 sent_volume;
            float                 sent_headphone_level;
            float                 sent_pitch = 0.0F;
            /// The pitch shown: what the fader says
            float shown_pitch = 0.0F;
            /// Commands that go out in this order
            std::deque<core::api::Command>    pending;
            std::optional<core::api::Command> pending_scrub;
            double                            scrub_seconds = 0.0;
            bool                              scrub_active  = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_DECK_VIEW_HPP
