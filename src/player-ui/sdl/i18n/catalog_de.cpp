// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

namespace djw::player_ui::sdl::catalogs
{
    namespace
    {
        constexpr CatalogEntry entries[] = {
            {"window.library", "djw - Bibliothek"},
            {"window.playlist", "djw - Playlist"},
            {"menu.database", "Datenbank"},
            {"menu.playlist", "Playlist"},
            {"menu.settings", "Einstellungen"},
            {"menu.view", "Ansicht"},
            {"menu.database.add_folder", "Musikordner hinzufügen…"},
            {"menu.database.rescan", "Bibliothek neu einlesen"},
            {"menu.playlist.new", "Neue Playlist"},
            {"menu.playlist.import", "Playlist importieren…"},
            {"menu.playlist.export", "Playlist exportieren…"},
            {"menu.playlist.add_track", "Gewählten Titel hinzufügen"},
            {"menu.view.clear_filters", "Suche und Filter zurücksetzen"},
            {"menu.view.missing_only", "Nur fehlende Titel anzeigen"},
            {"search.placeholder", "Suche"},
            {"column.artist", "Interpret"},
            {"column.title", "Titel"},
            {"column.genre", "Genre"},
            {"column.year", "Jahr"},
            {"column.time", "Zeit"},
            {"tracks.count", "{count, plural, one {# Titel} other {# Titel}}"},
            {"files.count", "{count, plural, one {# Datei} other {# Dateien}}"},
            {"library.add_to_playlist", "Zur Playlist"},
            {"library.missing", "Fehlend: {count}"},
            {"library.remove_missing", "Fehlende entfernen"},
            {"library.remove_confirm", "Nochmal tippen: {count} entfernen"},
            {"library.scanning", "scannt, {files}"},
            {"library.saving_tags", "Tags werden gespeichert"},
            {"library.error.in_use", "der Titel ist in einem Deck geladen, seine Tags ändern sich während des Spielens nicht"},
            {"library.error.year", "das Jahr muss eine Zahl mit bis zu vier Ziffern sein"},
            {"playlist.name_placeholder", "Name der Playlist"},
            {"playlist.default_name", "Playlist {number}"},
            {"playlist.new", "Neu"},
            {"playlist.delete", "Löschen"},
            {"playlist.delete_confirm", "Wirklich löschen?"},
            {"playlist.import", "Import"},
            {"playlist.export", "Export"},
            {"playlist.up", "Hoch"},
            {"playlist.down", "Runter"},
            {"playlist.remove", "Entfernen"},
            {"playlist.none", "keine Playlist"},
            {"playlist.saving", "speichert"},
            {"playlist.error.create_first", "lege zuerst eine Playlist an"},
            {"playlist.imported", "{name} importiert: {tracks}, {not_found} nicht gefunden"},
            {"deck.cannot_load", "Laden nicht möglich: {error}"},
        };
    } // namespace

    std::span<const CatalogEntry> german()
    {
        return entries;
    }
} // namespace djw::player_ui::sdl::catalogs
