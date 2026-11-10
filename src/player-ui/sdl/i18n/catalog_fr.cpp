// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

namespace djw::player_ui::sdl::catalogs
{
    namespace
    {
        constexpr CatalogEntry entries[] = {
            {"window.library", "djw - Bibliothèque"},
            {"window.playlist", "djw - Liste de lecture"},
            {"menu.database", "Base de données"},
            {"menu.playlist", "Liste de lecture"},
            {"menu.settings", "Réglages"},
            {"menu.language", "Langue"},
            {"menu.view", "Affichage"},
            {"menu.settings.none", "Pas encore de réglages"},
            {"menu.database.add_folder", "Ajouter un dossier de musique…"},
            {"menu.database.rescan", "Réanalyser la bibliothèque"},
            {"menu.playlist.new", "Nouvelle liste de lecture"},
            {"menu.playlist.import", "Importer une liste de lecture…"},
            {"menu.playlist.export", "Exporter la liste de lecture…"},
            {"menu.playlist.add_track", "Ajouter le titre sélectionné"},
            {"menu.view.clear_filters", "Effacer la recherche et les filtres"},
            {"menu.view.missing_only", "Afficher seulement les titres manquants"},
            {"menu.view.playlist_window", "Fenêtre de la liste de lecture"},
            {"search.placeholder", "Recherche"},
            {"column.artist", "Artiste"},
            {"column.title", "Titre"},
            {"column.genre", "Genre"},
            {"column.year", "Année"},
            {"column.time", "Durée"},
            {"tracks.count", "{count, plural, one {# piste} many {# de pistes} other {# pistes}}"},
            {"files.count", "{count, plural, one {# fichier} many {# de fichiers} other {# fichiers}}"},
            {"library.add_to_playlist", "Ajouter à la liste"},
            {"library.missing", "Manquants : {count}"},
            {"library.remove_missing", "Retirer les manquants"},
            {"library.remove_confirm", "Touchez encore pour retirer {count}"},
            {"library.scanning", "analyse, {files}"},
            {"library.saving_tags", "enregistrement des tags"},
            {"library.error.in_use", "le titre est chargé dans une platine, ses tags ne changent pas pendant la lecture"},
            {"library.error.year", "l'année doit être un nombre d'au plus quatre chiffres"},
            {"playlist.name_placeholder", "Nom de la liste"},
            {"playlist.default_name", "Liste {number}"},
            {"playlist.new", "Nouvelle"},
            {"playlist.delete", "Supprimer"},
            {"playlist.delete_confirm", "Vraiment ?"},
            {"playlist.import", "Importer"},
            {"playlist.export", "Exporter"},
            {"playlist.up", "Monter"},
            {"playlist.down", "Descendre"},
            {"playlist.remove", "Retirer"},
            {"playlist.none", "aucune liste"},
            {"playlist.saving", "enregistrement"},
            {"playlist.error.create_first", "créez d'abord une liste"},
            {"playlist.imported", "{name} importée : {tracks}, {not_found} introuvables"},
            {"deck.cannot_load", "Chargement impossible : {error}"},
        };
    } // namespace

    std::span<const CatalogEntry> french()
    {
        return entries;
    }
} // namespace djw::player_ui::sdl::catalogs
