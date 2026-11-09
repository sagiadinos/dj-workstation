// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/core.hpp"
#include "api/version.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace api = djw::core::api;

// Console player until the SDL3 window comes (M0, step 6)
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

    const auto deck = (*core)->addDeck(argv[1]);
    if (!deck)
    {
        std::cerr << "djw: " << deck.error() << '\n';
        return 1;
    }

    (*core)->commandQueue().tryPush(api::DeckPlay{*deck});
    if (auto started = (*core)->start(); !started)
    {
        std::cerr << "djw: " << started.error() << '\n';
        return 1;
    }

    std::cout << "djw " << api::version() << ": playing " << argv[1] << ", press Enter to quit\n";
    std::string line;
    std::getline(std::cin, line);

    // Let the pause fade out instead of cutting the sound
    (*core)->commandQueue().tryPush(api::DeckPause{*deck});
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    return 0;
}
