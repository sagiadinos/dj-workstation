// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "waveform_bar.hpp"

#include "pointer_router.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;

TEST_CASE("WaveformBar: the waveform is scaled so that its loudest point reaches full scale")
{
    WaveformBar bar;
    CHECK(bar.gain() == 1.0F);

    bar.setWaveform({10, 40, 20});
    CHECK(bar.gain() == doctest::Approx(255.0F / 40.0F));

    bar.setWaveform({255, 3});
    CHECK(bar.gain() == doctest::Approx(1.0F));

    bar.setWaveform({0, 0}); // silence: nothing to scale
    CHECK(bar.gain() == 1.0F);
    bar.setWaveform({});
    CHECK_FALSE(bar.hasWaveform());
}

namespace
{
    /// Widths and heights of the bars drawn, in the order they were drawn, for a 300 x 60 bar
    struct Recorder final : Canvas
    {
            void fillRect(const Rect &rect, Color color) override { rects.push_back({rect, color}); }
            void fillTriangle(Point, Point, Point, Color) override {}
            void text(Point, std::string_view, Color) override {}
            void textElided(Point, std::string_view, Color, float) override {}
            float textHeight() const noexcept override { return 10; }
            float textWidth(std::string_view) const noexcept override { return 10; }
            void  largeText(Point, std::string_view, Color) override {}
            float largeTextHeight() const noexcept override { return 10; }
            float largeTextWidth(std::string_view) const noexcept override { return 10; }
            void  setClip(std::optional<Rect>) override {}

            std::vector<std::pair<Rect, Color>> rects;
    };

    bool sameColor(Color a, Color b)
    {
        return a.red == b.red && a.green == b.green && a.blue == b.blue;
    }
} // namespace

TEST_CASE("WaveformBar: the loudest bar fills the bar, whatever the loudness of the track")
{
    WaveformBar bar;
    bar.setBounds({0, 0, 300, 60});
    bar.setWaveform(std::vector<std::uint8_t>(100, 20)); // quiet track
    Recorder canvas;
    bar.draw(canvas);

    float tallest = 0.0F;
    for (const auto &[rect, color] : canvas.rects)
    {
        tallest = std::max(tallest, rect.height);
    }
    CHECK(tallest > 0.8F * (60.0F - 6.0F)); // nearly the full inner height
}

TEST_CASE("WaveformBar: bars after the loaded part are gray, the loaded part is blue, the played part yellow")
{
    WaveformBar bar;
    bar.setBounds({0, 0, 300, 60});
    bar.setWaveform(std::vector<std::uint8_t>(200, 200));
    bar.setLoaded(0.5F);
    bar.setPosition(0.25F);
    Recorder canvas;
    bar.draw(canvas);

    const Color yellow{255, 205, 0};
    const Color blue{40, 140, 245};
    const Color gray{105, 108, 115};
    bool        seen_yellow = false;
    bool        seen_blue   = false;
    bool        seen_gray   = false;
    for (const auto &[rect, color] : canvas.rects)
    {
        if (rect.height < 5.0F || rect.width > 3.0F)
        {
            continue; // the playhead, the frame and the panel are no bars
        }
        const float x = rect.x;
        if (sameColor(color, yellow))
        {
            seen_yellow = true;
            CHECK(x < 3.0F + (0.25F * 294.0F) + 1.0F); // only before the position
        }
        else if (sameColor(color, blue))
        {
            seen_blue = true;
            CHECK(x > 3.0F + (0.25F * 294.0F) - 3.0F);
            CHECK(x < 3.0F + (0.5F * 294.0F) + 1.0F);
        }
        else if (sameColor(color, gray))
        {
            seen_gray = true;
            CHECK(x > 3.0F + (0.5F * 294.0F) - 3.0F);
        }
    }
    CHECK(seen_yellow);
    CHECK(seen_blue);
    CHECK(seen_gray);
}

TEST_CASE("WaveformBar: without a waveform a loading track shows as a line that fills with blue")
{
    WaveformBar bar;
    bar.setBounds({0, 0, 300, 60});
    bar.setLoaded(0.3F);
    Recorder canvas;
    bar.draw(canvas);

    bool found = false;
    for (const auto &[rect, color] : canvas.rects)
    {
        if (sameColor(color, Color{40, 140, 245}) && rect.height <= 4.0F)
        {
            found = true;
            CHECK(rect.width == doctest::Approx(0.3F * 294.0F));
        }
    }
    CHECK(found);
}

TEST_CASE("WaveformBar: touching chooses a position, dragging follows, the last one is taken once")
{
    WaveformBar   bar;
    PointerRouter router;
    bar.setBounds({0, 0, 306, 60});
    router.add(bar);

    router.handle({{1, 1}, PointerPhase::Down, {3.0F + 150.0F, 30}});
    CHECK(bar.held());
    CHECK(bar.takeSeek().value() == doctest::Approx(0.5F));
    CHECK_FALSE(bar.takeSeek().has_value());

    router.handle({{1, 1}, PointerPhase::Move, {3.0F + 450.0F, 30}}); // past the end
    CHECK(bar.takeSeek().value() == doctest::Approx(1.0F));
    router.handle({{1, 1}, PointerPhase::Up, {10, 30}});
    CHECK_FALSE(bar.held());
}
