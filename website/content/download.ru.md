---
title: "Скачать"
description: "Скачайте последние сборки DJ-Workstation для Linux, Raspberry Pi, Windows и macOS."
layout: download

intro: "Сборки для разработки, автоматически создаваемые из текущего исходного кода. DJ-Workstation находится на ранней стадии разработки и пока не готова к рабочему использованию."

builds:
  - system: "Linux"
    arch: "x86 64 бит"
    file: latest_linux_x64.AppImage
    hint: "Сделайте файл исполняемым и запустите его."
  - system: "Linux ARM"
    arch: "ARM 64 бит, Raspberry Pi 4 и 5"
    file: latest_linux_aarch64.AppImage
    hint: "Сделайте файл исполняемым и запустите его."
  - system: "Windows"
    arch: "x86 64 бит, Windows 10 и 11"
    file: latest_windows_x64.zip
    hint: "Распакуйте архив и запустите djw.exe. Установка не требуется. Если Windows SmartScreen выдаёт предупреждение, выберите «Подробнее» и «Выполнить в любом случае»."
  - system: "macOS"
    arch: "Apple Silicon"
    file: latest_macos_arm64.dmg
    hint: "Откройте образ диска и перетащите приложение в «Программы». Если macOS блокирует приложение, разрешите его в «Системных настройках» → «Конфиденциальность и безопасность»."

source:
  title: "Исходный код"
  text: "Исходный код находится на GitHub."
---
