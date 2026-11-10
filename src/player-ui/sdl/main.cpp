// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/core.hpp"
#include "sdl_app.hpp"

#include <SDL3/SDL_main.h>

#include <chrono>
#include <iostream>
#include <thread>

namespace api = djw::core::api;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage: djw <audio file>\n";
        return 2;
    }

    auto core = api::createCore({});
    if (!core)
    {
        std::cerr << "djw: " << core.error() << '\n';
        return 1;
    }

    const api::DeckId deck = (*core)->addDeck();
    if (auto loaded = (*core)->loadTrack(deck, argv[1]); !loaded)
    {
        std::cerr << "djw: " << loaded.error() << '\n';
        return 1;
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
