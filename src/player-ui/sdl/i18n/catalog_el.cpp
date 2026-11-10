// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

namespace djw::player_ui::sdl::catalogs
{
    namespace
    {
        constexpr CatalogEntry entries[] = {
            {"window.library", "djw - Βιβλιοθήκη"},
            {"window.playlist", "djw - Λίστα"},
            {"menu.database", "Βάση δεδομένων"},
            {"menu.playlist", "Λίστα"},
            {"menu.settings", "Ρυθμίσεις"},
            {"menu.view", "Προβολή"},
            {"menu.database.add_folder", "Προσθήκη φακέλου μουσικής…"},
            {"menu.database.rescan", "Επανασάρωση βιβλιοθήκης"},
            {"menu.playlist.new", "Νέα λίστα"},
            {"menu.playlist.import", "Εισαγωγή λίστας…"},
            {"menu.playlist.export", "Εξαγωγή λίστας…"},
            {"menu.playlist.add_track", "Προσθήκη επιλεγμένου κομματιού"},
            {"menu.view.clear_filters", "Εκκαθάριση αναζήτησης και φίλτρων"},
            {"menu.view.missing_only", "Εμφάνιση μόνο των κομματιών που λείπουν"},
            {"search.placeholder", "Αναζήτηση"},
            {"column.artist", "Καλλιτέχνης"},
            {"column.title", "Τίτλος"},
            {"column.genre", "Είδος"},
            {"column.year", "Έτος"},
            {"column.time", "Διάρκεια"},
            {"tracks.count", "{count, plural, one {# κομμάτι} other {# κομμάτια}}"},
            {"files.count", "{count, plural, one {# αρχείο} other {# αρχεία}}"},
            {"library.add_to_playlist", "Προσθήκη στη λίστα"},
            {"library.missing", "Λείπουν: {count}"},
            {"library.remove_missing", "Αφαίρεση όσων λείπουν"},
            {"library.remove_confirm", "Πατήστε ξανά για αφαίρεση ({count})"},
            {"library.scanning", "σάρωση, {files}"},
            {"library.saving_tags", "αποθήκευση ετικετών"},
            {"library.error.in_use", "το κομμάτι είναι φορτωμένο σε deck, οι ετικέτες του δεν αλλάζουν όσο παίζει"},
            {"library.error.year", "το έτος πρέπει να είναι αριθμός με έως τέσσερα ψηφία"},
            {"playlist.name_placeholder", "Όνομα λίστας"},
            {"playlist.default_name", "Λίστα {number}"},
            {"playlist.new", "Νέα"},
            {"playlist.delete", "Διαγραφή"},
            {"playlist.delete_confirm", "Σίγουρα;"},
            {"playlist.import", "Εισαγωγή"},
            {"playlist.export", "Εξαγωγή"},
            {"playlist.up", "Πάνω"},
            {"playlist.down", "Κάτω"},
            {"playlist.remove", "Αφαίρεση"},
            {"playlist.none", "καμία λίστα"},
            {"playlist.saving", "αποθήκευση"},
            {"playlist.error.create_first", "δημιουργήστε πρώτα μια λίστα"},
            {"playlist.imported", "εισήχθη η {name}: {tracks}, δεν βρέθηκαν {not_found}"},
            {"deck.cannot_load", "Αδυναμία φόρτωσης: {error}"},
        };
    } // namespace

    std::span<const CatalogEntry> greek()
    {
        return entries;
    }
} // namespace djw::player_ui::sdl::catalogs
