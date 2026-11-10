// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"

namespace djw::player_ui::sdl::catalogs
{
    namespace
    {
        constexpr CatalogEntry entries[] = {
            {"window.library", "djw - Библиотека"},
            {"window.playlist", "djw - Плейлист"},
            {"menu.database", "База данных"},
            {"menu.playlist", "Плейлист"},
            {"menu.settings", "Настройки"},
            {"menu.view", "Вид"},
            {"menu.database.add_folder", "Добавить папку с музыкой…"},
            {"menu.database.rescan", "Пересканировать библиотеку"},
            {"menu.playlist.new", "Новый плейлист"},
            {"menu.playlist.import", "Импорт плейлиста…"},
            {"menu.playlist.export", "Экспорт плейлиста…"},
            {"menu.playlist.add_track", "Добавить выбранный трек"},
            {"menu.view.clear_filters", "Сбросить поиск и фильтры"},
            {"menu.view.missing_only", "Показать только отсутствующие треки"},
            {"search.placeholder", "Поиск"},
            {"column.artist", "Исполнитель"},
            {"column.title", "Название"},
            {"column.genre", "Жанр"},
            {"column.year", "Год"},
            {"column.time", "Время"},
            {"tracks.count", "{count, plural, one {# трек} few {# трека} many {# треков} other {# трека}}"},
            {"files.count", "{count, plural, one {# файл} few {# файла} many {# файлов} other {# файла}}"},
            {"library.add_to_playlist", "В плейлист"},
            {"library.missing", "Отсутствуют: {count}"},
            {"library.remove_missing", "Удалить отсутствующие"},
            {"library.remove_confirm", "Нажмите ещё раз, чтобы удалить: {count}"},
            {"library.scanning", "сканирование, {files}"},
            {"library.saving_tags", "сохранение тегов"},
            {"library.error.in_use", "трек загружен в дек, его теги не меняются, пока он играет"},
            {"library.error.year", "год должен быть числом не более чем из четырёх цифр"},
            {"playlist.name_placeholder", "Название плейлиста"},
            {"playlist.default_name", "Плейлист {number}"},
            {"playlist.new", "Новый"},
            {"playlist.delete", "Удалить"},
            {"playlist.delete_confirm", "Точно удалить?"},
            {"playlist.import", "Импорт"},
            {"playlist.export", "Экспорт"},
            {"playlist.up", "Вверх"},
            {"playlist.down", "Вниз"},
            {"playlist.remove", "Убрать"},
            {"playlist.none", "нет плейлиста"},
            {"playlist.saving", "сохранение"},
            {"playlist.error.create_first", "сначала создайте плейлист"},
            {"playlist.imported", "импортирован {name}: {tracks}, не найдено: {not_found}"},
            {"deck.cannot_load", "Не удалось загрузить: {error}"},
        };
    } // namespace

    std::span<const CatalogEntry> russian()
    {
        return entries;
    }
} // namespace djw::player_ui::sdl::catalogs
