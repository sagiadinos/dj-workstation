---
title: Download
description: Download the latest development builds of DJ-Workstation for Linux, Raspberry Pi, Windows and macOS.
layout: download

intro: "Built automatically from the current source code. This is the first usable version of DJ-Workstation; feedback is welcome."

builds:
  - system: Linux
    arch: x86 64-bit
    file: latest_linux_x86_64.AppImage
    hint: "Needs glibc 2.41 or newer, e.g. Debian 13, Ubuntu 25.04 or Fedora 42. Make the file executable and start it."
  - system: Linux ARM
    arch: ARM 64-bit, Raspberry Pi 4 and 5
    file: latest_linux_aarch64.AppImage
    hint: "Needs Raspberry Pi OS or Debian 13 (Trixie) or newer. Make the file executable and start it."
  - system: Windows
    arch: x86 64-bit, Windows 10 and 11
    file: latest_windows.exe
    hint: Start the installer and follow the steps. If Windows SmartScreen warns, choose “More info” and “Run anyway”.
  - system: macOS
    arch: Apple silicon
    file: latest_macos.dmg
    hint: Open the disk image and drag the app into Applications. If macOS blocks the app, allow it under System Settings → Privacy & Security.

source:
  title: Source code
  text: The source code is on GitHub.
---
