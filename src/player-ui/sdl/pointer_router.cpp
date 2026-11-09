// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "pointer_router.hpp"

#include <algorithm>
#include <ranges>

namespace djw::player_ui::sdl
{
    void PointerRouter::add(Widget &widget)
    {
        widgets.push_back(&widget);
    }

    void PointerRouter::handle(const PointerEvent &event)
    {
        if (event.phase == PointerPhase::Down)
        {
            Widget *widget = widgetAt(event.position);
            if (widget != nullptr && !isCaptured(*widget))
            {
                captures[event.pointer] = widget;
                widget->pointerDown(event.position);
            }
            return;
        }

        const auto capture = captures.find(event.pointer);
        if (capture == captures.end())
        {
            return;
        }
        if (event.phase == PointerPhase::Move)
        {
            capture->second->pointerMove(event.position);
            return;
        }
        Widget *widget = capture->second;
        captures.erase(capture);
        widget->pointerUp(event.position);
    }

    bool PointerRouter::isCaptured(const Widget &widget) const noexcept
    {
        return std::ranges::any_of(captures, [&widget](const auto &capture) { return capture.second == &widget; });
    }

    Widget *PointerRouter::widgetAt(Point position) const noexcept
    {
        for (Widget *widget : std::views::reverse(widgets))
        {
            if (widget->bounds().contains(position))
            {
                return widget;
            }
        }
        return nullptr;
    }
} // namespace djw::player_ui::sdl
