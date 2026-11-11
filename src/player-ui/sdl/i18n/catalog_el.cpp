// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

#include <array>

namespace djw::player_ui::sdl::catalogs
{
    namespace
    {
        constexpr auto entries = std::to_array<CatalogEntry>({
            {"window.library", "djw - Βιβλιοθήκη"},
            {"window.deck", "djw - Player {number}"},
            {"window.playlist", "djw - Λίστα"},
            {"menu.database", "Βάση δεδομένων"},
            {"menu.playlist", "Λίστα"},
            {"menu.settings", "Ρυθμίσεις"},
            {"menu.language", "Γλώσσα"},
            {"menu.view", "Προβολή"},
            {"menu.settings.none", "Δεν υπάρχουν ρυθμίσεις ακόμη"},
            {"menu.database.add_folder", "Προσθήκη φακέλου μουσικής…"},
            {"menu.database.rescan", "Επανασάρωση βιβλιοθήκης"},
            {"menu.database.analyze_all", "Ανάλυση όλων των κομματιών"},
            {"menu.playlist.new", "Νέα λίστα"},
            {"menu.playlist.import", "Εισαγωγή λίστας…"},
            {"menu.playlist.export", "Εξαγωγή λίστας…"},
            {"menu.playlist.add_track", "Προσθήκη επιλεγμένου κομματιού"},
            {"menu.view.clear_filters", "Εκκαθάριση αναζήτησης και φίλτρων"},
            {"menu.view.missing_only", "Εμφάνιση μόνο των κομματιών που λείπουν"},
            {"menu.view.save_layout", "Αποθήκευση διάταξης"},
            {"menu.view.new_deck", "Νέο player"},
            {"menu.view.playlist_window", "Παράθυρο λίστας"},
            {"search.placeholder", "Αναζήτηση"},
            {"column.artist", "Καλλιτέχνης"},
            {"column.title", "Τίτλος"},
            {"column.genre", "Είδος"},
            {"column.year", "Έτος"},
            {"column.bpm", "BPM"},
            {"column.time", "Διάρκεια"},
            {"tracks.count", "{count, plural, one {# κομμάτι} other {# κομμάτια}}"},
            {"files.count", "{count, plural, one {# αρχείο} other {# αρχεία}}"},
            {"library.add_to_playlist", "Προσθήκη στη λίστα"},
            {"library.missing", "Λείπουν: {count}"},
            {"library.remove_missing", "Αφαίρεση όσων λείπουν"},
            {"library.remove_confirm", "Πατήστε ξανά για αφαίρεση ({count})"},
            {"library.scanning", "σάρωση, {files}"},
            {"library.saving_tags", "αποθήκευση ετικετών"},
            {"library.analyzing", "ανάλυση {count, plural, one {# κομματιού} other {# κομματιών}}"},
            {"context.edit_tags", "Επεξεργασία ετικετών"},
            {"context.rescan_track", "Νέα σάρωση"},
            {"library.error.in_use", "το κομμάτι είναι φορτωμένο σε player, οι ετικέτες του δεν αλλάζουν όσο παίζει"},
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
            {"window.mixer", "djw - Crossfader"},
            {"menu.view.mixer_window", "Παράθυρο crossfader"},
            {"player.load", "Φόρτωση"},
            {"player.time", "Χρόνος"},
            {"player.keylock", "KEY"},
            {"player.cue", "CUE"},
            {"player.headphones_button", "HP"},
            {"player.cue_load", "Cue στη φόρτωση"},
            {"player.pitch", "Pitch: {value} %"},
            {"player.bpm", "{value} BPM"},
            {"player.volume", "Έντ."},
            {"player.headphones", "Ακουστ."},
            {"player.state.loaded", "φορτώθηκε"},
            {"player.state.cued", "στο cue"},
            {"player.state.paused", "παύση"},
            {"player.state.playing", "παίζει"},
            {"player.notice.playing", "δεν φορτώθηκε"},
            {"mixer.player", "Player {number}"},
            {"mixer.none", "(κανένα)"},
            {"mixer.zero", "Μηδέν"},
            {"mixer.fade_now", "Εξασθένιση τώρα!"},
            {"mixer.fade_time", "Διάρκεια (s)"},
            {"library.notice.select_first", "επιλέξτε πρώτα ένα κομμάτι στη βιβλιοθήκη"},
            {"library.notice.all_playing", "όλα τα player παίζουν"},
            {"library.notice.player_playing", "αυτό το player παίζει"},
            {"library.notice.layout_saved", "η διάταξη αποθηκεύτηκε"},
            {"library.notice.layout_failed", "η διάταξη δεν αποθηκεύτηκε"},
            {"deck.cannot_load", "Αδυναμία φόρτωσης: {error}"},
        });
    } // namespace

    std::span<const CatalogEntry> greek()
    {
        return entries;
    }
} // namespace djw::player_ui::sdl::catalogs
