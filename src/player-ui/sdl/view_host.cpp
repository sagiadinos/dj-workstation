// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "view_host.hpp"

#include "look.hpp"
#include "mdi_arrangement.hpp"

#include <algorithm>
#include <ranges>

namespace djw::player_ui::sdl
{
    void ViewHost::add(View &view, std::optional<Rect> area, bool always_on_top)
    {
        if (contains(view))
        {
            return;
        }
        panes.push_back({.view = &view, .area = area, .placed = area.value_or(Rect{}), .on_top = always_on_top});
        rebuildRouter();
    }

    std::vector<const ViewHost::Pane *> ViewHost::inOrder() const
    {
        std::vector<const Pane *> ordered;
        ordered.reserve(panes.size());
        for (const bool top : {false, true})
        {
            for (const Pane &pane : panes)
            {
                if (pane.on_top == top)
                {
                    ordered.push_back(&pane);
                }
            }
        }
        return ordered;
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
        if (selected == &view)
        {
            selected = nullptr;
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
        placeLogo();
        if (arrange)
        {
            constexpr float done_width  = 180.0F;
            constexpr float done_height = 48.0F;
            const Rect      area        = arrangeArea();
            done_button.setBounds({area.x + ((area.width - done_width) / 2), area.y + 12.0F, done_width, done_height});
        }
    }

    void ViewHost::update()
    {
        for (const Pane &pane : panes)
        {
            pane.view->update();
        }
        if (const Widget *under = arrange ? nullptr : router.hovered(); under != tip_widget)
        {
            tip_widget = under;
            tip_since  = clock();
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

    void ViewHost::placeLogo()
    {
        logo_area.reset();
        std::vector<Rect> taken;
        for (const Pane &pane : panes)
        {
            if (pane.area && !pane.on_top)
            {
                taken.push_back(pane.placed);
            }
        }
        if (taken.empty())
        {
            return; // one view over the whole window: no free part
        }
        const auto free = largestFree(arrangeArea(), taken);
        if (!free)
        {
            return;
        }
        constexpr float aspect = aspectOf(Picture::Logo);
        const float     width =
            std::min({free->width - (2 * logo_margin), (free->height - (2 * logo_margin)) * aspect, logo_max_width});
        if (width < logo_min_width)
        {
            return;
        }
        const float height = width / aspect;
        logo_area = Rect{free->x + ((free->width - width) / 2), free->y + ((free->height - height) / 2), width, height};
    }

    void ViewHost::draw(Canvas &canvas) const
    {
        if (logo_area)
        {
            canvas.picture(Picture::Logo, *logo_area, logo_alpha);
        }
        bool focus_shown = false;
        for (const Pane *pane : inOrder())
        {
            if (pane->on_top && !focus_shown)
            {
                drawFocus(canvas); // over the parts, under what is always on top
                focus_shown = true;
            }
            canvas.setTextScale(1.0F); // a view that scales its text (a player) does so for itself only
            pane->view->draw(canvas);
        }
        canvas.setTextScale(1.0F);
        if (!focus_shown)
        {
            drawFocus(canvas);
        }
        if (arrange)
        {
            drawArranging(canvas);
        }
        drawTip(canvas);
    }

    std::string ViewHost::shownTip() const
    {
        // The router's widget, not tip_widget: the router forgets a widget whose view went
        const Widget *under = router.hovered();
        if (arrange || under == nullptr || under != tip_widget || clock() - tip_since < tip_delay)
        {
            return {};
        }
        return under->tip();
    }

    void ViewHost::drawTip(Canvas &canvas) const
    {
        const std::string tip = shownTip();
        if (tip.empty())
        {
            return;
        }
        constexpr float padding = 6.0F;
        constexpr Point offset{12.0F, 20.0F}; // below the right of the pointer, clear of its arrow
        const float     width  = canvas.textWidth(tip) + (2 * padding);
        const float     height = canvas.textHeight() + (2 * padding);
        const Point     at     = router.hoverPosition();
        const float     right  = window_area.x + window_area.width;
        const float     bottom = window_area.y + window_area.height;
        // Left of or above the pointer where it would leave the window
        const float x = at.x + offset.x + width <= right ? at.x + offset.x : std::max(window_area.x, right - width);
        const float y =
            at.y + offset.y + height <= bottom ? at.y + offset.y : std::max(window_area.y, at.y - height - 4);
        const Rect bubble{x, y, width, height};
        canvas.fillRect(bubble, look::colors().text_dim);
        canvas.fillRect(bubble.inset(1.0F), look::colors().bubble);
        canvas.text({x + padding, y + padding}, tip, look::colors().text);
    }

    void ViewHost::startArranging(Arranging arranging)
    {
        arrange = std::move(arranging);
        drag.reset();
        done_pointer.reset();
        selected = nullptr;
        selectNext(false); // the first view that can be arranged
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
                    if (pane.area && !pane.on_top && pane.placed.contains(at))
                    {
                        const Rect &area = pane.placed;
                        const bool  corner =
                            at.x >= area.x + area.width - resize_handle && at.y >= area.y + area.height - resize_handle;
                        selected = pane.view;
                        drag     = Drag{.pointer = event.pointer,
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
                    const Rect  area  = arrangeArea();
                    // The other arranged parts, whose edges it snaps to
                    std::vector<Rect> others;
                    for (const Pane &other : panes)
                    {
                        if (other.view != drag->view && other.area && !other.on_top)
                        {
                            others.push_back(other.placed);
                        }
                    }
                    drag->target = drag->resize ? snapResize({start.x, start.y, start.width + dx, start.height + dy},
                                                             area, mdi_grid, others)
                                                : snapMove({start.x + dx, start.y + dy, start.width, start.height},
                                                           area, mdi_grid, others);
                    drag->fits   = std::ranges::none_of(panes,
                                                        [this](const Pane &other)
                                                        {
                                                          return other.view != drag->view && other.area &&
                                                                 !other.on_top && overlap(drag->target, other.placed);
                                                        });
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

    void ViewHost::drawFocus(Canvas &canvas) const
    {
        // Only where several parts share the window: then it shows which one the keys go to
        const auto parts =
            std::ranges::count_if(panes, [](const Pane &pane) { return pane.area.has_value() && !pane.on_top; });
        const View *view = focused();
        const auto  pane = std::ranges::find(panes, view, &Pane::view);
        if (parts < 2 || pane == panes.end() || !pane->area || pane->on_top || arrange)
        {
            return;
        }
        const Rect &area = pane->placed;
        canvas.fillRect({area.x, area.y, area.width, focus_line}, look::colors().focus);
        canvas.fillRect({area.x, area.y + area.height - focus_line, area.width, focus_line}, look::colors().focus);
        canvas.fillRect({area.x, area.y, focus_line, area.height}, look::colors().focus);
        canvas.fillRect({area.x + area.width - focus_line, area.y, focus_line, area.height}, look::colors().focus);
    }

    void ViewHost::drawArranging(Canvas &canvas) const
    {
        const Rect window = arrangeArea();
        // The views dimmed, so it is clear they do not react, with the grid over them
        canvas.fillRect(window, {0, 0, 0, 110});
        constexpr Color grid_color{255, 255, 255, 22};
        // Every fourth line of the grid: the full grid would cover the window
        constexpr float shown   = mdi_grid * 4;
        const auto      columns = static_cast<int>(window.width / shown);
        const auto      rows    = static_cast<int>(window.height / shown);
        for (int column = 0; column <= columns; ++column)
        {
            canvas.fillRect({window.x + (static_cast<float>(column) * shown), window.y, 1.0F, window.height},
                            grid_color);
        }
        for (int row = 0; row <= rows; ++row)
        {
            canvas.fillRect({window.x, window.y + (static_cast<float>(row) * shown), window.width, 1.0F}, grid_color);
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
            if (!pane.area || pane.on_top)
            {
                continue;
            }
            const Rect &area   = pane.placed;
            const bool  held   = drag && drag->view == pane.view;
            const bool  marked = !drag && selected == pane.view; // the keys go to it: thicker and lighter
            const Color color  = held     ? (drag->fits ? Color{70, 200, 90} : Color{230, 60, 50})
                                 : marked ? Color{120, 190, 255}
                                          : look::accent_blue;
            frame(area, color, marked ? 4.0F : 2.0F);
            // The resize handle: a triangle in the lower right corner
            const float right  = area.x + area.width;
            const float bottom = area.y + area.height;
            canvas.fillTriangle({right, bottom - resize_handle}, {right, bottom}, {right - resize_handle, bottom},
                                color);
        }
        done_button.draw(canvas);
    }

    void ViewHost::selectNext(bool backwards)
    {
        std::vector<const View *> arrangeable;
        for (const Pane &pane : panes)
        {
            if (pane.area && !pane.on_top)
            {
                arrangeable.push_back(pane.view);
            }
        }
        if (arrangeable.empty())
        {
            selected = nullptr;
            return;
        }
        const auto current = std::ranges::find(arrangeable, selected);
        if (current == arrangeable.end())
        {
            selected = backwards ? arrangeable.back() : arrangeable.front();
            return;
        }
        const auto count = static_cast<std::ptrdiff_t>(arrangeable.size());
        const auto at    = current - arrangeable.begin();
        selected         = arrangeable[static_cast<std::size_t>((at + (backwards ? count - 1 : 1)) % count)];
    }

    void ViewHost::nudge(EditKey key)
    {
        struct Step
        {
                float dx     = 0.0F;
                float dy     = 0.0F;
                bool  resize = false;
        };
        constexpr float small = 1.0F;
        constexpr float big   = 10.0F;
        Step            step;
        switch (key)
        {
            case EditKey::Left:
                step = {-small, 0.0F, false};
                break;
            case EditKey::Right:
                step = {small, 0.0F, false};
                break;
            case EditKey::Up:
                step = {0.0F, -small, false};
                break;
            case EditKey::Down:
                step = {0.0F, small, false};
                break;
            case EditKey::ShiftLeft:
                step = {-big, 0.0F, false};
                break;
            case EditKey::ShiftRight:
                step = {big, 0.0F, false};
                break;
            case EditKey::ShiftUp:
                step = {0.0F, -big, false};
                break;
            case EditKey::ShiftDown:
                step = {0.0F, big, false};
                break;
            case EditKey::CtrlLeft:
                step = {-small, 0.0F, true};
                break;
            case EditKey::CtrlRight:
                step = {small, 0.0F, true};
                break;
            case EditKey::CtrlUp:
                step = {0.0F, -small, true};
                break;
            case EditKey::CtrlDown:
                step = {0.0F, small, true};
                break;
            case EditKey::CtrlShiftLeft:
                step = {-big, 0.0F, true};
                break;
            case EditKey::CtrlShiftRight:
                step = {big, 0.0F, true};
                break;
            case EditKey::CtrlShiftUp:
                step = {0.0F, -big, true};
                break;
            case EditKey::CtrlShiftDown:
                step = {0.0F, big, true};
                break;
            default:
                return;
        }
        const auto pane = std::ranges::find(panes, selected, &Pane::view);
        if (drag || pane == panes.end())
        {
            return;
        }
        const std::optional<Rect> put = pane->area;
        if (!put)
        {
            return;
        }
        // From where it was put last, which may be newer than the last layout (several keys within one frame)
        Rect target = *put;
        if (step.resize)
        {
            target.width += step.dx;
            target.height += step.dy;
        }
        else
        {
            target.x += step.dx;
            target.y += step.dy;
        }
        const Rect area       = arrangeArea();
        const bool inside     = target.x >= area.x && target.y >= area.y &&
                                target.x + target.width <= area.x + area.width &&
                                target.y + target.height <= area.y + area.height;
        const bool big_enough = target.width >= mdi_min_width && target.height >= mdi_min_height;
        const bool free       = std::ranges::none_of(panes,
                                                     [&](const Pane &other)
                                                     {
                                                   return other.view != selected && other.area && !other.on_top &&
                                                          overlap(target, other.area.value_or(other.placed));
                                                     });
        if (!inside || !big_enough || !free)
        {
            return;
        }
        place(*selected, target);
        if (arrange && arrange->moved)
        {
            arrange->moved(*selected, target);
        }
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
            const auto *key = std::get_if<EditKey>(&input);
            if (key == nullptr)
            {
                return;
            }
            if (*key == EditKey::Escape && arrange->done)
            {
                arrange->done();
            }
            else if (*key == EditKey::Tab || *key == EditKey::BackTab)
            {
                selectNext(*key == EditKey::BackTab);
            }
            else
            {
                nudge(*key);
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
        for (const Pane *pane : inOrder())
        {
            pane->view->addTo(router); // added later lies on top
        }
    }
} // namespace djw::player_ui::sdl
