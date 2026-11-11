// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "track_list.hpp"

#include "time_format.hpp"
#include "track_columns.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding = 10.0F;

        constexpr Color row_color{28, 28, 28};
        constexpr Color alternate_row_color{34, 34, 34};
        constexpr Color selected_row_color{40, 75, 120};
        constexpr Color text_color{230, 230, 230};
        constexpr Color secondary_text_color{160, 160, 160};
        constexpr Color missing_text_color{205, 105, 105}; // the file is gone: dead link

    } // namespace

    void TrackList::setTracks(std::vector<core::api::LibraryTrack> tracks, bool keep_position)
    {
        rows = std::move(tracks);
        if (!keep_position)
        {
            fling.reset();
            offset = 0.0F;
            selection.reset();
            last_tap_row.reset();
        }
        else if (selection && *selection >= rows.size())
        {
            selection.reset();
        }
        offset = std::clamp(offset, 0.0F, maxOffset());
    }

    std::optional<std::size_t> TrackList::takeActivated() noexcept
    {
        return std::exchange(activated, std::nullopt);
    }

    void TrackList::moveSelection(std::ptrdiff_t by) noexcept
    {
        fling.reset(); // the keys take over
        if (rows.empty())
        {
            return;
        }
        const auto last = static_cast<std::ptrdiff_t>(rows.size()) - 1;
        const auto from = selection ? static_cast<std::ptrdiff_t>(*selection) : (by > 0 ? -1 : last + 1);
        const auto row  = static_cast<std::size_t>(std::clamp(from + by, std::ptrdiff_t{0}, last));
        selection       = row;
        // Into view: at the top edge going up, at the bottom edge going down
        const float top    = static_cast<float>(row) * row_height;
        const float bottom = top + row_height;
        if (top < offset)
        {
            offset = top;
        }
        else if (bottom > offset + bounds().height)
        {
            offset = bottom - bounds().height;
        }
        offset = std::clamp(offset, 0.0F, maxOffset());
    }

    std::size_t TrackList::pageRows() const noexcept
    {
        return std::max<std::size_t>(1, static_cast<std::size_t>(bounds().height / row_height));
    }

    bool TrackList::navigate(EditKey key) noexcept
    {
        const auto page = static_cast<std::ptrdiff_t>(pageRows());
        switch (key)
        {
            case EditKey::Up:
                moveSelection(-1);
                return true;
            case EditKey::Down:
                moveSelection(1);
                return true;
            case EditKey::PageUp:
                moveSelection(-page);
                return true;
            case EditKey::PageDown:
                moveSelection(page);
                return true;
            case EditKey::Backspace:
            case EditKey::Enter:
            case EditKey::Escape:
            case EditKey::Tab:
            case EditKey::BackTab:
            case EditKey::Left:
            case EditKey::Right:
            case EditKey::ShiftLeft:
            case EditKey::ShiftRight:
                break;
        }
        return false;
    }

    void TrackList::scrollBy(float dy) noexcept
    {
        fling.reset();
        offset = std::clamp(offset - dy, 0.0F, maxOffset());
    }

    float TrackList::maxOffset() const noexcept
    {
        return std::max(0.0F, (static_cast<float>(rows.size()) * row_height) - bounds().height);
    }

    std::optional<std::size_t> TrackList::rowAt(Point position) const noexcept
    {
        if (!bounds().contains(position))
        {
            return std::nullopt;
        }
        const auto row = static_cast<std::size_t>((position.y - bounds().y + offset) / row_height);
        return row < rows.size() ? std::optional{row} : std::nullopt;
    }

    std::optional<TrackList::Cell> TrackList::cellAt(Point position) const noexcept
    {
        const auto row = rowAt(position);
        if (!row)
        {
            return std::nullopt;
        }
        for (const core::api::LibraryColumn column : track_columns)
        {
            if (columnArea(bounds(), column).contains(position))
            {
                return Cell{*row, column};
            }
        }
        return std::nullopt;
    }

    std::optional<TrackList::Cell> TrackList::takeLongPress()
    {
        if (!held || dragging || long_pressed || clock() - down_time < long_press_time)
        {
            return std::nullopt;
        }
        long_pressed    = true;
        const auto cell = cellAt(start);
        if (cell)
        {
            selection = cell->row;
        }
        return cell;
    }

    void TrackList::replaceTrack(std::size_t row, core::api::LibraryTrack track)
    {
        if (row < rows.size())
        {
            rows[row] = std::move(track);
        }
    }

    std::optional<Rect> TrackList::cellArea(const Cell &cell) const
    {
        const float y = bounds().y + (static_cast<float>(cell.row) * row_height) - offset;
        if (cell.row >= rows.size() || y < bounds().y || y + row_height > bounds().y + bounds().height)
        {
            return std::nullopt;
        }
        return columnArea({bounds().x, y, bounds().width, row_height}, cell.column);
    }

    void TrackList::pointerDown(Point position)
    {
        fling.reset(); // a touch catches a running list
        samples.clear();
        start        = position;
        down_time    = clock();
        start_offset = offset;
        held         = true;
        dragging     = false;
        long_pressed = false;
        touched      = true;
    }

    void TrackList::pointerMove(Point position)
    {
        if (!dragging && std::abs(position.y - start.y) > tap_slop)
        {
            dragging = true;
        }
        if (dragging)
        {
            offset         = std::clamp(start_offset - (position.y - start.y), 0.0F, maxOffset());
            const auto now = clock();
            samples.push_back({.time = now, .offset = offset});
            std::erase_if(samples, [now](const Sample &sample) { return now - sample.time > fling_sample_time; });
        }
    }

    void TrackList::pointerUp(Point position)
    {
        if (!dragging && !long_pressed)
        {
            tap(position);
        }
        else if (dragging)
        {
            startFling();
        }
        held     = false;
        dragging = false;
        samples.clear();
    }

    void TrackList::startFling()
    {
        // The speed of the moves just before the finger lifted; a finger that came to rest gives no fling
        const auto now = clock();
        std::erase_if(samples, [now](const Sample &sample) { return now - sample.time > fling_sample_time; });
        if (samples.size() < 2)
        {
            return;
        }
        const double seconds = std::chrono::duration<double>(samples.back().time - samples.front().time).count();
        if (seconds <= 0.0)
        {
            return;
        }
        const double speed = static_cast<double>(samples.back().offset - samples.front().offset) / seconds;
        if (std::abs(speed) >= fling_min_speed)
        {
            fling = Fling{.start = now, .from = offset, .speed = speed};
        }
    }

    void TrackList::advance()
    {
        if (!fling)
        {
            return;
        }
        // The speed falls exponentially; the way gone is its integral
        const double seconds = std::chrono::duration<double>(clock() - fling->start).count();
        const double fading  = std::exp(-seconds / fling_time_constant);
        const double way     = fling->speed * fling_time_constant * (1.0 - fading);
        const float  target  = fling->from + static_cast<float>(way);
        offset               = std::clamp(target, 0.0F, maxOffset());
        if (offset != target || std::abs(fling->speed * fading) < fling_stop_speed)
        {
            fling.reset(); // at an end of the list, or slow enough to stop
        }
    }

    void TrackList::tap(Point position)
    {
        const auto row = rowAt(position);
        if (!row)
        {
            return;
        }
        const auto now = clock();
        if (last_tap_row == row && now - last_tap_time <= double_tap_time)
        {
            activated = row;
            last_tap_row.reset(); // a third tap starts a new double tap
        }
        else
        {
            last_tap_row  = row;
            last_tap_time = now;
        }
        selection = row;
    }

    void TrackList::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        canvas.setClip(area);

        const auto  first   = static_cast<std::size_t>(offset / row_height);
        const float text_dy = (row_height - canvas.textHeight()) / 2;
        for (std::size_t index = first; index < rows.size(); ++index)
        {
            const float y = area.y + (static_cast<float>(index) * row_height) - offset;
            if (y >= area.y + area.height)
            {
                break;
            }
            const Color background = selection == index ? selected_row_color
                                     : index % 2 == 0   ? row_color
                                                        : alternate_row_color;
            if (selection == index)
            {
                canvas.fillRoundedRect({area.x, y, area.width, row_height}, 0.0F, {62, 112, 172}, {36, 72, 118});
            }
            else
            {
                canvas.fillRect({area.x, y, area.width, row_height}, background);
            }

            const auto &track  = rows[index];
            const float text_y = y + text_dy;
            const auto  cells  = columnAreas({area.x, y, area.width, row_height});
            const auto  cell   = [&](core::api::LibraryColumn column, const std::string &text, Color color)
            {
                const Rect &cell_area = cells[columnIndex(column)];
                canvas.textElided({cell_area.x + padding, text_y}, text, color, cell_area.width - (2 * padding));
            };
            const Color primary   = track.missing ? missing_text_color : text_color;
            const Color secondary = track.missing ? missing_text_color : secondary_text_color;
            cell(core::api::LibraryColumn::Artist, track.artist, primary);
            cell(core::api::LibraryColumn::Title, track.title, primary);
            cell(core::api::LibraryColumn::Genre, track.genre, secondary);
            cell(core::api::LibraryColumn::Year, track.year != 0 ? std::to_string(track.year) : std::string{},
                 secondary);
            cell(core::api::LibraryColumn::Bpm, track.bpm ? formatBpm(*track.bpm) : std::string{}, secondary);
            cell(core::api::LibraryColumn::Duration, formatDuration(track.duration_seconds), secondary);
        }
        canvas.setClip(std::nullopt);
    }
} // namespace djw::player_ui::sdl
