// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "deck_view.hpp"

#include "look.hpp"
#include "time_format.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin = 8.0F;
        constexpr float gap    = 8.0F;

        constexpr float top_row_height   = 44.0F;
        constexpr float display_height   = 118.0F;
        constexpr float waveform_height  = 60.0F;
        constexpr float transport_height = 70.0F;
        constexpr float column_width     = 56.0F;
        constexpr float small_button     = 26.0F;
        constexpr float label_height     = 20.0F;
        constexpr float info_height      = 62.0F;
        constexpr float bend_width       = 48.0F;

        constexpr Color yellow{255, 225, 0};
        constexpr Color red{225, 40, 40};
        constexpr Color green{40, 215, 70};
        constexpr Color label_color{200, 200, 200};
        constexpr Color cue_background{225, 40, 40};
        constexpr Color cue_text{250, 250, 250};

        /// Pitch range of the fader in percent; the engine clamps to the same
        constexpr float pitch_range = 12.0F;
        /// Within this distance of 0 the fader counts as 0, so the center can be found by touch
        constexpr float pitch_detent = 0.15F;
        /// Position and cue point closer than this (seconds) are the same place
        constexpr double same_place = 0.02;

        /// A pitch as the display shows it: a sign and one decimal, e.g. "+1.2"
        std::string signedTenths(float percent)
        {
            const long        tenths = std::lround(std::abs(percent) * 10.0F);
            const std::string text   = std::to_string(tenths / 10) + '.' + std::to_string(tenths % 10);
            return (percent < 0.0F && tenths != 0 ? "-" : "+") + text;
        }
    } // namespace

    float DeckView::pitchOf(float fader_value) noexcept
    {
        const float percent = (std::clamp(fader_value, 0.0F, 1.0F) - 0.5F) * 2.0F * pitch_range;
        const float rounded = std::round(percent * 10.0F) / 10.0F; // tenths of a percent
        return std::abs(rounded) < pitch_detent ? 0.0F : rounded;
    }

    float DeckView::faderValueOf(float pitch_percent) noexcept
    {
        return 0.5F + (std::clamp(pitch_percent, -pitch_range, pitch_range) / (2.0F * pitch_range));
    }

    DeckView::DeckView(core::api::DeckId deck, core::api::CommandQueue &queue, core::api::DeckSnapshotBuffer &snapshot,
                       const Translator &translator, Clock clock)
        : deck{deck}, queue{&queue}, snapshot{&snapshot}, translator{&translator},
          notice{std::move(clock), notice_time}, state{snapshot.read()}, seen_cue_sets{state.cue_set_count},
          sent_volume{state.channel_volume}, sent_headphone_level{state.headphone_level},
          sent_pitch{state.pitch_percent}, shown_pitch{state.pitch_percent}
    {
        volume_fader.setValue(state.channel_volume);
        headphone_fader.setValue(state.headphone_level);
        pitch_fader.setValue(faderValueOf(state.pitch_percent));
        play_button.setPlaying(state.play_state == core::api::PlayState::Playing);
        cue_button.setColors(cue_background, cue_text);
        load_button.setEnabled(false);
        load_button.setIcon(TextButton::Icon::Folder);
    }

    void DeckView::layout(const Rect &window)
    {
        // Laid out in design units, then every rect is stretched to the window: both directions grow, as does the text
        const float sx     = window.width / design_width;
        const float sy     = window.height / design_height;
        text_scale         = std::clamp(std::min(sx, sy), 0.5F, 6.0F);
        const auto stretch = [&](const Rect &design)
        { return Rect{window.x + (design.x * sx), window.y + (design.y * sy), design.width * sx, design.height * sy}; };

        const Rect  inner   = Rect{0, 0, design_width, design_height}.inset(margin);
        const float column  = column_width;
        const float left_w  = inner.width - column - gap;
        const float right_x = inner.x + left_w + gap;
        const float bottom  = inner.y + inner.height;
        float       y       = inner.y;

        // Load, pitch bend, KEY
        load_button.setBounds(stretch({inner.x, y, left_w * 0.42F, top_row_height}));
        bend_up_button.setBounds(stretch({inner.x + left_w - bend_width, y, bend_width, top_row_height}));
        bend_down_button.setBounds(stretch({inner.x + left_w - (2 * bend_width) - gap, y, bend_width, top_row_height}));
        keylock_button.setBounds(stretch({right_x, y, column, top_row_height}));
        y += top_row_height + gap;

        // The black display: Time button, pitch, the large time, the tempo
        const float display_top = y;
        const Rect  display     = {inner.x, y, left_w, display_height};
        display_area            = stretch(display);
        time_button.setBounds(stretch({display.x + 6.0F, display.y + 6.0F, 88.0F, small_button}));
        pitch_text_area = stretch({display.x + 102.0F, display.y + 6.0F, left_w - 102.0F - 8.0F, small_button});
        time_area       = stretch({display.x, display.y + 32.0F, left_w, display_height - 58.0F});
        bpm_area        = stretch({display.x, display.y + display_height - 26.0F, left_w, 22.0F});
        y += display_height + gap;

        // The waveform
        waveform_bar.setBounds(stretch({inner.x, y, left_w, waveform_height}));
        y += waveform_height + gap;

        // State, CUE, play
        const float state_width = left_w * 0.28F;
        const float rest        = left_w - state_width - (2 * gap);
        state_area              = stretch({inner.x, y, state_width, transport_height});
        cue_button.setBounds(stretch({inner.x + state_width + gap, y, rest / 2, transport_height}));
        play_button.setBounds(stretch({inner.x + state_width + gap + (rest / 2) + gap, y, rest / 2, transport_height}));
        // The pitch fader runs from the display down to the transport row
        pitch_fader.setBounds(stretch({right_x, display_top, column, y + transport_height - display_top}));
        y += transport_height + gap;

        // Below: channel and headphone faders with labels, the track display and the Cue-Load option
        const float faders_height = std::max(0.0F, bottom - y - label_height);
        const float head_x        = inner.x + column_width + gap;
        volume_fader.setBounds(stretch({inner.x, y, column_width, faders_height}));
        const float hp_height = std::min(small_button + 4.0F, faders_height);
        headphone_button.setBounds(stretch({head_x, y, column_width, hp_height}));
        headphone_fader.setBounds(
            stretch({head_x, y + hp_height + 4.0F, column_width, std::max(0.0F, faders_height - hp_height - 4.0F)}));
        volume_label_area    = stretch({inner.x, y + faders_height, column_width, label_height});
        headphone_label_area = stretch({head_x, y + faders_height, column_width, label_height});

        const float info_x = head_x + column_width + gap;
        const float info_w = inner.x + inner.width - info_x;
        info_area          = stretch({info_x, y, info_w, info_height});
        cue_load_box.setBounds(stretch({info_x, y + info_height + gap, info_w, 28.0F}));
    }

    void DeckView::addTo(PointerRouter &router)
    {
        router.add(load_button);
        router.add(bend_down_button);
        router.add(bend_up_button);
        router.add(keylock_button);
        router.add(time_button);
        router.add(waveform_bar);
        router.add(cue_button);
        router.add(play_button);
        router.add(pitch_fader);
        router.add(volume_fader);
        router.add(headphone_button);
        router.add(headphone_fader);
        router.add(cue_load_box);
    }

    void DeckView::setTrack(std::string artist, std::string title)
    {
        track_artist = std::move(artist);
        track_title  = std::move(title);
    }

    void DeckView::setMessage(std::string message)
    {
        track_artist.clear();
        track_title = std::move(message);
    }

    std::string_view DeckView::stateKey() const
    {
        if (!state.track_loaded)
        {
            return {};
        }
        if (playing())
        {
            return "player.state.playing";
        }
        if (std::abs(state.position_seconds - state.cue_seconds) < same_place)
        {
            return state.position_seconds < same_place ? "player.state.loaded" : "player.state.cued";
        }
        return "player.state.paused";
    }

    void DeckView::setNotice(std::string_view key)
    {
        notice.show(key);
    }

    std::string DeckView::stateText() const
    {
        if (const std::string_view key = notice.current(); !key.empty())
        {
            return translator->tr(key);
        }
        const std::string_view key = stateKey();
        return key.empty() ? std::string{} : translator->tr(key);
    }

    void DeckView::applyTranslations()
    {
        load_button.setLabel(translator->tr("player.load"));
        time_button.setLabel(translator->tr("player.time"));
        keylock_button.setLabel(translator->tr("player.keylock"));
        cue_button.setLabel(translator->tr("player.cue"));
        headphone_button.setLabel(translator->tr("player.headphones_button"));
        cue_load_box.setLabel(translator->tr("player.cue_load"));
    }

    void DeckView::handleButtons()
    {
        // The Load button asks the owner, which knows the library's selection; a playing player cannot be loaded
        load_button.setEnabled(!playing() && static_cast<bool>(on_load));
        if (load_button.takePress() && on_load)
        {
            on_load();
        }

        // Decided on the state the user sees, so a press always does what the button shows
        if (play_button.takePress())
        {
            if (playing())
            {
                pending.emplace_back(core::api::DeckPause{deck});
            }
            else
            {
                pending.emplace_back(core::api::DeckPlay{deck});
            }
        }
        // CUE acts on press and release
        if (cue_button.takeDown())
        {
            pending.emplace_back(core::api::DeckCue{deck, true});
        }
        if (cue_button.takeUp())
        {
            pending.emplace_back(core::api::DeckCue{deck, false});
        }
        // Pitch bend while held
        for (const auto &[button, direction] :
             {std::pair{&bend_down_button, std::int8_t{-1}}, std::pair{&bend_up_button, std::int8_t{1}}})
        {
            if (button->takeDown())
            {
                pending.emplace_back(core::api::DeckPitchBend{deck, direction});
            }
            if (button->takeUp())
            {
                pending.emplace_back(core::api::DeckPitchBend{deck, 0});
            }
        }
        if (keylock_button.takePress())
        {
            pending.emplace_back(core::api::DeckSetKeylock{deck, !state.keylock});
        }
        if (headphone_button.takePress())
        {
            pending.emplace_back(core::api::MixerSetHeadphoneCue{deck, !state.headphone_cue});
        }
        if (time_button.takePress())
        {
            show_remaining = !show_remaining;
        }
        // A double tap on the pitch fader sets the pitch to 0
        if (pitch_fader.takeDoubleTap())
        {
            pitch_fader.setValue(0.5F);
        }
        static_cast<void>(cue_load_box.takeToggled());

        // The waveform shows the track's position and cue point unless the finger is on it
        if (!waveform_bar.held())
        {
            waveform_bar.setPosition(state.duration_seconds > 0.0
                                         ? static_cast<float>(state.position_seconds / state.duration_seconds)
                                         : 0.0F);
        }
        waveform_bar.setCue(state.track_loaded && state.duration_seconds > 0.0
                                ? std::optional{static_cast<float>(state.cue_seconds / state.duration_seconds)}
                                : std::nullopt);
        waveform_bar.setLoaded(state.loaded_fraction);
        // Touching the waveform of a playing deck jumps there at once and it plays on; on a deck that stands it
        // scrubs: the deck plays a short loop where the finger is, letting go ends it
        if (const auto chosen = waveform_bar.takeSeek(); chosen && state.duration_seconds > 0.0)
        {
            const double seconds = static_cast<double>(*chosen) * state.duration_seconds;
            if (playing() && !scrub_active)
            {
                pending_position = core::api::DeckSeek{deck, seconds};
            }
            else
            {
                scrub_seconds    = seconds;
                scrub_active     = true;
                pending_position = core::api::DeckScrub{deck, scrub_seconds, true};
            }
        }
        if (scrub_active && !waveform_bar.held())
        {
            // In order with the other commands, and after the last position that went out
            scrub_active = false;
            pending_position.reset();
            pending.emplace_back(core::api::DeckScrub{deck, scrub_seconds, false});
        }

        keylock_button.setHighlighted(state.keylock);
        headphone_button.setHighlighted(state.headphone_cue);
        time_button.setHighlighted(show_remaining);
    }

    void DeckView::flush()
    {
        while (!pending.empty() && queue->tryPush(pending.front()))
        {
            pending.pop_front();
        }
        if (pending.empty() && pending_position && queue->tryPush(*pending_position))
        {
            pending_position.reset();
        }
    }

    void DeckView::sendFaders()
    {
        const float volume = volume_fader.value();
        if (volume != sent_volume && queue->tryPush(core::api::MixerSetChannelVolume{deck, volume}))
        {
            sent_volume = volume;
        }
        const float level = headphone_fader.value();
        if (level != sent_headphone_level && queue->tryPush(core::api::MixerSetHeadphoneLevel{deck, level}))
        {
            sent_headphone_level = level;
        }
        const float pitch = pitchOf(pitch_fader.value());
        shown_pitch       = pitch;
        if (pitch != sent_pitch && queue->tryPush(core::api::DeckSetPitch{deck, pitch}))
        {
            sent_pitch = pitch;
        }
    }

    void DeckView::update()
    {
        state = snapshot->read();
        if (state.cue_set_count != seen_cue_sets)
        {
            seen_cue_sets = state.cue_set_count;
            if (on_cue_set)
            {
                on_cue_set(state.cue_seconds);
            }
        }
        play_button.setPlaying(playing());
        applyTranslations();
        handleButtons();
        flush();
        sendFaders();
    }

    std::optional<double> DeckView::shownBpm() const noexcept
    {
        if (!track_bpm)
        {
            return std::nullopt;
        }
        return *track_bpm * (1.0 + (static_cast<double>(shown_pitch) / 100.0));
    }

    void DeckView::draw(Canvas &canvas) const
    {
        canvas.setTextScale(text_scale);
        // The display: Time button, pitch and the large time, remaining with a minus
        look::sunken(canvas, display_area);
        time_button.draw(canvas);
        const std::string pitch = translator->tr("player.pitch", {{"value", signedTenths(shown_pitch)}});
        canvas.textElided({pitch_text_area.x + std::max(0.0F, pitch_text_area.width - canvas.textWidth(pitch)),
                           pitch_text_area.y + ((pitch_text_area.height - canvas.textHeight()) / 2)},
                          pitch, yellow, pitch_text_area.width);
        const double shown_seconds =
            show_remaining ? std::max(0.0, state.duration_seconds - state.position_seconds) : state.position_seconds;
        const std::string clock = formatClock(shown_seconds, show_remaining);
        canvas.largeText({time_area.x + std::max(0.0F, (time_area.width - canvas.largeTextWidth(clock)) / 2),
                          time_area.y + std::max(0.0F, (time_area.height - canvas.largeTextHeight()) / 2)},
                         clock, yellow);

        if (const auto tempo = shownBpm())
        {
            const std::string text = translator->tr("player.bpm", {{"value", formatBpm(*tempo)}});
            canvas.textElided({bpm_area.x + std::max(0.0F, (bpm_area.width - canvas.textWidth(text)) / 2),
                               bpm_area.y + ((bpm_area.height - canvas.textHeight()) / 2)},
                              text, yellow, bpm_area.width);
        }

        waveform_bar.draw(canvas);

        const std::string status = stateText();
        canvas.textElided({state_area.x, state_area.y + ((state_area.height - canvas.textHeight()) / 2)}, status,
                          playing() ? green : red, state_area.width);
        cue_button.draw(canvas);
        play_button.draw(canvas);

        load_button.draw(canvas);
        bend_down_button.draw(canvas);
        bend_up_button.draw(canvas);
        keylock_button.draw(canvas);
        pitch_fader.draw(canvas);

        volume_fader.draw(canvas);
        headphone_button.draw(canvas);
        headphone_fader.draw(canvas);
        const auto label = [&canvas](const Rect &area, const std::string &text)
        {
            canvas.textElided({area.x + std::max(0.0F, (area.width - canvas.textWidth(text)) / 2),
                               area.y + ((area.height - canvas.textHeight()) / 2)},
                              text, label_color, area.width);
        };
        label(volume_label_area, translator->tr("player.volume"));
        label(headphone_label_area, translator->tr("player.headphones"));

        look::sunken(canvas, info_area);
        const float line = canvas.textHeight();
        canvas.textElided({info_area.x + 6.0F, info_area.y + 4.0F}, track_artist, yellow,
                          std::max(0.0F, info_area.width - 12.0F));
        canvas.textElided({info_area.x + 6.0F, info_area.y + 4.0F + line}, track_title, yellow,
                          std::max(0.0F, info_area.width - 12.0F));
        cue_load_box.draw(canvas);
    }
} // namespace djw::player_ui::sdl
