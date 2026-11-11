// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_FILE_DIALOGS_HPP
#define DJW_PLAYER_UI_SDL_FILE_DIALOGS_HPP

#include <filesystem>
#include <functional>
#include <string>

namespace djw::player_ui::sdl
{
    /**
     * The system's dialogs for playlist files and music folders. Behind this interface so views can be tested without
     * dialogs.
     *
     * chosen is called on the UI thread with the file or folder the user picked, and not at all when the dialog is
     * cancelled.
     */
    class FileDialogs
    {
        public:
            using Chosen = std::function<void(const std::filesystem::path &file)>;

            FileDialogs()                               = default;
            FileDialogs(const FileDialogs &)            = delete;
            FileDialogs &operator=(const FileDialogs &) = delete;
            FileDialogs(FileDialogs &&)                 = delete;
            FileDialogs &operator=(FileDialogs &&)      = delete;
            virtual ~FileDialogs()                      = default;

            /// Lets the user pick an existing playlist file
            virtual void openPlaylist(Chosen chosen) = 0;
            /// Lets the user choose where to save a playlist file, suggesting suggested_name
            virtual void savePlaylist(const std::string &suggested_name, Chosen chosen) = 0;
            /// Lets the user pick an existing folder, e.g. a music folder for the library
            virtual void openFolder(Chosen chosen) = 0;
            /// Lets the user pick a Mixxx controller mapping (.midi.xml) to import
            virtual void openMixxxMapping(Chosen chosen) = 0;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_FILE_DIALOGS_HPP
