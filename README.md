# DJ-Workstation

[![CI](https://github.com/sagiadinos/dj-workstation/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/sagiadinos/dj-workstation/actions/workflows/ci.yml)
![coverage](https://raw.githubusercontent.com/sagiadinos/dj-workstation/badges/coverage.svg)
[![mutation score](https://raw.githubusercontent.com/sagiadinos/dj-workstation/badges/mutation.svg)](https://github.com/sagiadinos/dj-workstation/actions/workflows/mutation.yml)

Lean, touch-first DJ software for touchscreens and MIDI controllers. Runs on Linux, Raspberry Pi, Windows and macOS. No cloud, no accounts, no telemetry.

## What it is

- Any number of players, each in its own window or all arranged in one window, for mouse and touchscreen
- Mixer with crossfader, headphone cue, keylock and VU meters
- Music library with fast search, column filters, tag editor, playlists and tempo analysis
- MIDI controllers with simple TOML mappings, LED feedback and an import for Mixxx mappings
- In English, German, French, Spanish, Greek and Russian
- Designed to run smoothly on a Raspberry Pi

Planned, among others: timecode vinyl (DVS), automix, Android, iOS and a web build.

What it is not: a streaming client, a video mixer or a feature-packed all-in-one suite. The goal is to stay small and fast.

The full feature list, with what is done and what is planned, is in [FEATURES.md](FEATURES.md).

## History

The original DJ-Workstation was developed from 1998 to 2002 as a Win32 application for Windows 9x, with resizable player units designed for touchscreen monitors from day one. It supported MP3, Ogg Vorbis and WAV, multiple sound cards for pre-listening and automix.

The original version and its history are still available at [dj-workstation.de](https://dj-workstation.de).

This repository is a complete rewrite with a modern, cross-platform architecture. No code from the original is reused.

## Building

Requirements: CMake 3.25 or newer, Ninja, a C++23 compiler (GCC 13, Clang 17, Apple Clang 15 or MSVC 19.38 at least). On Linux also the ALSA development files (`libasound2-dev` on Debian/Ubuntu) and those SDL3 needs for X11 or Wayland. All other libraries (SDL3, SQLite, TagLib, SoundTouch, RtMidi, ...) are taken from the system if installed, otherwise downloaded and built in pinned versions.

```bash
cmake --preset linux-debug          # or linux-release, windows-debug, macos-debug, ...
cmake --build --preset linux-debug
ctest --preset linux-debug          # the tests
```

`scripts/build-linux.sh` builds the Linux release binary in `build/linux-release/src/player-ui/sdl/djw`.

Starting it:

```
djw [--version] [--headphones] [--add-folder <folder>]... [<audio file>]
  --version             print the version and stop
  --headphones          master on output channels 1/2, headphones on 3/4
                        (needs a 4-channel audio device)
  --add-folder <folder> add a music folder to the library and scan it
```

More on the build options and the tests in [Architecture.md](Architecture.md).

## License

[AGPL-3.0-or-later](LICENSE)

Third-party components keep their own licenses, see FEATURES.md section 6.
