// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "view_host.hpp"

#include "deck_view.hpp"
#include "fake_dialogs.hpp"
#include "fake_library.hpp"
#include "library_view.hpp"
#include "mixer_view.hpp"
#include "playlist_view.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;
namespace api = djw::core::api;

namespace
{
    /// A widget that counts what it gets
    class CountingWidget final : public Widget
    {
        public:
            void pointerDown(Point /*position*/) override { ++downs; }
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point /*position*/) override { ++ups; }
            void draw(Canvas & /*canvas*/) const override {}

            int downs = 0;
            int ups   = 0;
    };

    /// A view with one widget that fills its area; records what reaches it
    class FakeView final : public View
    {
        public:
            explicit FakeView(std::string name) : name{std::move(name)} {}

            void layout(const Rect &area) override
            {
                laid_out = area;
                widget.setBounds(area);
            }
            void               addTo(PointerRouter &router) override { router.add(widget); }
            void               update() override { ++updates; }
            void               draw(Canvas &canvas) const override { canvas.text({laid_out.x, laid_out.y}, name, {}); }
            [[nodiscard]] bool wantsTextInput() const override { return wants_text; }
            void               keyboard(const KeyboardInput               &/*input*/) override { ++keys; }
            void               wheel(Point /*position*/, float dy) override { wheeled += dy; }
            void               secondaryClick(Point /*position*/) override { ++secondary; }

            std::string    name;
            Rect           laid_out;
            CountingWidget widget;
            int            updates    = 0;
            int            keys       = 0;
            float          wheeled    = 0.0F;
            int            secondary  = 0;
            bool           wants_text = false;
    };

    constexpr Rect window{0, 0, 800, 600};
    constexpr Rect left{0, 0, 400, 600};
    constexpr Rect right{400, 0, 400, 600};

    void tap(ViewHost &host, Point position)
    {
        host.pointer({{1, 1}, PointerPhase::Down, position});
        host.pointer({{1, 1}, PointerPhase::Up, position});
    }

    bool same(float first, float second)
    {
        return std::abs(first - second) < 0.01F;
    }

    bool shifted(const Rect &moved, const Rect &original, Point by)
    {
        return same(moved.x, original.x + by.x) && same(moved.y, original.y + by.y) &&
               same(moved.width, original.width) && same(moved.height, original.height);
    }

    /**
     * Draws the view laid out at the origin and laid out moved by offset, and checks that everything drawn the
     * second time lies exactly offset from the first: the view uses the area it is given, not the window.
     */
    void checkUsesItsArea(View &view, Rect area)
    {
        constexpr Point offset{137, 59};
        RecordingCanvas at_origin;
        view.layout(area);
        view.update();
        view.draw(at_origin);

        RecordingCanvas moved;
        view.layout({area.x + offset.x, area.y + offset.y, area.width, area.height});
        view.update();
        view.draw(moved);

        // Something must be drawn, or there is nothing to compare
        REQUIRE(at_origin.rects.size() + at_origin.rounded.size() > 0);
        REQUIRE_FALSE(at_origin.texts.empty());
        REQUIRE(moved.rects.size() == at_origin.rects.size());
        for (std::size_t index = 0; index < moved.rects.size(); ++index)
        {
            INFO("rect " << index);
            CHECK(shifted(moved.rects[index], at_origin.rects[index], offset));
        }
        REQUIRE(moved.rounded.size() == at_origin.rounded.size());
        for (std::size_t index = 0; index < moved.rounded.size(); ++index)
        {
            INFO("rounded rect " << index);
            CHECK(shifted(moved.rounded[index].rect, at_origin.rounded[index].rect, offset));
        }
        REQUIRE(moved.texts.size() == at_origin.texts.size());
        for (std::size_t index = 0; index < moved.texts.size(); ++index)
        {
            INFO("text " << moved.texts[index].text);
            CHECK(moved.texts[index].text == at_origin.texts[index].text);
            CHECK(same(moved.texts[index].position.x, at_origin.texts[index].position.x + offset.x));
            CHECK(same(moved.texts[index].position.y, at_origin.texts[index].position.y + offset.y));
        }
    }
} // namespace

TEST_CASE("ViewHost: a view without an area fills the window, one with an area gets that area")
{
    ViewHost host;
    FakeView whole{"whole"};
    FakeView part{"part"};
    host.add(whole);
    host.add(part, right);
    host.layout(window);

    CHECK(shifted(whole.laid_out, window, {}));
    CHECK(shifted(part.laid_out, right, {}));
    REQUIRE(host.areaOf(part).has_value());
    CHECK(shifted(*host.areaOf(part), right, {}));
    CHECK(host.size() == 2);
}

TEST_CASE("ViewHost: place moves a view from the next layout on, remove takes it out")
{
    ViewHost host;
    FakeView view{"view"};
    host.add(view, left);
    host.layout(window);
    host.place(view, right);
    CHECK(shifted(view.laid_out, left, {})); // not before the next layout
    host.layout(window);
    CHECK(shifted(view.laid_out, right, {}));
    host.place(view, std::nullopt);
    host.layout(window);
    CHECK(shifted(view.laid_out, window, {}));

    host.remove(view);
    CHECK(host.empty());
    CHECK_FALSE(host.contains(view));
    CHECK_FALSE(host.areaOf(view).has_value());
    host.remove(view); // unknown: nothing happens
    CHECK(host.empty());
}

TEST_CASE("ViewHost: a view is added once; update and draw reach every view in the order added")
{
    ViewHost host;
    FakeView first{"first"};
    FakeView second{"second"};
    host.add(first, left);
    host.add(second, right);
    host.add(first, right); // already there: ignored, keeps its area
    CHECK(host.size() == 2);

    host.layout(window);
    host.update();
    RecordingCanvas canvas;
    host.draw(canvas);

    CHECK(shifted(first.laid_out, left, {}));
    CHECK(first.updates == 1);
    CHECK(second.updates == 1);
    REQUIRE(canvas.texts.size() == 2);
    CHECK(canvas.texts[0].text == "first");
    CHECK(canvas.texts[1].text == "second");
}

TEST_CASE("ViewHost: pointer events reach the widgets of every view, not of a removed one")
{
    ViewHost host;
    FakeView first{"first"};
    FakeView second{"second"};
    host.add(first, left);
    host.add(second, right);
    host.layout(window);

    tap(host, {100, 100});
    tap(host, {500, 100});
    CHECK(first.widget.downs == 1);
    CHECK(first.widget.ups == 1);
    CHECK(second.widget.downs == 1);

    host.remove(second);
    tap(host, {500, 100});
    CHECK(second.widget.downs == 1);
}

TEST_CASE("ViewHost: typed text goes to the view a pointer last went down on, the first view until then")
{
    ViewHost host;
    FakeView first{"first"};
    FakeView second{"second"};
    host.add(first, left);
    host.add(second, right);
    host.layout(window);

    CHECK(host.focused() == &first);
    host.keyboard(std::string{"a"});
    CHECK(first.keys == 1);

    tap(host, {500, 100});
    CHECK(host.focused() == &second);
    host.keyboard(std::string{"b"});
    CHECK(second.keys == 1);
    CHECK(first.keys == 1);

    // Text input follows the focus
    first.wants_text = true;
    CHECK_FALSE(host.wantsTextInput());
    tap(host, {100, 100});
    CHECK(host.wantsTextInput());

    // A focused view that is removed hands the focus back to the first one
    host.remove(first);
    CHECK(host.focused() == &second);
}

TEST_CASE("ViewHost: the wheel and the right button go to the view under the pointer")
{
    ViewHost host;
    FakeView first{"first"};
    FakeView second{"second"};
    host.add(first, left);
    host.add(second, Rect{400, 0, 400, 300});
    host.layout(window);

    host.wheel({500, 100}, 2.0F);
    host.secondaryClick({500, 100});
    CHECK(second.wheeled == doctest::Approx(2.0F));
    CHECK(second.secondary == 1);
    CHECK(first.wheeled == doctest::Approx(0.0F));

    host.wheel({100, 100}, -1.0F);
    CHECK(first.wheeled == doctest::Approx(-1.0F));

    // Under no view: nobody gets it
    host.wheel({500, 500}, 3.0F);
    host.secondaryClick({500, 500});
    CHECK(second.wheeled == doctest::Approx(2.0F));
    CHECK(second.secondary == 1);
    CHECK(first.secondary == 0);
}

TEST_CASE("ViewHost: without views nothing happens")
{
    ViewHost host;
    host.layout(window);
    host.update();
    RecordingCanvas canvas;
    host.draw(canvas);
    host.keyboard(std::string{"a"});
    host.wheel({1, 1}, 1.0F);
    host.secondaryClick({1, 1});
    tap(host, {1, 1});
    CHECK(host.focused() == nullptr);
    CHECK_FALSE(host.wantsTextInput());
    CHECK(canvas.texts.empty());
}

namespace
{
    /// Two views side by side, arranged; records what the owner hears
    struct ArrangeFixture
    {
            ArrangeFixture()
            {
                host.add(first, Rect{0, 0, 300, 200});
                host.add(second, Rect{400, 0, 300, 200});
                host.layout(window);
                host.startArranging({.moved =
                                         [this](const View &view, const Rect &area)
                                     {
                                         moved_view = &view;
                                         moved_to   = area;
                                     },
                                     .done       = [this] { ++done; },
                                     .done_label = [] { return std::string{"Done"}; }});
                host.layout(window);
                host.update();
            }

            void drag(Point from, Point to)
            {
                host.pointer({{1, 1}, PointerPhase::Down, from});
                host.pointer({{1, 1}, PointerPhase::Move, to});
                host.layout(window);
            }

            void release(Point at)
            {
                host.pointer({{1, 1}, PointerPhase::Up, at});
                host.layout(window);
            }

            ViewHost    host;
            FakeView    first{"first"};
            FakeView    second{"second"};
            const View *moved_view = nullptr;
            Rect        moved_to;
            int         done = 0;
    };
} // namespace

TEST_CASE("ViewHost arranging: a view dragged anywhere moves with the finger, snaps to the grid and the owner hears")
{
    ArrangeFixture fixture;
    CHECK(fixture.host.arranging());
    fixture.drag({100, 100}, {133, 307});
    // While dragged it shows where it would go
    REQUIRE(fixture.host.dragTarget().has_value());
    CHECK(shifted(fixture.first.laid_out, {40, 200, 300, 200}, {}));
    CHECK(fixture.host.dragFits());

    fixture.release({133, 307});
    CHECK(fixture.moved_view == &fixture.first);
    CHECK(shifted(fixture.moved_to, {40, 200, 300, 200}, {}));
    CHECK(shifted(fixture.first.laid_out, {40, 200, 300, 200}, {}));
    // The widgets of the views got nothing
    CHECK(fixture.first.widget.downs == 0);
    CHECK(fixture.second.widget.downs == 0);
}

TEST_CASE("ViewHost arranging: the lower right corner resizes; a view let go over another goes back")
{
    ArrangeFixture fixture;
    fixture.drag({290, 190}, {350, 250});
    fixture.release({350, 250});
    CHECK(fixture.moved_view == &fixture.first);
    CHECK(shifted(fixture.moved_to, {0, 0, 360, 260}, {}));

    fixture.moved_view = nullptr;
    fixture.drag({450, 100}, {200, 100}); // the second over the first
    CHECK_FALSE(fixture.host.dragFits());
    fixture.release({200, 100});
    CHECK(fixture.moved_view == nullptr);
    CHECK(shifted(fixture.second.laid_out, {400, 0, 300, 200}, {}));
}

TEST_CASE("ViewHost arranging: Done and Escape end it, the wheel, the right button and typing reach no view")
{
    ArrangeFixture fixture;
    fixture.host.wheel({100, 100}, 1.0F);
    fixture.host.secondaryClick({100, 100});
    fixture.host.keyboard(std::string{"a"});
    CHECK(fixture.first.wheeled == 0.0F);
    CHECK(fixture.first.secondary == 0);
    CHECK(fixture.first.keys == 0);
    fixture.first.wants_text = true;
    CHECK_FALSE(fixture.host.wantsTextInput());

    fixture.host.keyboard(EditKey::Escape);
    CHECK(fixture.done == 1);

    const Rect done = fixture.host.doneButton().bounds();
    REQUIRE(done.width > 0);
    RecordingCanvas canvas;
    fixture.host.draw(canvas);
    CHECK(canvas.drew("Done"));
    tap(fixture.host, {done.x + 5, done.y + 5});
    fixture.host.update();
    CHECK(fixture.done == 2);

    fixture.host.stopArranging();
    CHECK_FALSE(fixture.host.arranging());
    tap(fixture.host, {100, 100});
    CHECK(fixture.first.widget.downs == 1); // the views work again
}

TEST_CASE("ViewHost: a view always on top is drawn last and gets the pointer first, also over views added later")
{
    ViewHost host;
    FakeView top{"top"};
    FakeView below{"below"};
    host.add(top, Rect{0, 0, 800, 600}, true);
    host.add(below, Rect{0, 0, 800, 600});
    host.layout(window);

    RecordingCanvas canvas;
    host.draw(canvas);
    REQUIRE(canvas.texts.size() == 2);
    CHECK(canvas.texts[1].text == "top");

    tap(host, {100, 100});
    CHECK(top.widget.downs == 1);
    CHECK(below.widget.downs == 0);
}

TEST_CASE("ViewHost arranging: the work area bounds the moves, views always on top are not arranged")
{
    ArrangeFixture fixture;
    FakeView       bar{"bar"};
    fixture.host.add(bar, Rect{0, 0, 800, 40}, true);
    fixture.host.setWorkArea(Rect{0, 40, 800, 560});
    fixture.host.layout(window);

    // The first view (at the top of the window) dragged up: it stays below the bar, on the grid from 40
    fixture.drag({100, 100}, {100, 50});
    fixture.release({100, 50});
    REQUIRE(fixture.moved_view == &fixture.first);
    CHECK(shifted(fixture.moved_to, {0, 40, 300, 200}, {}));

    // A press on the bar drags nothing
    fixture.moved_view = nullptr;
    fixture.drag({750, 20}, {750, 300});
    CHECK_FALSE(fixture.host.dragTarget().has_value());
    fixture.release({750, 300});
    CHECK(fixture.moved_view == nullptr);
}

TEST_CASE("ViewHost: with several views side by side the one the keys go to has a line around it")
{
    ViewHost host;
    FakeView first{"first"};
    FakeView second{"second"};
    host.add(first, left);
    host.add(second, right);
    host.layout(window);
    tap(host, {500, 100});
    REQUIRE(host.focused() == &second);

    RecordingCanvas canvas;
    host.draw(canvas);
    const auto line = std::ranges::find_if(canvas.rects,
                                           [](const Rect &rect)
                                           {
                                               return same(rect.x, right.x) && same(rect.y, right.y) &&
                                                      same(rect.width, right.width) &&
                                                      same(rect.height, ViewHost::focus_line);
                                           });
    CHECK(line != canvas.rects.end());

    // One view alone has none
    ViewHost        alone;
    FakeView        only{"only"};
    RecordingCanvas plain;
    alone.add(only, left);
    alone.layout(window);
    alone.draw(plain);
    CHECK(plain.rects.empty());
}

TEST_CASE("Views use the area they are given, not the window: the player")
{
    api::DeckSnapshotBuffer snapshot{api::DeckSnapshot{.deck = api::DeckId{1}, .track_loaded = true}};
    api::CommandQueue       queue;
    const Translator        translator;
    DeckView                view{api::DeckId{1}, queue, snapshot, translator};
    checkUsesItsArea(view, {0, 0, 380, 514});
}

TEST_CASE("Views use the area they are given, not the window: the crossfader")
{
    api::MixerSnapshotBuffer snapshot{api::MixerSnapshot{}};
    api::CommandQueue        queue;
    const Translator         translator;
    MixerView                view{queue, snapshot, translator, [] { return std::vector<MixerDeck>{}; },
                                  [] { return std::chrono::steady_clock::time_point{}; }};
    checkUsesItsArea(view, {0, 0, 440, 300});
}

TEST_CASE("Views use the area they are given, not the window: the library")
{
    djw::tests::FakeLibrary library;
    djw::tests::FakeDialogs dialogs;
    const Translator        translator;
    LibraryView             view{library, dialogs, translator, [](const api::LibraryTrack &) {}};
    checkUsesItsArea(view, {0, 0, 900, 600});
}

TEST_CASE("Views use the area they are given, not the window: the playlist")
{
    djw::tests::FakeLibrary library;
    djw::tests::FakeDialogs dialogs;
    const Translator        translator;
    PlaylistView            view{library, dialogs, translator, [](const api::LibraryTrack &) {}};
    checkUsesItsArea(view, {0, 0, 420, 600});
}
