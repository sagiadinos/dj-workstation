// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_FILE_DIALOGS_HPP
#define DJW_PLAYER_UI_SDL_SDL_FILE_DIALOGS_HPP

#include "file_dialogs.hpp"

#include <SDL3/SDL_video.h>

#include <memory>
#include <mutex>
#include <optional>

namespace djw::player_ui::sdl
{
    /**
     * FileDialogs through SDL3 (portal or zenity on Linux, the native dialogs elsewhere).
     *
     * SDL may answer on another thread, so the chosen file is only stored there; deliver(), called once per frame on
     * the UI thread, hands it on. One dialog at a time: a new request replaces a pending one. A dialog still open when
     * this object goes away finds its answer dropped.
     */
    class SdlFileDialogs final : public FileDialogs
    {
        public:
            /// The window the dialogs belong to; may be nullptr
            explicit SdlFileDialogs(SDL_Window *parent = nullptr);

            void setParent(SDL_Window *window) noexcept { parent = window; }

            void openPlaylist(Chosen chosen) override;
            void savePlaylist(const std::string &suggested_name, Chosen chosen) override;
            void openFolder(Chosen chosen) override;

            /// UI thread: calls the waiting continuation if its dialog has answered
            void deliver();

        private:
            /// Shared with the SDL callback, which may outlive this object
            struct Answer
            {
                    std::mutex                           mutex;
                    std::optional<std::filesystem::path> file;
                    bool                                 answered = false;
            };

            std::shared_ptr<Answer> waitFor(Chosen chosen);
            /// SDL's callback; userdata is a heap-allocated std::shared_ptr<Answer>, freed here
            static void SDLCALL onAnswer(void *userdata, const char *const *files, int filter);

            SDL_Window             *parent;
            Chosen                  pending;
            std::shared_ptr<Answer> answer;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_FILE_DIALOGS_HPP
