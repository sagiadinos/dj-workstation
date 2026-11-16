---
title: Download
description: Die neuesten Entwicklungsversionen von DJ-Workstation für Linux, Raspberry Pi, Windows und macOS herunterladen.
layout: download

intro: "Automatisch aus dem aktuellen Quellcode gebaut. Dies ist die erste nutzbare Version von DJ-Workstation; Feedback ist willkommen."

builds:
  - system: Linux
    arch: x86 64 Bit
    file: latest_linux_x86_64.AppImage
    hint: "Braucht glibc 2.41 oder neuer, z. B. Debian 13, Ubuntu 25.04 oder Fedora 42. Datei ausführbar machen und starten."
  - system: Linux ARM
    arch: ARM 64 Bit, Raspberry Pi 4 und 5
    file: latest_linux_aarch64.AppImage
    hint: "Braucht Raspberry Pi OS oder Debian 13 (Trixie) oder neuer. Datei ausführbar machen und starten."
  - system: Windows
    arch: x86 64 Bit, Windows 10 und 11
    file: latest_windows.exe
    hint: Installer starten und den Schritten folgen. Falls Windows SmartScreen warnt, „Weitere Informationen“ und „Trotzdem ausführen“ wählen.
  - system: macOS
    arch: Apple Silicon
    file: latest_macos.dmg
    hint: Disk-Image öffnen und die App in den Programme-Ordner ziehen. Falls macOS die App blockiert, unter Systemeinstellungen → Datenschutz & Sicherheit erlauben.

source:
  title: Quellcode
  text: Der Quellcode liegt auf GitHub.
---
