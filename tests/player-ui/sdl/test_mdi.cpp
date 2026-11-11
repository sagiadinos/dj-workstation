// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mdi_arrangement.hpp"
#include "pane_frame.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;

namespace
{
    constexpr MdiSizes sizes{.player_width = 380, .player_height = 544, .mixer_width = 440, .playlist_width = 560};

    bool same(float first, float second)
    {
        return std::abs(first - second) < 0.01F;
    }

    bool inside(const Rect &part, const Rect &window)
    {
        return part.x >= window.x - 0.01F && part.y >= window.y - 0.01F &&
               part.x + part.width <= window.x + window.width + 0.01F &&
               part.y + part.height <= window.y + window.height + 0.01F;
    }

    std::vector<Rect> all(const MdiArrangement &placed)
    {
        std::vector<Rect> parts = placed.players;
        for (const auto &part : {placed.mixer, placed.playlist, placed.library})
        {
            if (part)
            {
                parts.push_back(*part);
            }
        }
        return parts;
    }

    /// No part overlaps another or leaves the window
    void checkTidy(const MdiArrangement &placed, const Rect &window)
    {
        const auto parts = all(placed);
        for (std::size_t first = 0; first < parts.size(); ++first)
        {
            INFO("part " << first);
            CHECK(inside(parts[first], window));
            for (std::size_t second = first + 1; second < parts.size(); ++second)
            {
                CHECK_FALSE(overlap(parts[first], parts[second]));
            }
        }
    }

    /// A view that remembers what it was given
    class Content final : public View
    {
        public:
            void layout(const Rect &area) override { laid_out = area; }
            void addTo(PointerRouter & /*router*/) override { ++added; }
            void update() override { ++updates; }
            void draw(Canvas &canvas) const override { canvas.text({laid_out.x, laid_out.y}, "content", {}); }
            [[nodiscard]] bool wantsTextInput() const override { return true; }
            void               keyboard(const KeyboardInput               &/*input*/) override { ++keys; }
            void               wheel(Point /*position*/, float dy) override { wheeled += dy; }
            void               secondaryClick(Point /*position*/) override { ++secondary; }

            Rect  laid_out;
            int   added     = 0;
            int   updates   = 0;
            int   keys      = 0;
            float wheeled   = 0.0F;
            int   secondary = 0;
    };

    void press(PointerRouter &router, const Rect &area)
    {
        const Point center{area.x + (area.width / 2), area.y + (area.height / 2)};
        router.handle({{1, 1}, PointerPhase::Down, center});
        router.handle({{1, 1}, PointerPhase::Up, center});
    }
} // namespace

TEST_CASE("MDI: two players with the crossfader between them on top, the library below over the whole width")
{
    constexpr Rect window{0, 0, 1920, 1080};
    const auto     placed = arrangeMdi(window, {.players = 2, .mixer = true, .library = true}, sizes);

    REQUIRE(placed.players.size() == 2);
    REQUIRE(placed.mixer.has_value());
    REQUIRE(placed.library.has_value());
    CHECK_FALSE(placed.playlist.has_value());
    // Room enough: the sizes they are made for, from left to right, centered
    CHECK(same(placed.players[0].width, 380));
    CHECK(same(placed.players[0].height, 544));
    CHECK(placed.players[0].x < placed.mixer->x);
    CHECK(placed.mixer->x < placed.players[1].x);
    const float left  = placed.players[0].x - window.x;
    const float right = window.x + window.width - (placed.players[1].x + placed.players[1].width);
    CHECK(same(left, right));
    // The library takes the rest
    CHECK(same(placed.library->x, mdi_gap));
    CHECK(same(placed.library->width, window.width - (2 * mdi_gap)));
    CHECK(same(placed.library->y, placed.players[0].y + placed.players[0].height + mdi_gap));
    CHECK(same(placed.library->y + placed.library->height, window.height - mdi_gap));
    checkTidy(placed, window);
}

TEST_CASE("MDI: a row too wide for the window shrinks as a whole and keeps the proportions of the players")
{
    constexpr Rect window{0, 0, 1280, 800};
    const auto     placed = arrangeMdi(window, {.players = 4, .mixer = true, .library = true}, sizes);
    REQUIRE(placed.players.size() == 4);
    for (const Rect &player : placed.players)
    {
        CHECK(player.width < 380);
        CHECK(same(player.width / player.height, 380.0F / 544.0F));
    }
    // From edge to edge, less the gaps
    CHECK(same(placed.players[0].x, mdi_gap));
    CHECK(same(placed.players[3].x + placed.players[3].width, window.width - mdi_gap));
    checkTidy(placed, window);
}

TEST_CASE("MDI: with the library below, the row takes at most its share of the height")
{
    constexpr Rect window{0, 0, 3000, 700};
    const auto     placed = arrangeMdi(window, {.players = 1, .library = true}, sizes);
    CHECK(placed.players[0].height <= (window.height * mdi_row_share) + 0.01F);
    CHECK(placed.library->height > 200);
    checkTidy(placed, window);

    // Without anything below the player may use the height, but grows no bigger than it is made
    const auto alone = arrangeMdi(window, {.players = 1}, sizes);
    CHECK(same(alone.players[0].height, sizes.player_height));
    const Rect low{0, 0, 3000, 400};
    const auto squeezed = arrangeMdi(low, {.players = 1}, sizes);
    CHECK(same(squeezed.players[0].height, low.height - (2 * mdi_gap)));
    checkTidy(alone, window);
    checkTidy(squeezed, low);
}

TEST_CASE("MDI: the playlist goes to the right of the library, at most its size and share")
{
    constexpr Rect window{100, 50, 1920, 1080}; // the window need not start at 0
    const auto     placed = arrangeMdi(window, {.players = 2, .mixer = true, .playlist = true, .library = true}, sizes);
    REQUIRE(placed.playlist.has_value());
    CHECK(same(placed.playlist->width, 560));
    CHECK(same(placed.playlist->y, placed.library->y));
    CHECK(same(placed.playlist->x, placed.library->x + placed.library->width + mdi_gap));
    CHECK(same(placed.playlist->x + placed.playlist->width, window.x + window.width - mdi_gap));
    checkTidy(placed, window);

    const Rect narrow{0, 0, 800, 900};
    const auto small = arrangeMdi(narrow, {.players = 1, .playlist = true, .library = true}, sizes);
    CHECK(small.playlist->width <= (narrow.width - (3 * mdi_gap)) * 0.4F + 0.01F);
    checkTidy(small, narrow);
}

TEST_CASE("MDI: without players the crossfader or the library alone, nothing at all without content")
{
    constexpr Rect window{0, 0, 1920, 1080};
    const auto     mixer_only = arrangeMdi(window, {.mixer = true, .library = true}, sizes);
    CHECK(mixer_only.players.empty());
    REQUIRE(mixer_only.mixer.has_value());
    CHECK(same(mixer_only.mixer->width, 440));
    CHECK(mixer_only.library->y > mixer_only.mixer->y + mixer_only.mixer->height);

    const auto library_only = arrangeMdi(window, {.library = true}, sizes);
    CHECK(same(library_only.library->y, mdi_gap));
    CHECK(same(library_only.library->height, window.height - (2 * mdi_gap)));

    const auto empty = arrangeMdi(window, {}, sizes);
    CHECK(all(empty).empty());
}

TEST_CASE("MDI: a window smaller than everything still gives no part a negative size")
{
    constexpr Rect window{0, 0, 20, 20};
    const auto     placed = arrangeMdi(window, {.players = 3, .mixer = true, .playlist = true, .library = true}, sizes);
    for (const Rect &part : all(placed))
    {
        CHECK(part.width >= 0.0F);
        CHECK(part.height >= 0.0F);
    }
}

TEST_CASE("PaneFrame: a title bar on top, the content below it")
{
    Content   content;
    PaneFrame frame{content, [] { return std::string{"Player 1"}; }};
    frame.layout({10, 20, 400, 300});
    CHECK(same(frame.bar().y, 20));
    CHECK(same(frame.bar().height, PaneFrame::title_height));
    CHECK(same(content.laid_out.y, 20 + PaneFrame::title_height));
    CHECK(same(content.laid_out.height, 300 - PaneFrame::title_height));
    CHECK(same(content.laid_out.x, 10));
    CHECK(same(content.laid_out.width, 400));

    RecordingCanvas canvas;
    frame.draw(canvas);
    CHECK(canvas.drew("Player 1"));
    CHECK(canvas.drew("content"));
    // Without a close action there is no close button
    CHECK(same(frame.closeButton().bounds().width, 0));

    // Smaller than the bar: no negative content
    frame.layout({0, 0, 100, 10});
    CHECK(same(content.laid_out.height, 0));
}

TEST_CASE("PaneFrame: the close button at the right end of the bar asks for the pane to go, on update")
{
    Content       content;
    int           closed = 0;
    PaneFrame     frame{content, [] { return std::string{"Crossfader"}; }, [&closed] { ++closed; }};
    PointerRouter router;
    frame.layout({0, 0, 400, 300});
    frame.addTo(router);
    CHECK(content.added == 1);

    const Rect button = frame.closeButton().bounds();
    CHECK(button.width > 0);
    CHECK(same(button.x + button.width + 3, 400));
    CHECK(frame.bar().contains({button.x + 1, button.y + 1}));

    press(router, button);
    CHECK(closed == 0); // not while the pointer is handled
    frame.update();
    CHECK(closed == 1);
    CHECK(content.updates == 1);
    frame.update();
    CHECK(closed == 1); // once per press
}

TEST_CASE("PaneFrame: typed text, the wheel and the right button go to the content")
{
    Content   content;
    PaneFrame frame{content, {}};
    CHECK(frame.wantsTextInput());
    frame.keyboard(std::string{"a"});
    frame.wheel({1, 1}, 2.0F);
    frame.secondaryClick({1, 1});
    CHECK(content.keys == 1);
    CHECK(same(content.wheeled, 2.0F));
    CHECK(content.secondary == 1);
    CHECK(&frame.inner() == &content);

    // Without a title only the content is drawn
    RecordingCanvas canvas;
    frame.layout({0, 0, 100, 100});
    frame.draw(canvas);
    REQUIRE(canvas.texts.size() == 1);
    CHECK(canvas.texts[0].text == "content");
}

TEST_CASE("MDI by hand: a moved pane snaps its corner to the grid and stays inside the window")
{
    const Rect window{0, 0, 1000, 800};
    const Rect moved = snapMove({133, 47, 300, 200}, window);
    CHECK(same(moved.x, 140));
    CHECK(same(moved.y, 40));
    CHECK(same(moved.width, 300));
    CHECK(same(moved.height, 200));

    // Dragged over the edges: back inside, on the grid
    const Rect out = snapMove({-50, 790, 300, 200}, window);
    CHECK(same(out.x, 0));
    CHECK(same(out.y, 600));
    const Rect right = snapMove({711, 10, 300, 200}, window);
    CHECK(same(right.x, 700));
    CHECK(inside(right, window));

    // The grid counts from the window's corner
    const Rect offset = snapMove({113, 57, 100, 100}, {5, 5, 1000, 800});
    CHECK(same(offset.x, 105));
    CHECK(same(offset.y, 65));
}

TEST_CASE("MDI by hand: a resized pane snaps its lower right corner, keeps the smallest size and stays inside")
{
    const Rect window{0, 0, 1000, 800};
    const Rect bigger = snapResize({100, 100, 309, 191}, window);
    CHECK(same(bigger.x, 100));
    CHECK(same(bigger.y, 100));
    CHECK(same(bigger.width, 300));
    CHECK(same(bigger.height, 200));

    const Rect tiny = snapResize({100, 100, 10, 10}, window);
    CHECK(same(tiny.width, mdi_min_width));
    CHECK(same(tiny.height, mdi_min_height));

    const Rect huge = snapResize({100, 100, 5000, 5000}, window);
    CHECK(same(huge.width, 900));
    CHECK(same(huge.height, 700));
}

TEST_CASE("MDI by hand: panes that only touch do not overlap, keepInside moves and shrinks")
{
    CHECK_FALSE(overlap({0, 0, 100, 100}, {100, 0, 100, 100}));
    CHECK_FALSE(overlap({0, 0, 100, 100}, {0, 100, 100, 100}));
    CHECK(overlap({0, 0, 100, 100}, {99, 99, 100, 100}));

    const Rect kept = keepInside({900, -20, 300, 2000}, {0, 0, 1000, 800});
    CHECK(same(kept.x, 700));
    CHECK(same(kept.y, 0));
    CHECK(same(kept.height, 800));
}

TEST_CASE("MDI by hand: a new pane takes the first free place on the grid, smaller if it must, or none")
{
    const Rect window{0, 0, 1000, 800};
    const auto first = freePlace(300, 200, window, {{0, 0, 500, 800}});
    REQUIRE(first.has_value());
    CHECK(same(first->x, 500));
    CHECK(same(first->y, 0));
    CHECK(same(first->width, 300));

    // Only 400 x 400 left: the pane of 600 x 600 comes in at two thirds
    const auto smaller = freePlace(600, 600, window, {{0, 0, 600, 800}, {600, 400, 400, 400}});
    REQUIRE(smaller.has_value());
    CHECK(smaller->width <= 400);
    CHECK(smaller->height <= 400);
    CHECK(smaller->width >= 300);
    CHECK(inside(*smaller, window));

    CHECK_FALSE(freePlace(300, 200, window, {window}).has_value());
}
