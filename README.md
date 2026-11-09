# DJ-Workstation

Lean, touch-first DJ software with controller and timecode vinyl support. Runs on Linux, Raspberry Pi, Windows and macOS, with Android, iOS and a web build planned. No cloud, no accounts, no telemetry.

> **Status: early development, not usable yet.**

## What it is

- Any number of decks, each in its own player window, freely movable and resizable for touchscreen use
- Mixer with crossfader, headphone cue, keylock
- Music library with fast search, tag editor, playlists and automix
- MIDI controllers with simple TOML mappings and optional Lua scripts
- DVS with Serato Control Vinyl
- Designed to run smoothly on a Raspberry Pi

What it is not: a streaming client, a video mixer or a feature-packed all-in-one suite. The goal is to stay small and fast.

The full feature list and roadmap are in [FEATURES.md](FEATURES.md).

## History

The original DJ-Workstation was developed from 1998 to 2002 as a Win32 application for Windows 9x, with resizable player units designed for touchscreen monitors from day one. It supported MP3, Ogg Vorbis and WAV, multiple sound cards for pre-listening and automix.

The original version and its history are still available at [dj-workstation.de](https://dj-workstation.de).

This repository is a complete rewrite with a modern, cross-platform architecture. No code from the original is reused.

## Building

Not available yet. Build instructions will follow with the first milestone.

## License

[AGPL-3.0-or-later](LICENSE)

Third-party components keep their own licenses, see FEATURES.md section 6.
