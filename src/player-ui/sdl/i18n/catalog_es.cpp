// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

namespace djw::player_ui::sdl::catalogs
{
    namespace
    {
        constexpr CatalogEntry entries[] = {
            {"window.library", "djw - Biblioteca"},
            {"window.deck", "djw - Bandeja {number}"},
            {"window.playlist", "djw - Lista"},
            {"menu.database", "Base de datos"},
            {"menu.playlist", "Lista"},
            {"menu.settings", "Ajustes"},
            {"menu.language", "Idioma"},
            {"menu.view", "Vista"},
            {"menu.settings.none", "Aún no hay ajustes"},
            {"menu.database.add_folder", "Añadir carpeta de música…"},
            {"menu.database.rescan", "Volver a analizar la biblioteca"},
            {"menu.playlist.new", "Nueva lista"},
            {"menu.playlist.import", "Importar lista…"},
            {"menu.playlist.export", "Exportar lista…"},
            {"menu.playlist.add_track", "Añadir la pista seleccionada"},
            {"menu.view.clear_filters", "Borrar búsqueda y filtros"},
            {"menu.view.missing_only", "Mostrar solo las pistas que faltan"},
            {"menu.view.new_deck", "Nueva bandeja"},
            {"menu.view.playlist_window", "Ventana de la lista"},
            {"search.placeholder", "Buscar"},
            {"column.artist", "Artista"},
            {"column.title", "Título"},
            {"column.genre", "Género"},
            {"column.year", "Año"},
            {"column.time", "Tiempo"},
            {"tracks.count", "{count, plural, one {# pista} many {# de pistas} other {# pistas}}"},
            {"files.count", "{count, plural, one {# archivo} many {# de archivos} other {# archivos}}"},
            {"library.add_to_playlist", "Añadir a la lista"},
            {"library.missing", "Faltan: {count}"},
            {"library.remove_missing", "Quitar las que faltan"},
            {"library.remove_confirm", "Toca de nuevo para quitar {count}"},
            {"library.scanning", "analizando, {files}"},
            {"library.saving_tags", "guardando etiquetas"},
            {"library.error.in_use", "la pista está cargada en una bandeja, sus etiquetas no cambian mientras suena"},
            {"library.error.year", "el año debe ser un número de hasta cuatro cifras"},
            {"playlist.name_placeholder", "Nombre de la lista"},
            {"playlist.default_name", "Lista {number}"},
            {"playlist.new", "Nueva"},
            {"playlist.delete", "Borrar"},
            {"playlist.delete_confirm", "¿Seguro?"},
            {"playlist.import", "Importar"},
            {"playlist.export", "Exportar"},
            {"playlist.up", "Subir"},
            {"playlist.down", "Bajar"},
            {"playlist.remove", "Quitar"},
            {"playlist.none", "sin lista"},
            {"playlist.saving", "guardando"},
            {"playlist.error.create_first", "crea primero una lista"},
            {"playlist.imported", "{name} importada: {tracks}, {not_found} no encontradas"},
            {"deck.cannot_load", "No se puede cargar: {error}"},
        };
    } // namespace

    std::span<const CatalogEntry> spanish()
    {
        return entries;
    }
} // namespace djw::player_ui::sdl::catalogs
