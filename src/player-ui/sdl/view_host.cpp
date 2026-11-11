// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "view_host.hpp"

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
        for (Pane &pane : panes)
        {
            pane.placed = pane.area.value_or(window);
            pane.view->layout(pane.placed);
        }
    }

    void ViewHost::update()
    {
        for (const Pane &pane : panes)
        {
            pane.view->update();
        }
    }

    void ViewHost::draw(Canvas &canvas) const
    {
        for (const Pane &pane : panes)
        {
            pane.view->draw(canvas);
        }
    }

    void ViewHost::pointer(const PointerEvent &event)
    {
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
        if (View *view = focused())
        {
            view->keyboard(input);
        }
    }

    void ViewHost::wheel(Point position, float dy)
    {
        if (View *view = viewAt(position))
        {
            view->wheel(position, dy);
        }
    }

    void ViewHost::secondaryClick(Point position)
    {
        if (View *view = viewAt(position))
        {
            view->secondaryClick(position);
        }
    }

    bool ViewHost::wantsTextInput() const
    {
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
