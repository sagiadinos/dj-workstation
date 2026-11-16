// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_file_dialogs.hpp"

#include <SDL3/SDL_dialog.h>

#include <array>
#include <string>
#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        const std::array<SDL_DialogFileFilter, 1> playlist_filters{{{"Playlists (M3U, M3U8)", "m3u;m3u8"}}};
        const std::array<SDL_DialogFileFilter, 1> mixxx_filters{{{"Mixxx controller mappings (.midi.xml)", "xml"}}};
        const std::array<SDL_DialogFileFilter, 1> layout_filters{{{"Layouts (.djwlayout)", "djwlayout"}}};

        /// The folder as SDL wants it, UTF-8 and ending in a separator so that it is taken as a folder
        std::string dialogFolder(const std::filesystem::path &folder)
        {
            if (folder.empty())
            {
                return {};
            }
            const std::u8string utf8 = (folder / "").u8string();
            return {utf8.begin(), utf8.end()};
        }
    } // namespace

    SdlFileDialogs::SdlFileDialogs(SDL_Window *parent) : parent{parent} {}

    void SDLCALL SdlFileDialogs::onAnswer(void *userdata, const char *const *files, int /*filter*/)
    {
        const std::unique_ptr<std::shared_ptr<Answer>> owner{static_cast<std::shared_ptr<Answer> *>(userdata)};
        Answer                                        &answer = **owner;
        const std::scoped_lock                         lock{answer.mutex};
        answer.answered = true;
        // files is nullptr on an error and empty when the user cancelled
        if (files != nullptr && files[0] != nullptr)
        {
            const std::string utf8{files[0]}; // SDL gives UTF-8 on every platform
            answer.file = std::filesystem::path{std::u8string{utf8.begin(), utf8.end()}};
        }
    }

    std::shared_ptr<SdlFileDialogs::Answer> SdlFileDialogs::waitFor(Chosen chosen)
    {
        pending = std::move(chosen);
        answer  = std::make_shared<Answer>();
        return answer;
    }

    void SdlFileDialogs::openPlaylist(Chosen chosen)
    {
        auto *userdata = new std::shared_ptr<Answer>{waitFor(std::move(chosen))};
        SDL_ShowOpenFileDialog(&onAnswer, userdata, parent, playlist_filters.data(),
                               static_cast<int>(playlist_filters.size()), nullptr, false);
    }

    void SdlFileDialogs::savePlaylist(const std::string &suggested_name, Chosen chosen)
    {
        auto *userdata = new std::shared_ptr<Answer>{waitFor(std::move(chosen))};
        SDL_ShowSaveFileDialog(&onAnswer, userdata, parent, playlist_filters.data(),
                               static_cast<int>(playlist_filters.size()), suggested_name.c_str());
    }

    void SdlFileDialogs::openFolder(Chosen chosen)
    {
        auto *userdata = new std::shared_ptr<Answer>{waitFor(std::move(chosen))};
        SDL_ShowOpenFolderDialog(&onAnswer, userdata, parent, nullptr, false);
    }

    void SdlFileDialogs::openMixxxMapping(Chosen chosen)
    {
        auto *userdata = new std::shared_ptr<Answer>{waitFor(std::move(chosen))};
        SDL_ShowOpenFileDialog(&onAnswer, userdata, parent, mixxx_filters.data(),
                               static_cast<int>(mixxx_filters.size()), nullptr, false);
    }

    void SdlFileDialogs::openLayout(const std::filesystem::path &folder, Chosen chosen)
    {
        const std::string start    = dialogFolder(folder);
        auto             *userdata = new std::shared_ptr<Answer>{waitFor(std::move(chosen))};
        SDL_ShowOpenFileDialog(&onAnswer, userdata, parent, layout_filters.data(),
                               static_cast<int>(layout_filters.size()), start.empty() ? nullptr : start.c_str(),
                               false);
    }

    void SdlFileDialogs::saveLayout(const std::filesystem::path &folder, Chosen chosen)
    {
        const std::string start    = dialogFolder(folder);
        auto             *userdata = new std::shared_ptr<Answer>{waitFor(std::move(chosen))};
        SDL_ShowSaveFileDialog(&onAnswer, userdata, parent, layout_filters.data(),
                               static_cast<int>(layout_filters.size()), start.empty() ? nullptr : start.c_str());
    }

    void SdlFileDialogs::deliver()
    {
        if (!answer)
        {
            return;
        }
        std::optional<std::filesystem::path> file;
        {
            const std::scoped_lock lock{answer->mutex};
            if (!answer->answered)
            {
                return;
            }
            file = std::move(answer->file);
        }
        answer.reset();
        const Chosen chosen = std::exchange(pending, nullptr);
        if (file && chosen)
        {
            chosen(*file);
        }
    }
} // namespace djw::player_ui::sdl
