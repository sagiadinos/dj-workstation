// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_DECK_VIEW_HPP
#define DJW_PLAYER_UI_SDL_DECK_VIEW_HPP

#include "api/commands.hpp"
#include "api/deck_id.hpp"
#include "api/snapshot.hpp"
#include "canvas.hpp"
#include "fader.hpp"
#include "play_button.hpp"
#include "pointer_router.hpp"
#include "view.hpp"

#include <optional>
#include <string>

namespace djw::player_ui::sdl
{
    /**
     * One deck on screen: track title, play button, position and channel fader.
     *
     * Turns input into commands for the core and shows the deck's snapshot. A command that does not fit into the
     * queue is kept and sent again on the next update(), so a fader position is never lost. For now the UI is the
     * only one changing the volume, so the fader is the source of truth for it.
     */
    class DeckView final : public View
    {
        public:
            DeckView(core::api::DeckId deck, core::api::CommandQueue &queue, core::api::DeckSnapshotBuffer &snapshot);

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;

            /// Once per frame: reads the snapshot and sends what the input asked for.
            void update() override;

            void draw(Canvas &canvas) const override;

            /// What is shown as the loaded track, e.g. "Artist - Title"
            void                             setTrackTitle(std::string title) { track_title = std::move(title); }
            [[nodiscard]] const std::string &trackTitle() const noexcept { return track_title; }

            [[nodiscard]] const Fader      &fader() const noexcept { return volume_fader; }
            [[nodiscard]] const PlayButton &playButton() const noexcept { return play_button; }

        private:
            core::api::DeckId                 deck;
            core::api::CommandQueue          *queue;
            core::api::DeckSnapshotBuffer    *snapshot;
            core::api::DeckSnapshot           state;
            PlayButton                        play_button;
            Fader                             volume_fader;
            Rect                              title_area;
            Rect                              position_area;
            std::string                       track_title;
            float                             sent_volume;
            std::optional<core::api::Command> pending_transport;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_DECK_VIEW_HPP
