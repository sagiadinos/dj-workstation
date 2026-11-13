---
title: "Télécharger"
description: "Télécharger les dernières versions de développement de DJ-Workstation pour Linux, Raspberry Pi, Windows et macOS."
layout: download

intro: "Versions de développement, construites automatiquement à partir du code source actuel. DJ-Workstation est en début de développement et pas encore utilisable en production."

builds:
  - system: "Linux"
    arch: "x86 64 bits"
    file: latest_linux_x64.AppImage
    hint: "Rendre le fichier exécutable et le lancer."
  - system: "Linux ARM"
    arch: "ARM 64 bits, Raspberry Pi 4 et 5"
    file: latest_linux_aarch64.AppImage
    hint: "Rendre le fichier exécutable et le lancer."
  - system: "Windows"
    arch: "x86 64 bits, Windows 10 et 11"
    file: latest_windows_x64.zip
    hint: "Décompresser l’archive et lancer djw.exe. Aucune installation nécessaire. Si Windows SmartScreen affiche un avertissement, choisir « Informations complémentaires » puis « Exécuter quand même »."
  - system: "macOS"
    arch: "Apple Silicon"
    file: latest_macos_arm64.dmg
    hint: "Ouvrir l’image disque et glisser l’application dans Applications. Si macOS bloque l’application, l’autoriser dans Réglages Système → Confidentialité et sécurité."

source:
  title: "Code source"
  text: "Le code source est sur GitHub."
---
