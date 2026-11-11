// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "view_host.hpp"

#include "look.hpp"
#include "mdi_arrangement.hpp"

#include <algorithm>
#include <ranges>

namespace djw::player_ui::sdl
{
    void ViewHost::add(View &view, std::optional<Rect> area)
    {
        if (contains(view))
        {
            return;
        }
        panes.push_back({.view = &view, .area = area, .placed = area.value_or(Rect{})});
        rebuildRouter();
    }

    void ViewHost::remove(const View &view)
    {
        const auto pane = find(view);
        if (pane == panes.end())
        {
            return;
        }
        panes.erase(pane);
        if (focus == &view)
        {
            focus = nullptr;
        }
        rebuildRouter();
    }

    void ViewHost::place(const View &view, std::optional<Rect> area)
    {
        for (Pane &pane : panes)
        {
            if (pane.view == &view)
            {
                pane.area = area;
            }
        }
    }

    bool ViewHost::contains(const View &view) const noexcept
    {
        return find(view) != panes.end();
    }

    std::optional<Rect> ViewHost::areaOf(const View &view) const noexcept
    {
        const auto pane = find(view);
        if (pane == panes.end())
        {
            return std::nullopt;
        }
        return pane->placed;
    }

    View *ViewHost::focused() const noexcept
    {
        if (focus != nullptr)
        {
            return focus;
        }
        return panes.empty() ? nullptr : panes.front().view;
    }

    void ViewHost::layout(const Rect &window)
    {
        window_area = window;
        for (Pane &pane : panes)
        {
            // A view being dragged shows where it would go
            pane.placed = drag && drag->view == pane.view ? drag->target : pane.area.value_or(window);
            pane.view->layout(pane.placed);
        }
        if (arrange)
        {
            constexpr float done_width  = 180.0F;
            constexpr float done_height = 48.0F;
            done_button.setBounds(
                {window.x + ((window.width - done_width) / 2), window.y + 12.0F, done_width, done_height});
        }
    }

    void ViewHost::update()
    {
        for (const Pane &pane : panes)
        {
            pane.view->update();
        }
        if (arrange)
        {
            if (arrange->done_label)
            {
                done_button.setLabel(arrange->done_label());
            }
            if (done_button.takePress() && arrange->done)
            {
                arrange->done();
            }
        }
    }

    void ViewHost::draw(Canvas &canvas) const
    {
        for (const Pane &pane : panes)
        {
            pane.view->draw(canvas);
        }
        if (arrange)
        {
            drawArranging(canvas);
        }
    }

    void ViewHost::startArranging(Arranging arranging)
    {
        arrange = std::move(arranging);
        drag.reset();
        done_pointer.reset();
    }

    void ViewHost::stopArranging()
    {
        arrange.reset();
        drag.reset();
        done_pointer.reset();
        done_button.setBounds({});
    }

    std::optional<Rect> ViewHost::dragTarget() const noexcept
    {
        return drag ? std::optional{drag->target} : std::nullopt;
    }

    void ViewHost::arrangePointer(const PointerEvent &event)
    {
        const Point at = event.position;
        switch (event.phase)
        {
            case PointerPhase::Down:
                if (drag || done_pointer)
                {
                    return; // one finger at a time
                }
                if (done_button.bounds().contains(at))
                {
                    done_button.pointerDown(at);
                    done_pointer = event.pointer;
                    return;
                }
                for (const Pane &pane : std::views::reverse(panes))
                {
                    if (pane.area && pane.placed.contains(at))
                    {
                        const Rect &area = pane.placed;
                        const bool  corner =
                            at.x >= area.x + area.width - resize_handle && at.y >= area.y + area.height - resize_handle;
                        drag = Drag{.pointer = event.pointer,
                                    .view    = pane.view,
                                    .start   = area,
                                    .from    = at,
                                    .resize  = corner,
                                    .target  = area,
                                    .fits    = true};
                        return;
                    }
                }
                return;
            case PointerPhase::Move:
                if (done_pointer == event.pointer)
                {
                    done_button.pointerMove(at);
                }
                if (drag && drag->pointer == event.pointer)
                {
                    const float dx    = at.x - drag->from.x;
                    const float dy    = at.y - drag->from.y;
                    const Rect &start = drag->start;
                    drag->target =
                        drag->resize ? snapResize({start.x, start.y, start.width + dx, start.height + dy}, window_area)
                                     : snapMove({start.x + dx, start.y + dy, start.width, start.height}, window_area);
                    drag->fits = std::ranges::none_of(
                        panes, [this](const Pane &other)
                        { return other.view != drag->view && other.area && overlap(drag->target, other.placed); });
                }
                return;
            case PointerPhase::Up:
            case PointerPhase::Cancel:
                if (done_pointer == event.pointer)
                {
                    // A cancelled press does not count: let go far outside the button
                    done_button.pointerUp(event.phase == PointerPhase::Up ? at : Point{-1.0E6F, -1.0E6F});
                    done_pointer.reset();
                }
                if (drag && drag->pointer == event.pointer)
                {
                    const Drag ended = *drag;
                    drag.reset();
                    if (event.phase == PointerPhase::Up && ended.fits)
                    {
                        place(*ended.view, ended.target);
                        if (arrange && arrange->moved)
                        {
                            arrange->moved(*ended.view, ended.target);
                        }
                    }
                }
                return;
        }
    }

    void ViewHost::drawArranging(Canvas &canvas) const
    {
        const Rect &window = window_area;
        // The views dimmed, so it is clear they do not react, with the grid over them
        canvas.fillRect(window, {0, 0, 0, 110});
        constexpr Color grid_color{255, 255, 255, 22};
        const auto      columns = static_cast<int>(window.width / mdi_grid);
        const auto      rows    = static_cast<int>(window.height / mdi_grid);
        for (int column = 0; column <= columns; ++column)
        {
            canvas.fillRect({window.x + (static_cast<float>(column) * mdi_grid), window.y, 1.0F, window.height},
                            grid_color);
        }
        for (int row = 0; row <= rows; ++row)
        {
            canvas.fillRect({window.x, window.y + (static_cast<float>(row) * mdi_grid), window.width, 1.0F},
                            grid_color);
        }
        const auto frame = [&canvas](const Rect &area, Color color, float line)
        {
            canvas.fillRect({area.x, area.y, area.width, line}, color);
            canvas.fillRect({area.x, area.y + area.height - line, area.width, line}, color);
            canvas.fillRect({area.x, area.y, line, area.height}, color);
            canvas.fillRect({area.x + area.width - line, area.y, line, area.height}, color);
        };
        for (const Pane &pane : panes)
        {
            if (!pane.area)
            {
                continue;
            }
            const Rect &area  = pane.placed;
            const bool  held  = drag && drag->view == pane.view;
            const Color color = held ? (drag->fits ? Color{70, 200, 90} : Color{230, 60, 50}) : look::accent_blue;
            frame(area, color, 2.0F);
            // The resize handle: a triangle in the lower right corner
            const float right  = area.x + area.width;
            const float bottom = area.y + area.height;
            canvas.fillTriangle({right, bottom - resize_handle}, {right, bottom}, {right - resize_handle, bottom},
                                color);
        }
        done_button.draw(canvas);
    }

    void ViewHost::pointer(const PointerEvent &event)
    {
        if (arrange)
        {
            arrangePointer(event);
            return;
        }
        if (event.phase == PointerPhase::Down)
        {
            if (View *under = viewAt(event.position))
            {
                focus = under;
            }
        }
        router.handle(event);
    }

    void ViewHost::keyboard(const KeyboardInput &input)
    {
        if (arrange)
        {
            if (input == KeyboardInput{EditKey::Escape} && arrange->done)
            {
                arrange->done();
            }
            return;
        }
        if (View *view = focused())
        {
            view->keyboard(input);
        }
    }

    void ViewHost::wheel(Point position, float dy)
    {
        if (arrange)
        {
            return;
        }
        if (View *view = viewAt(position))
        {
            view->wheel(position, dy);
        }
    }

    void ViewHost::secondaryClick(Point position)
    {
        if (arrange)
        {
            return;
        }
        if (View *view = viewAt(position))
        {
            view->secondaryClick(position);
        }
    }

    bool ViewHost::wantsTextInput() const
    {
        if (arrange)
        {
            return false;
        }
        const View *view = focused();
        return view != nullptr && view->wantsTextInput();
    }

    View *ViewHost::viewAt(Point position) const noexcept
    {
        for (const Pane &pane : std::views::reverse(panes))
        {
            if (pane.placed.contains(position))
            {
                return pane.view;
            }
        }
        return nullptr;
    }

    std::vector<ViewHost::Pane>::const_iterator ViewHost::find(const View &view) const noexcept
    {
        return std::ranges::find(panes, &view, &Pane::view);
    }

    void ViewHost::rebuildRouter()
    {
        router = PointerRouter{};
        for (const Pane &pane : panes)
        {
            pane.view->addTo(router);
        }
    }
} // namespace djw::player_ui::sdl
