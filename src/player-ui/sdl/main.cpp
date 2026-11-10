// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/core.hpp"
#include "sdl_app.hpp"

#include <SDL3/SDL_main.h>

#include <chrono>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>

namespace api = djw::core::api;

int main(int argc, char *argv[])
{
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    const bool              headphones = args.size() == 3 && std::string_view{args[1]} == "--headphones";
    if (args.size() != 2 && !headphones)
    {
        std::cerr << "usage: djw [--headphones] <audio file>\n"
                     "  --headphones  master on output channels 1/2, headphones on 3/4 (needs a 4-channel device)\n";
        return 2;
    }
    const char *file = args.back();

    auto core = api::createCore({.headphone_output = headphones});
    if (!core)
    {
        std::cerr << "djw: " << core.error() << '\n';
        return 1;
    }

    const api::DeckId deck = (*core)->addDeck();
    if (auto loaded = (*core)->loadTrack(deck, file); !loaded)
    {
        std::cerr << "djw: " << loaded.error() << '\n';
        return 1;
    }

    if (headphones && !(*core)->mixerSnapshot().read().headphones_available)
    {
        std::cerr << "djw: the audio device has fewer than 4 channels, playing the master only\n";
    }

    if (auto started = (*core)->start(); !started)
    {
        std::cerr << "djw: " << started.error() << '\n';
        return 1;
    }

    auto app = djw::player_ui::sdl::SdlApp::create(**core, deck);
    if (!app)
    {
        std::cerr << "djw: " << app.error() << '\n';
        return 1;
    }
    (*app)->run();

    // Let the pause fade out instead of cutting the sound; with a full queue the sound is cut, fine on exit
    if ((*core)->commandQueue().tryPush(api::DeckPause{deck}))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    return 0;
}
