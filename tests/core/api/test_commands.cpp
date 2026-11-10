// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/commands.hpp"

#include <doctest/doctest.h>

#include <variant>

using namespace djw::core::api;

TEST_CASE("Commands pass through the command queue unchanged")
{
    CommandQueue queue;
    REQUIRE(queue.tryPush(DeckPlay{DeckId{1}}));
    REQUIRE(queue.tryPush(MixerSetChannelVolume{DeckId{2}, 0.5F}));

    const auto first = queue.tryPop();
    REQUIRE(first.has_value());
    REQUIRE(std::holds_alternative<DeckPlay>(*first));
    CHECK(std::get<DeckPlay>(*first).deck == DeckId{1});

    const auto second = queue.tryPop();
    REQUIRE(second.has_value());
    REQUIRE(std::holds_alternative<MixerSetChannelVolume>(*second));
    CHECK(std::get<MixerSetChannelVolume>(*second).deck == DeckId{2});
    CHECK(std::get<MixerSetChannelVolume>(*second).value == doctest::Approx(0.5F));
}

TEST_CASE("Mixer commands pass through the command queue unchanged")
{
    CommandQueue queue;
    REQUIRE(queue.tryPush(MixerAssignCrossfader{DeckId{3}, CrossfaderSide::B}));
    REQUIRE(queue.tryPush(MixerCrossfaderMomentary{CrossfaderSide::A}));

    const auto assign = queue.tryPop();
    REQUIRE(assign.has_value());
    REQUIRE(std::holds_alternative<MixerAssignCrossfader>(*assign));
    CHECK(std::get<MixerAssignCrossfader>(*assign).deck == DeckId{3});
    CHECK(std::get<MixerAssignCrossfader>(*assign).side == CrossfaderSide::B);

    const auto momentary = queue.tryPop();
    REQUIRE(momentary.has_value());
    REQUIRE(std::holds_alternative<MixerCrossfaderMomentary>(*momentary));
    CHECK(std::get<MixerCrossfaderMomentary>(*momentary).side == CrossfaderSide::A);
}
