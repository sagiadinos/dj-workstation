// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "layout.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <span>
#include <sstream>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr std::string_view header    = "djw-layout 2";
        constexpr std::string_view header_v1 = "djw-layout 1"; // before the MDI mode: no mode, no mdi lines
        constexpr int              gap       = 12;
        // What the window frame and title bar take, so the library window below the players is not cut off
        constexpr int frame_allowance = 48;
        constexpr int min_size        = 100;
        constexpr int max_size        = 20'000;

        char sideLetter(core::api::CrossfaderSide side)
        {
            switch (side)
            {
                case core::api::CrossfaderSide::A:
                    return 'A';
                case core::api::CrossfaderSide::B:
                    return 'B';
                case core::api::CrossfaderSide::None:
                    break;
            }
            return '-';
        }

        std::optional<core::api::CrossfaderSide> sideOf(std::string_view text)
        {
            if (text == "A")
            {
                return core::api::CrossfaderSide::A;
            }
            if (text == "B")
            {
                return core::api::CrossfaderSide::B;
            }
            if (text == "-")
            {
                return core::api::CrossfaderSide::None;
            }
            return std::nullopt;
        }

        std::optional<int> numberOf(std::string_view text)
        {
            int value               = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size())
            {
                return std::nullopt;
            }
            return value;
        }

        std::vector<std::string_view> words(std::string_view line)
        {
            std::vector<std::string_view> result;
            std::size_t                   at = 0;
            while (at < line.size())
            {
                const std::size_t next = line.find(' ', at);
                const std::size_t end  = next == std::string_view::npos ? line.size() : next;
                if (end > at)
                {
                    result.push_back(line.substr(at, end - at));
                }
                at = end + 1;
            }
            return result;
        }

        /// x y width height from four words, std::nullopt for numbers that make no sense
        std::optional<Placement> placementOf(std::span<const std::string_view> four)
        {
            if (four.size() != 4)
            {
                return std::nullopt;
            }
            const auto x      = numberOf(four[0]);
            const auto y      = numberOf(four[1]);
            const auto width  = numberOf(four[2]);
            const auto height = numberOf(four[3]);
            if (!x || !y || !width || !height || *width < min_size || *height < min_size || *width > max_size ||
                *height > max_size || std::abs(*x) > max_size * 4 || std::abs(*y) > max_size * 4)
            {
                return std::nullopt;
            }
            return Placement{*x, *y, *width, *height};
        }

        void writePlacement(std::ostringstream &out, const Placement &place)
        {
            out << ' ' << place.x << ' ' << place.y << ' ' << place.width << ' ' << place.height;
        }

        std::string_view openWord(bool open)
        {
            return open ? "open" : "closed";
        }

        std::optional<bool> openOf(std::string_view text)
        {
            if (text == "open")
            {
                return true;
            }
            if (text == "closed")
            {
                return false;
            }
            return std::nullopt;
        }

        /// What the mdi lines of a layout said; the section exists once its mixer and playlist lines were read
        struct MdiLines
        {
                MdiLayout layout;
                bool      has_mixer    = false;
                bool      has_playlist = false;
                /// The places given on the lines; they count only with the library's place
                MdiPlaces places;
                bool      has_place   = false;
                bool      has_library = false;
        };

        /// The place after the first two words of an mdi line: none (two words) or x y width height (six words);
        /// false for anything else
        bool optionalPlacementOf(std::span<const std::string_view> parts, std::optional<Placement> &place)
        {
            if (parts.size() == 2)
            {
                place.reset();
                return true;
            }
            place = parts.size() == 6 ? placementOf(parts.subspan(2)) : std::nullopt;
            return place.has_value();
        }

        /// One line after "mdi"; false if it is not one
        bool parseMdiLine(std::span<const std::string_view> parts, MdiLines &lines)
        {
            if (parts.size() == 5 && parts[0] == "library" && !lines.has_library)
            {
                const auto place     = placementOf(parts.subspan(1));
                lines.places.library = place.value_or(Placement{});
                lines.has_library    = place.has_value();
                return place.has_value();
            }
            if (parts.size() < 2)
            {
                return false;
            }
            std::optional<Placement> place;
            if (!optionalPlacementOf(parts, place))
            {
                return false;
            }
            lines.has_place = lines.has_place || place.has_value();
            if (parts[0] == "player")
            {
                const auto side = sideOf(parts[1]);
                if (!side || lines.layout.players.size() >= max_layout_players)
                {
                    return false;
                }
                lines.layout.players.push_back(*side);
                lines.places.players.push_back(place);
                return true;
            }
            const auto open = openOf(parts[1]);
            if (!open)
            {
                return false;
            }
            if (parts[0] == "mixer")
            {
                lines.layout.mixer_open = *open;
                lines.places.mixer      = place;
                lines.has_mixer         = true;
                return true;
            }
            if (parts[0] == "playlist")
            {
                lines.layout.playlist_open = *open;
                lines.places.playlist      = place;
                lines.has_playlist         = true;
                return true;
            }
            return false;
        }

        /// The place of an mdi line if the parts were arranged by hand and this one has one
        void writeOptionalPlacement(std::ostringstream &out, const std::optional<Placement> &place)
        {
            if (place)
            {
                writePlacement(out, *place);
            }
        }
    } // namespace

    MdiLayout defaultMdiLayout()
    {
        return {.players       = {core::api::CrossfaderSide::A, core::api::CrossfaderSide::B},
                .mixer_open    = true,
                .playlist_open = false,
                .places        = std::nullopt};
    }

    std::string formatLayout(const Layout &layout)
    {
        std::ostringstream out;
        out << header << '\n';
        out << "mode " << (layout.mode == WindowMode::Mdi ? "mdi" : "separate") << '\n';
        for (const PlayerPlacement &player : layout.players)
        {
            out << "player " << sideLetter(player.side);
            writePlacement(out, player.place);
            out << '\n';
        }
        out << "library";
        writePlacement(out, layout.library);
        out << '\n';
        for (const auto &[name, tool] : {std::pair{"mixer", &layout.mixer}, std::pair{"playlist", &layout.playlist}})
        {
            out << name << ' ' << openWord(tool->open);
            writePlacement(out, tool->place);
            out << '\n';
        }
        if (layout.mdi)
        {
            const std::optional<MdiPlaces> &places = layout.mdi->places;
            for (std::size_t player = 0; player < layout.mdi->players.size(); ++player)
            {
                out << "mdi player " << sideLetter(layout.mdi->players[player]);
                if (places && player < places->players.size())
                {
                    writeOptionalPlacement(out, places->players[player]);
                }
                out << '\n';
            }
            out << "mdi mixer " << openWord(layout.mdi->mixer_open);
            writeOptionalPlacement(out, places ? places->mixer : std::nullopt);
            out << '\n';
            out << "mdi playlist " << openWord(layout.mdi->playlist_open);
            writeOptionalPlacement(out, places ? places->playlist : std::nullopt);
            out << '\n';
            if (places)
            {
                out << "mdi library";
                writePlacement(out, places->library);
                out << '\n';
            }
        }
        return out.str();
    }

    std::optional<Layout> parseLayout(std::string_view text)
    {
        Layout             layout;
        std::istringstream in{std::string{text}};
        std::string        line;
        if (!std::getline(in, line) || (line != header && line != header_v1))
        {
            return std::nullopt;
        }
        const bool version_1    = line == header_v1;
        bool       has_mode     = false;
        bool       has_library  = false;
        bool       has_mixer    = false;
        bool       has_playlist = false;
        MdiLines   mdi;
        bool       has_mdi = false;
        while (std::getline(in, line))
        {
            const auto parts = words(line);
            if (parts.empty())
            {
                continue;
            }
            if (!version_1 && parts[0] == "mode" && parts.size() == 2 && !has_mode &&
                (parts[1] == "separate" || parts[1] == "mdi"))
            {
                layout.mode = parts[1] == "mdi" ? WindowMode::Mdi : WindowMode::Separate;
                has_mode    = true;
            }
            else if (!version_1 && parts[0] == "mdi")
            {
                if (!parseMdiLine(std::span{parts}.subspan(1), mdi))
                {
                    return std::nullopt;
                }
                has_mdi = true;
            }
            else if (parts[0] == "player" && parts.size() == 6)
            {
                const auto side  = sideOf(parts[1]);
                const auto place = placementOf(std::span{parts}.subspan(2));
                if (!side || !place || layout.players.size() >= max_layout_players)
                {
                    return std::nullopt;
                }
                layout.players.push_back({*place, *side});
            }
            else if (parts[0] == "library" && parts.size() == 5)
            {
                const auto place = placementOf(std::span{parts}.subspan(1));
                if (!place)
                {
                    return std::nullopt;
                }
                layout.library = *place;
                has_library    = true;
            }
            else if ((parts[0] == "mixer" || parts[0] == "playlist") && parts.size() == 6 &&
                     (parts[1] == "open" || parts[1] == "closed"))
            {
                const auto place = placementOf(std::span{parts}.subspan(2));
                if (!place)
                {
                    return std::nullopt;
                }
                ToolPlacement &tool                              = parts[0] == "mixer" ? layout.mixer : layout.playlist;
                tool                                             = {.open = parts[1] == "open", .place = *place};
                (parts[0] == "mixer" ? has_mixer : has_playlist) = true;
            }
            else
            {
                return std::nullopt;
            }
        }
        // A layout with no player, or without one of the windows, is damaged; so is an mdi part without its tools,
        // or with places of parts but not the library's
        if (layout.players.empty() || !has_library || !has_mixer || !has_playlist || (!version_1 && !has_mode) ||
            (has_mdi && (!mdi.has_mixer || !mdi.has_playlist || (mdi.has_place && !mdi.has_library))))
        {
            return std::nullopt;
        }
        if (has_mdi)
        {
            layout.mdi = std::move(mdi.layout);
            if (mdi.has_library)
            {
                layout.mdi->places = std::move(mdi.places);
            }
        }
        return layout;
    }

    Placement fitInto(Placement place, const Placement &area)
    {
        place.width  = std::min(place.width, area.width);
        place.height = std::min(place.height, area.height);
        place.x      = std::clamp(place.x, area.x, std::max(area.x, area.x + area.width - place.width));
        place.y      = std::clamp(place.y, area.y, std::max(area.y, area.y + area.height - place.height));
        return place;
    }

    Layout defaultLayout(const Placement &work_area, const DefaultSizes &sizes)
    {
        const int group = sizes.player_width + gap + sizes.mixer_width + gap + sizes.player_width;
        const int left  = work_area.x + std::max(0, (work_area.width - group) / 2);
        const int top   = work_area.y;

        Layout          layout;
        const Placement first{left, top, sizes.player_width, sizes.player_height};
        const Placement mixer{left + sizes.player_width + gap, top, sizes.mixer_width, sizes.mixer_height};
        const Placement second{mixer.x + sizes.mixer_width + gap, top, sizes.player_width, sizes.player_height};
        layout.players = {{fitInto(first, work_area), core::api::CrossfaderSide::A},
                          {fitInto(second, work_area), core::api::CrossfaderSide::B}};
        layout.mixer   = {.open = true, .place = fitInto(mixer, work_area)};

        // The library below the players, as wide as they are together
        const int       library_top = top + sizes.player_height + frame_allowance;
        const int       room        = work_area.y + work_area.height - library_top - gap;
        const Placement library{left, library_top, std::min(group, work_area.width),
                                std::clamp(room, 300, sizes.library_height)};
        layout.library = fitInto(library, work_area);
        // The playlist window: to the right of the library, closed
        layout.playlist = {.open  = false,
                           .place = fitInto({library.x + library.width + gap, library_top, 560, 640}, work_area)};
        return layout;
    }
} // namespace djw::player_ui::sdl
