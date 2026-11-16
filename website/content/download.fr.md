---
title: "Télécharger"
description: "Télécharger les dernières versions de développement de DJ-Workstation pour Linux, Raspberry Pi, Windows et macOS."
layout: download

intro: "Construites automatiquement à partir du code source actuel. Voici la première version utilisable de DJ-Workstation ; vos retours sont les bienvenus."

builds:
  - system: "Linux"
    arch: "x86 64 bits"
    file: latest_linux_x86_64.AppImage
    hint: "Nécessite glibc 2.41 ou plus récent, par ex. Debian 13, Ubuntu 25.04 ou Fedora 42. Rendre le fichier exécutable et le lancer."
  - system: "Linux ARM"
    arch: "ARM 64 bits, Raspberry Pi 4 et 5"
    file: latest_linux_aarch64.AppImage
    hint: "Nécessite Raspberry Pi OS ou Debian 13 (Trixie) ou plus récent. Rendre le fichier exécutable et le lancer."
  - system: "Windows"
    arch: "x86 64 bits, Windows 10 et 11"
    file: latest_windows.exe
    hint: "Lancer le programme d’installation et suivre les étapes. Si Windows SmartScreen affiche un avertissement, choisir « Informations complémentaires » puis « Exécuter quand même »."
  - system: "macOS"
    arch: "Apple Silicon"
    file: latest_macos.dmg
    hint: "Ouvrir l’image disque et glisser l’application dans Applications. Si macOS bloque l’application, l’autoriser dans Réglages Système → Confidentialité et sécurité."

source:
  title: "Code source"
  text: "Le code source est sur GitHub."
---
