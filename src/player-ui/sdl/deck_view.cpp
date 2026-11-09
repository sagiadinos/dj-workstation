// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "deck_view.hpp"

#include "time_format.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin = 16.0F;
        constexpr float gap    = 12.0F;

        constexpr Color text_color{230, 230, 230};
    } // namespace

    DeckView::DeckView(core::api::DeckId deck, core::api::CommandQueue &queue, core::api::DeckSnapshotBuffer &snapshot)
        : deck{deck}, queue{&queue}, snapshot{&snapshot}, state{snapshot.read()}, sent_volume{state.channel_volume}
    {
        volume_fader.setValue(state.channel_volume);
        play_button.setPlaying(state.play_state == core::api::PlayState::Playing);
    }

    void DeckView::layout(const Rect &area)
    {
        const Rect  inner         = area.inset(margin);
        const float button_height = std::min(inner.width, inner.height * 0.3F);
        const float text_height   = 24.0F;

        play_button.setBounds({inner.x, inner.y, inner.width, button_height});
        position_area = {inner.x, inner.y + button_height + gap, inner.width, text_height};

        const float fader_top = position_area.y + text_height + gap;
        volume_fader.setBounds({inner.x, fader_top, inner.width, std::max(0.0F, inner.y + inner.height - fader_top)});
    }

    void DeckView::addTo(PointerRouter &router)
    {
        router.add(play_button);
        router.add(volume_fader);
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
    }

    void DeckView::draw(Canvas &canvas) const
    {
        play_button.draw(canvas);

        const std::string position = formatPosition(state.position_seconds);
        canvas.text({position_area.x + (position_area.width - canvas.textWidth(position)) / 2,
                     position_area.y + (position_area.height - canvas.textHeight()) / 2},
                    position, text_color);

        volume_fader.draw(canvas);
    }
} // namespace djw::player_ui::sdl
