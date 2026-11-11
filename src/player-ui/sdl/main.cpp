// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/core.hpp"
#include "api/library.hpp"
#include "command_line.hpp"
#include "sdl_app.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_locale.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_stdinc.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <span>
#include <thread>

namespace api = djw::core::api;
namespace sdl = djw::player_ui::sdl;

namespace
{
    /// The library database in the user's data folder, e.g. ~/.local/share/djw/library.db on Linux
    std::optional<std::filesystem::path> libraryFile()
    {
        char *folder = SDL_GetPrefPath("", "djw");
        if (folder == nullptr)
        {
            return std::nullopt;
        }
        std::filesystem::path file = std::filesystem::path{folder} / "library.db";
        SDL_free(folder);
        return file;
    }

    /// The file with the language the user chose, e.g. ~/.local/share/djw/language on Linux
    std::optional<std::filesystem::path> languageFile()
    {
        char *folder = SDL_GetPrefPath("", "djw");
        if (folder == nullptr)
        {
            return std::nullopt;
        }
        std::filesystem::path file = std::filesystem::path{folder} / "language";
        SDL_free(folder);
        return file;
    }

    /// The file with the window layout, e.g. ~/.local/share/djw/layout on Linux
    std::filesystem::path layoutFile()
    {
        char *folder = SDL_GetPrefPath("", "djw");
        if (folder == nullptr)
        {
            return {};
        }
        std::filesystem::path file = std::filesystem::path{folder} / "layout";
        SDL_free(folder);
        return file;
    }

    /// The language chosen before, otherwise the first language of the system's locales that we have, otherwise English
    sdl::Language startLanguage()
    {
        if (const auto file = languageFile())
        {
            std::string code;
            if (std::ifstream{*file} >> code)
            {
                if (const auto saved = sdl::languageFromCode(code))
                {
                    return *saved;
                }
            }
        }
        int         count   = 0;
        SDL_Locale **locales = SDL_GetPreferredLocales(&count);
        sdl::Language found  = sdl::Language::English;
        for (int index = 0; locales != nullptr && index < count; ++index)
        {
            if (locales[index]->language != nullptr)
            {
                if (const auto language = sdl::languageFromCode(locales[index]->language))
                {
                    found = *language;
                    break;
                }
            }
        }
        SDL_free(locales);
        return found;
    }

    void saveLanguage(sdl::Language language)
    {
        if (const auto file = languageFile())
        {
            std::ofstream{*file} << sdl::languageCode(language) << '\n';
        }
    }

    /// The library, or nullptr (with a message) if it cannot be opened: the player then works without it
    std::unique_ptr<api::Library> openLibrary(const sdl::CommandLine &command_line)
    {
        const auto file = libraryFile();
        if (!file)
        {
            std::cerr << "djw: no data folder for the library: " << SDL_GetError() << '\n';
            return nullptr;
        }
        auto library = api::openLibrary({.database_file = *file});
        if (!library)
        {
            std::cerr << "djw: " << library.error() << '\n';
            return nullptr;
        }
        for (const auto &folder : command_line.add_folders)
        {
            if (auto added = (*library)->addFolder(folder); !added)
            {
                std::cerr << "djw: " << added.error() << '\n';
            }
        }
        (*library)->rescan(); // picks up what changed since the last start
        return std::move(*library);
    }
} // namespace

int main(int argc, char *argv[])
{
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    const auto              command_line = sdl::parseCommandLine(args.subspan(std::min<std::size_t>(1, args.size())));
    if (!command_line)
    {
        std::cerr << "djw: " << command_line.error() << '\n' << sdl::usage;
        return 2;
    }

    auto core = api::createCore({.headphone_output = command_line->headphones});
    if (!core)
    {
        std::cerr << "djw: " << core.error() << '\n';
        return 1;
    }

    const api::DeckId deck = (*core)->addDeck();
    if (command_line->file)
    {
        if (auto loaded = (*core)->loadTrack(deck, *command_line->file); !loaded)
        {
            std::cerr << "djw: " << loaded.error() << '\n';
            return 1;
        }
    }

    if (command_line->headphones && !(*core)->mixerSnapshot().read().headphones_available)
    {
        std::cerr << "djw: the audio device has fewer than 4 channels, playing the master only\n";
    }

    if (auto started = (*core)->start(); !started)
    {
        std::cerr << "djw: " << started.error() << '\n';
        return 1;
    }

    const std::unique_ptr<api::Library> library = openLibrary(*command_line);

    auto app = sdl::SdlApp::create(**core, deck, library.get(), startLanguage(), saveLanguage, layoutFile());
    if (!app)
    {
        std::cerr << "djw: " << app.error() << '\n';
        return 1;
    }
    if (command_line->file)
    {
        (*app)->setDeckTrack(std::filesystem::absolute(*command_line->file), command_line->file->filename().string());
    }
    (*app)->run();
    app->reset(); // the windows go before the library they show

    // Let the pause fade out instead of cutting the sound; with a full queue the sound is cut, fine on exit
    if ((*core)->commandQueue().tryPush(api::DeckPause{deck}))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    return 0;
}
