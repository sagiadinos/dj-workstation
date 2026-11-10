// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "column_header.hpp"
#include "track_columns.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;

TEST_CASE("columnAreas: the columns fill the area left to right without gaps")
{
    const Rect area{10, 20, 700, 30};
    const auto areas = columnAreas(area);

    CHECK(areas.front().x == 10);
    for (std::size_t index = 1; index < areas.size(); ++index)
    {
        CHECK(areas[index].x == doctest::Approx(areas[index - 1].x + areas[index - 1].width));
        CHECK(areas[index].width > 0);
    }
    CHECK(areas.back().x + areas.back().width == doctest::Approx(710));
    for (const Rect &column : areas)
    {
        CHECK(column.y == 20);
        CHECK(column.height == 30);
    }
    CHECK(columnArea(area, api::LibraryColumn::Title).x == areas[columnIndex(api::LibraryColumn::Title)].x);
}

TEST_CASE("ColumnHeader: a tap on a column reports it once, a drag to another column reports nothing")
{
    ColumnHeader header;
    header.setBounds({0, 0, 700, 30});
    const Rect year  = columnArea(header.bounds(), api::LibraryColumn::Year);
    const Rect genre = columnArea(header.bounds(), api::LibraryColumn::Genre);

    header.pointerDown({year.x + 5, 10});
    header.pointerUp({year.x + 5, 10});
    CHECK(header.takeTapped() == api::LibraryColumn::Year);
    CHECK_FALSE(header.takeTapped());

    header.pointerDown({year.x + 5, 10});
    header.pointerUp({genre.x + 5, 10});
    CHECK_FALSE(header.takeTapped());
}

TEST_CASE("ColumnHeader: draws every column name")
{
    ColumnHeader header;
    header.setBounds({0, 0, 700, 30});
    djw::tests::RecordingCanvas canvas;
    header.draw(canvas);
    for (const auto column : track_columns)
    {
        CHECK(canvas.drew(columnName(column)));
    }
}
