---
title: "Descargar"
description: "Descarga las últimas versiones de desarrollo de DJ-Workstation para Linux, Raspberry Pi, Windows y macOS."
layout: download

intro: "Versiones de desarrollo, compiladas automáticamente a partir del código fuente actual. DJ-Workstation está en fase temprana de desarrollo y todavía no es apto para uso en producción."

builds:
  - system: "Linux"
    arch: "x86 64 bits"
    file: latest_linux_x64.AppImage
    hint: "Haz el archivo ejecutable e inícialo."
  - system: "Linux ARM"
    arch: "ARM 64 bits, Raspberry Pi 4 y 5"
    file: latest_linux_aarch64.AppImage
    hint: "Haz el archivo ejecutable e inícialo."
  - system: "Windows"
    arch: "x86 64 bits, Windows 10 y 11"
    file: latest_windows_x64.zip
    hint: "Descomprime el archivo y ejecuta djw.exe. No requiere instalación. Si Windows SmartScreen muestra una advertencia, elige «Más información» y «Ejecutar de todas formas»."
  - system: "macOS"
    arch: "Apple Silicon"
    file: latest_macos_arm64.dmg
    hint: "Abre la imagen de disco y arrastra la app a Aplicaciones. Si macOS bloquea la app, permítela en Ajustes del Sistema → Privacidad y seguridad."

source:
  title: "Código fuente"
  text: "El código fuente está en GitHub."
---
