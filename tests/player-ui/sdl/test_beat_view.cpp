// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "beat_view.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace djw::player_ui::sdl;
using djw::core::api::TrackPeaks;
using djw::tests::RecordingCanvas;

namespace
{
    /// Values per second the tests give the view: a value is 10 ms
    constexpr double per_second = 100.0;

    // A lane shows 6 s over 600 points: 10 ms per point, 2 values per bar of 2 points. Horizontal, area gives the
    // lanes {6, 6, 606, 100} and {6, 112, 606, 100}; inside their frame {9, 9, 600, 94} and {9, 115, 600, 94}.
    constexpr Rect wide{0, 0, 618, 218};
    constexpr Rect tall{0, 0, 218, 618};

    bool sameRect(const Rect &first, const Rect &second)
    {
        return first.x == doctest::Approx(second.x) && first.y == doctest::Approx(second.y) &&
               first.width == doctest::Approx(second.width) && first.height == doctest::Approx(second.height);
    }

    bool sameColor(Color first, Color second)
    {
        return first.red == second.red && first.green == second.green && first.blue == second.blue &&
               first.alpha == second.alpha;
    }

    /// A track of 20 s with the given values (index, 0 to 255) loud, all others silent
    std::shared_ptr<TrackPeaks> peaksWith(std::initializer_list<std::pair<std::size_t, std::uint8_t>> loud)
    {
        auto peaks = std::make_shared<TrackPeaks>(2'000 * TrackPeaks::frames_per_peak);
        for (const auto &[index, value] : loud)
        {
            const std::vector<float> frames(TrackPeaks::frames_per_peak * 2, static_cast<float>(value) / 255.0F);
            peaks->add(index * TrackPeaks::frames_per_peak, frames);
        }
        return peaks;
    }

    BeatDeck deckAt(int number, double position, std::shared_ptr<TrackPeaks> peaks, float pitch = 0.0F,
                    double cue = 0.0)
    {
        BeatDeck deck{.number = number, .state = {}, .peaks = std::move(peaks)};
        deck.state.track_loaded     = true;
        deck.state.position_seconds = position;
        deck.state.pitch_percent    = pitch;
        deck.state.cue_seconds      = cue;
        return deck;
    }

    struct Fixture
    {
            explicit Fixture(const Rect &area = wide, bool vertical = false)
            {
                view.setVertical(vertical);
                view.layout(area);
                view.update();
            }

            /// Draws and returns the bars of the waveforms: the rects bar_width (1.5) long in time
            std::vector<std::pair<Rect, Color>> bars()
            {
                canvas = std::make_unique<RecordingCanvas>();
                view.draw(*canvas);
                std::vector<std::pair<Rect, Color>> found;
                for (std::size_t at = 0; at < canvas->rects.size(); ++at)
                {
                    const Rect &rect = canvas->rects[at];
                    if ((view.vertical() ? rect.height : rect.width) == doctest::Approx(1.5F))
                    {
                        found.emplace_back(rect, canvas->rect_colors[at]);
                    }
                }
                return found;
            }

            Translator                       translator;
            BeatSides                        sides;
            int                              asked = 0;
            BeatView                         view{translator,
                                                  [this]
                                                  {
                              ++asked;
                              return sides;
                                                  },
                                                  per_second};
            std::unique_ptr<RecordingCanvas> canvas;
    };
} // namespace

TEST_CASE("BeatView: horizontal lanes lie one above the other, vertical ones stand side by side, A first")
{
    Fixture fixture;
    CHECK_FALSE(fixture.view.vertical());
    CHECK(sameRect(fixture.view.laneA(), Rect{6, 6, 606, 100}));
    CHECK(sameRect(fixture.view.laneB(), Rect{6, 112, 606, 100}));

    // Turned, the lanes follow at once
    fixture.view.setVertical(true);
    CHECK(fixture.view.vertical());
    CHECK(sameRect(fixture.view.laneA(), Rect{6, 6, 300, 206}));
    CHECK(sameRect(fixture.view.laneB(), Rect{312, 6, 300, 206}));

    Fixture standing{tall, true};
    CHECK(sameRect(standing.view.laneA(), Rect{6, 6, 100, 606}));
    CHECK(sameRect(standing.view.laneB(), Rect{112, 6, 100, 606}));

    // Too small for lanes: none
    fixture.view.layout({0, 0, 10, 10});
    CHECK(fixture.view.laneA().width == 0.0F);
    CHECK(fixture.view.laneB().height == 0.0F);
}

TEST_CASE("BeatView: it asks for the sides once per frame and shows what it got")
{
    Fixture fixture;
    CHECK(fixture.asked == 1);
    fixture.sides.b = deckAt(2, 1.0, nullptr);
    fixture.view.update();
    CHECK(fixture.asked == 2);
    CHECK_FALSE(fixture.view.shown().a.has_value());
    REQUIRE(fixture.view.shown().b.has_value());
    CHECK(fixture.view.shown().b->number == 2);
}

TEST_CASE("BeatView: each lane names its side and player, the playhead is in the middle")
{
    Fixture fixture;
    fixture.sides.a = deckAt(1, 10.0, nullptr);
    fixture.view.update();
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("A  Player 01"));
    CHECK(canvas.drew("B  (none)"));
    CHECK(canvas.rounded.size() == 4); // the two lanes, each with its frame

    std::vector<Rect> playheads;
    for (std::size_t at = 0; at < canvas.rects.size(); ++at)
    {
        if (sameColor(canvas.rect_colors[at], Color{255, 255, 255}))
        {
            playheads.push_back(canvas.rects[at]);
        }
    }
    REQUIRE(playheads.size() == 2);
    CHECK(sameRect(playheads[0], Rect{9 + 299, 9, 2, 94}));
    CHECK(sameRect(playheads[1], Rect{9 + 299, 115, 2, 94}));
    // Without a waveform: no bars
    CHECK(fixture.bars().empty());
}

TEST_CASE("BeatView: the waveform scrolls with the position, the part played darker, as loud as its value")
{
    Fixture fixture;
    // Just after 10 s (the bars' borders fall between values): 9.01 s is 1 s back (100 points left of the middle),
    // 11.01 s is 1 s ahead
    fixture.sides.a = deckAt(1, 10.005, peaksWith({{901, 255}, {1101, 128}}));
    fixture.view.update();
    auto bars = fixture.bars();
    REQUIRE(bars.size() == 2);
    CHECK(sameRect(bars[0].first, Rect{9 + 200, 9, 1.5F, 94}));
    CHECK(sameRect(bars[1].first, Rect{9 + 400, 9 + ((94 - (94 * 128 / 255.0F)) / 2), 1.5F, 94 * 128 / 255.0F}));
    CHECK(sameColor(bars[0].second, Color{30, 80, 135}));  // played
    CHECK(sameColor(bars[1].second, Color{40, 140, 245})); // coming

    // Half a second later both moved 50 points to the left
    fixture.sides.a->state.position_seconds = 10.505;
    fixture.view.update();
    bars = fixture.bars();
    REQUIRE(bars.size() == 2);
    CHECK(bars[0].first.x == doctest::Approx(9 + 150));
    CHECK(bars[1].first.x == doctest::Approx(9 + 350));

    // Side B in its own lane
    fixture.sides.b = fixture.sides.a;
    fixture.sides.a.reset();
    fixture.view.update();
    bars = fixture.bars();
    REQUIRE(bars.size() == 2);
    CHECK(bars[0].first.y == doctest::Approx(115));
}

TEST_CASE("BeatView: the lane shows real time: a player pitched up runs through more of its track")
{
    Fixture fixture;
    // +100 %: twice as fast, 2.01 s of the track ahead of the position are about 1 s ahead in real time
    fixture.sides.a = deckAt(1, 10.005, peaksWith({{1201, 255}}), 100.0F);
    fixture.view.update();
    auto bars = fixture.bars();
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].first.x == doctest::Approx(9 + 400));

    // +50 %: 1.51 s ahead in the track are about 1 s in real time
    fixture.sides.a = deckAt(1, 10.005, peaksWith({{1151, 255}}), 50.0F);
    fixture.view.update();
    bars = fixture.bars();
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].first.x == doctest::Approx(9 + 400));
}

TEST_CASE("BeatView: nothing before the start of the track or after its end, nothing without a track")
{
    Fixture fixture;
    fixture.sides.a = deckAt(1, 0.505, peaksWith({{0, 255}, {1'999, 255}}));
    fixture.view.update();
    auto bars = fixture.bars();
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].first.x == doctest::Approx(9 + 250)); // the first value, half a second back

    fixture.sides.a = deckAt(1, 19.505, peaksWith({{0, 255}, {1'999, 255}}));
    fixture.view.update();
    bars = fixture.bars();
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].first.x == doctest::Approx(9 + 348)); // the last value, 0.49 s ahead

    fixture.sides.a->state.track_loaded = false;
    fixture.view.update();
    CHECK(fixture.bars().empty());
}

TEST_CASE("BeatView: the cue point is a red line where it is in time, none when it is out of sight")
{
    Fixture    fixture;
    const auto cueLines = [&fixture]
    {
        RecordingCanvas canvas;
        fixture.view.draw(canvas);
        std::vector<Rect> lines;
        for (std::size_t at = 0; at < canvas.rects.size(); ++at)
        {
            if (sameColor(canvas.rect_colors[at], Color{235, 55, 55}))
            {
                lines.push_back(canvas.rects[at]);
            }
        }
        return lines;
    };
    fixture.sides.a = deckAt(1, 10.0, peaksWith({}), 0.0F, 8.0);
    fixture.view.update();
    auto lines = cueLines();
    REQUIRE(lines.size() == 1);
    CHECK(sameRect(lines[0], Rect{9 + 100 - 1, 9, 2, 94}));

    fixture.sides.a->state.cue_seconds = 2.0; // 8 s back
    fixture.view.update();
    CHECK(cueLines().empty());
    fixture.sides.a->state.cue_seconds = 13.0; // the right edge
    fixture.view.update();
    lines = cueLines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].x == doctest::Approx(9 + 600 - 1));
    fixture.sides.a->state.cue_seconds = 13.1;
    fixture.view.update();
    CHECK(cueLines().empty());
}

TEST_CASE("BeatView: vertical, time runs down and the bars lie across the lane")
{
    Fixture fixture{tall, true};
    fixture.sides.a = deckAt(1, 10.005, peaksWith({{901, 255}, {1101, 128}}), 0.0F, 8.005);
    fixture.view.update();
    const auto bars = fixture.bars();
    REQUIRE(bars.size() == 2);
    CHECK(sameRect(bars[0].first, Rect{9, 9 + 200, 94, 1.5F}));
    CHECK(bars[1].first.y == doctest::Approx(9 + 400));
    CHECK(bars[1].first.width == doctest::Approx(94 * 128 / 255.0F));

    bool playhead = false;
    bool cue      = false;
    for (std::size_t at = 0; at < fixture.canvas->rects.size(); ++at)
    {
        playhead = playhead || (sameColor(fixture.canvas->rect_colors[at], Color{255, 255, 255}) &&
                                sameRect(fixture.canvas->rects[at], Rect{9, 9 + 299, 94, 2}));
        cue      = cue || (sameColor(fixture.canvas->rect_colors[at], Color{235, 55, 55}) &&
                           sameRect(fixture.canvas->rects[at], Rect{9, 9 + 99, 94, 2}));
    }
    CHECK(playhead);
    CHECK(cue);
}
