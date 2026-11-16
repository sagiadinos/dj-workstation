---
title: "Скачать"
description: "Скачайте последние сборки DJ-Workstation для Linux, Raspberry Pi, Windows и macOS."
layout: download

intro: "Собираются автоматически из текущего исходного кода. Это первая рабочая версия DJ-Workstation; отзывы приветствуются."

builds:
  - system: "Linux"
    arch: "x86 64 бит"
    file: latest_linux_x86_64.AppImage
    hint: "Нужна glibc 2.41 или новее, например Debian 13, Ubuntu 25.04 или Fedora 42. Сделайте файл исполняемым и запустите его."
  - system: "Linux ARM"
    arch: "ARM 64 бит, Raspberry Pi 4 и 5"
    file: latest_linux_aarch64.AppImage
    hint: "Нужна Raspberry Pi OS или Debian 13 (Trixie) или новее. Сделайте файл исполняемым и запустите его."
  - system: "Windows"
    arch: "x86 64 бит, Windows 10 и 11"
    file: latest_windows.exe
    hint: "Запустите установщик и следуйте инструкциям. Если Windows SmartScreen выдаёт предупреждение, выберите «Подробнее» и «Выполнить в любом случае»."
  - system: "macOS"
    arch: "Apple Silicon"
    file: latest_macos.dmg
    hint: "Откройте образ диска и перетащите приложение в «Программы». Если macOS блокирует приложение, разрешите его в «Системных настройках» → «Конфиденциальность и безопасность»."

source:
  title: "Исходный код"
  text: "Исходный код находится на GitHub."
---
