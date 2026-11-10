// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "deck_view.hpp"

#include "time_format.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin = 16.0F;
        constexpr float gap    = 12.0F;

        constexpr Color text_color{230, 230, 230};

        constexpr float small_button_height = 44.0F;
        /// Pitch range of the fader in percent; the engine clamps to the same
        constexpr float pitch_range = 12.0F;
        /// Within this distance of 0 the fader counts as 0, so the center can be found by touch
        constexpr float pitch_detent = 0.15F;
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

    DeckView::DeckView(core::api::DeckId deck, core::api::CommandQueue &queue, core::api::DeckSnapshotBuffer &snapshot)
        : deck{deck}, queue{&queue}, snapshot{&snapshot}, state{snapshot.read()}, sent_volume{state.channel_volume}
    {
        volume_fader.setValue(state.channel_volume);
        pitch_fader.setValue(faderValueOf(state.pitch_percent));
        sent_pitch = state.pitch_percent;
        play_button.setPlaying(state.play_state == core::api::PlayState::Playing);
    }

    void DeckView::layout(const Rect &area)
    {
        const Rect  inner         = area.inset(margin);
        const float text_height   = 24.0F;
        const float button_height = std::min(inner.width, inner.height * 0.3F);

        title_area = {inner.x, inner.y, inner.width, text_height};
        play_button.setBounds({inner.x, inner.y + text_height + gap, inner.width, button_height});
        position_area = {inner.x, play_button.bounds().y + button_height + gap, inner.width, text_height};

        // Below: the channel fader on the left, on the right the pitch fader under its buttons and its value
        const float fader_top    = position_area.y + text_height + gap;
        const float fader_height = std::max(0.0F, inner.y + inner.height - fader_top);
        const float half         = (inner.width - gap) / 2;
        volume_fader.setBounds({inner.x, fader_top, half, fader_height});

        const float right = inner.x + half + gap;
        keylock_button.setBounds({right, fader_top, half * 0.6F - (gap / 2), std::min(small_button_height, fader_height)});
        zero_button.setBounds({right + (half * 0.6F) + (gap / 2), fader_top, half * 0.4F - (gap / 2),
                               std::min(small_button_height, fader_height)});
        pitch_text_area  = {right, fader_top + small_button_height + (gap / 2), half, text_height};
        const float pitch_top = pitch_text_area.y + text_height + (gap / 2);
        pitch_fader.setBounds({right, pitch_top, half, std::max(0.0F, fader_top + fader_height - pitch_top)});
    }

    void DeckView::addTo(PointerRouter &router)
    {
        router.add(play_button);
        router.add(volume_fader);
        router.add(pitch_fader);
        router.add(keylock_button);
        router.add(zero_button);
    }

    void DeckView::update()
    {
        state = snapshot->read();
        play_button.setPlaying(state.play_state == core::api::PlayState::Playing);

        if (play_button.takePress())
        {
            // Decided on the state the user sees, so a press always does what the button shows
            if (state.play_state == core::api::PlayState::Playing)
            {
                pending_transport = core::api::DeckPause{deck};
            }
            else
            {
                pending_transport = core::api::DeckPlay{deck};
            }
        }
        if (pending_transport && queue->tryPush(*pending_transport))
        {
            pending_transport.reset();
        }

        const float volume = volume_fader.value();
        if (volume != sent_volume && queue->tryPush(core::api::MixerSetChannelVolume{deck, volume}))
        {
            sent_volume = volume;
        }

        // Keylock: decided on the state the user sees, like play
        if (keylock_button.takePress())
        {
            pending_keylock = core::api::DeckSetKeylock{deck, !state.keylock};
        }
        if (pending_keylock && queue->tryPush(*pending_keylock))
        {
            pending_keylock.reset();
        }
        keylock_button.setHighlighted(state.keylock);

        if (zero_button.takePress())
        {
            pitch_fader.setValue(0.5F);
        }
        const float pitch = pitchOf(pitch_fader.value());
        if (pitch != sent_pitch && queue->tryPush(core::api::DeckSetPitch{deck, pitch}))
        {
            sent_pitch = pitch;
        }
    }

    void DeckView::draw(Canvas &canvas) const
    {
        canvas.textElided({title_area.x, title_area.y + (title_area.height - canvas.textHeight()) / 2}, track_title,
                          text_color, title_area.width);
        play_button.draw(canvas);

        const std::string position = formatPosition(state.position_seconds);
        canvas.text({position_area.x + (position_area.width - canvas.textWidth(position)) / 2,
                     position_area.y + (position_area.height - canvas.textHeight()) / 2},
                    position, text_color);

        volume_fader.draw(canvas);
        keylock_button.draw(canvas);
        zero_button.draw(canvas);
        pitch_fader.draw(canvas);

        // The pitch in percent as the deck plays it, with a sign
        std::string pitch = std::to_string(std::lround(std::abs(sent_pitch) * 10.0F) / 10.0F);
        pitch.resize(pitch.find('.') + 2); // one decimal
        pitch = (sent_pitch < 0.0F ? "-" : "+") + pitch + " %";
        canvas.text({pitch_text_area.x + (pitch_text_area.width - canvas.textWidth(pitch)) / 2,
                     pitch_text_area.y + (pitch_text_area.height - canvas.textHeight()) / 2},
                    pitch, text_color);
    }
} // namespace djw::player_ui::sdl
