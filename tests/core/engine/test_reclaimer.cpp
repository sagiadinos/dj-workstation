// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/reclaimer.hpp"

#include <doctest/doctest.h>

#include <memory>

using djw::core::engine::Reclaimer;

namespace
{
    /// Sets a flag when destroyed
    struct Watched
    {
            explicit Watched(bool &destroyed) : destroyed{&destroyed} {}
            Watched(const Watched &)            = delete;
            Watched &operator=(const Watched &) = delete;
            Watched(Watched &&)                 = delete;
            Watched &operator=(Watched &&)      = delete;
            ~Watched() { *destroyed = true; }

            bool *destroyed;
    };
} // namespace

TEST_CASE("Reclaimer: keeps a retired object until the next render has started")
{
    Reclaimer reclaimer;
    bool      destroyed = false;
    reclaimer.enterRender(); // a render is running and may use the object

    reclaimer.retire(std::make_shared<Watched>(destroyed));
    reclaimer.collect();
    CHECK_FALSE(destroyed);
    CHECK(reclaimer.pending() == 1);

    reclaimer.enterRender();
    reclaimer.collect();
    CHECK(destroyed);
    CHECK(reclaimer.pending() == 0);
}

TEST_CASE("Reclaimer: frees only what was retired before the latest render started")
{
    Reclaimer reclaimer;
    bool      older = false;
    bool      newer = false;

    reclaimer.retire(std::make_shared<Watched>(older));
    reclaimer.enterRender();
    reclaimer.retire(std::make_shared<Watched>(newer));
    reclaimer.collect();

    CHECK(older);
    CHECK_FALSE(newer);
}

TEST_CASE("Reclaimer: everything still pending is freed with the reclaimer")
{
    bool destroyed = false;
    {
        Reclaimer reclaimer;
        reclaimer.retire(std::make_shared<Watched>(destroyed));
    }
    CHECK(destroyed);
}
