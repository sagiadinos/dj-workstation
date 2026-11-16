---
title: "Descargar"
description: "Descarga las últimas versiones de desarrollo de DJ-Workstation para Linux, Raspberry Pi, Windows y macOS."
layout: download

intro: "Compiladas automáticamente a partir del código fuente actual. Esta es la primera versión utilizable de DJ-Workstation; se agradecen comentarios."

builds:
  - system: "Linux"
    arch: "x86 64 bits"
    file: latest_linux_x86_64.AppImage
    hint: "Necesita glibc 2.41 o posterior, p. ej. Debian 13, Ubuntu 25.04 o Fedora 42. Haz el archivo ejecutable e inícialo."
  - system: "Linux ARM"
    arch: "ARM 64 bits, Raspberry Pi 4 y 5"
    file: latest_linux_aarch64.AppImage
    hint: "Necesita Raspberry Pi OS o Debian 13 (Trixie) o posterior. Haz el archivo ejecutable e inícialo."
  - system: "Windows"
    arch: "x86 64 bits, Windows 10 y 11"
    file: latest_windows.exe
    hint: "Ejecuta el instalador y sigue los pasos. Si Windows SmartScreen muestra una advertencia, elige «Más información» y «Ejecutar de todas formas»."
  - system: "macOS"
    arch: "Apple Silicon"
    file: latest_macos.dmg
    hint: "Abre la imagen de disco y arrastra la app a Aplicaciones. Si macOS bloquea la app, permítela en Ajustes del Sistema → Privacidad y seguridad."

source:
  title: "Código fuente"
  text: "El código fuente está en GitHub."
---
