# DJ-Workstation (djw): Feature Specification

Rewrite of the original DJ-Workstation, developed from 1998 to 2002. The goal is lean, high-performance DJ software for touchscreens and controllers, cross-platform, without any cloud.

License: **AGPLv3**

---

## 1. Principles

- **Lean over complete.** Not a clone of VirtualDJ, Serato or Traktor. Every feature has to earn its place.
- **Performance:** fast startup, low latency, runs smoothly on a Raspberry Pi 4/5.
- **Touch first:** multitouch is the primary input method, mouse and keyboard work just as well.
- **Offline:** no cloud, no accounts, no telemetry, no streaming.
- **Core / UI separation:** the core runs headless and is testable without a UI. The UI talks to the core exclusively through commands and state snapshots.
- **Extensible through data, not code:** controller mappings, layouts and languages are files.

### Non-goals

- Streaming services, cloud sync, accounts
- Video, visuals, karaoke
- Sampler, stem separation, AI features
- Large effect racks
- Telemetry of any kind

---

## 2. Platforms

| Platform | Audio | MIDI/HID | UI | Status |
|---|---|---|---|---|
| Linux x86_64 | ALSA, PipeWire, PulseAudio, JACK | ALSA MIDI, hidraw | SDL3 (X11/Wayland) | Primary |
| Raspberry Pi 4/5 (arm64) | ALSA, PipeWire | ALSA MIDI | SDL3, also KMSDRM without desktop (kiosk) | Primary |
| Windows 10/11 | WASAPI (exclusive/shared), optional ASIO | WinMM MIDI, HID | SDL3 | Primary |
| macOS | CoreAudio | CoreMIDI, IOKit HID | SDL3 | Primary |
| Android | AAudio (via miniaudio) | NDK AMidi (API 29+), USB host | SDL3 | Secondary |
| iOS/iPadOS | CoreAudio | CoreMIDI | SDL3 | Secondary, see license note |
| Web (WebAssembly) | Web Audio, AudioWorklet | Web MIDI, WebHID | SDL3 Emscripten | Experimental |

### Platform notes

**Raspberry Pi**
- Target hardware: Pi 4 (4 GB) and Pi 5 with the official touch display or an HDMI touchscreen.
- Kiosk mode via SDL3 KMSDRM directly on the console, without X11/Wayland.
- Enable NEON optimizations in SoundTouch.

**Android**
- RtMidi has no Android backend. A custom MIDI transport based on the NDK AMidi API is required.
- Class-compliant USB audio interfaces via AAudio, multichannel support depends on the device.
- File access via the Storage Access Framework.

**iOS/iPadOS**
- RtMidi supports CoreMIDI, miniaudio supports CoreAudio.
- **License:** GPL/AGPL code is incompatible with the App Store terms. The copyright holder can additionally license their own code under different terms, but third-party GPL code (e.g. the xwax timecoder) cannot be relicensed without the authors' consent. Distribution therefore initially via sideloading or alternative marketplaces, or an iOS build without the DVS module.

**Web (WebAssembly)**
- Built with Emscripten. Core, miniaudio, SDL3, SQLite and SoundTouch compile cleanly.
- Audio via AudioWorklet. Threads require SharedArrayBuffer, so the server must send `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`.
- Library: SQLite WASM with OPFS as storage. Music files via the File System Access API (Chromium) or drag and drop / file picker.
- MIDI: Web MIDI in Chromium and Firefox, not in Safari. HID only in Chromium (WebHID).
- Output device selection via `setSinkId` (Chromium). Headphone cue only with a 4-channel device, otherwise not available.
- DVS via `getUserMedia` with echo cancellation, AGC and noise suppression disabled. Multichannel input depends on the browser, so DVS on the web is experimental only.
- Latency higher than native (typically 10 to 30 ms).
- AGPL: the delivered WASM/JS code counts as distribution, a source code link in the app is mandatory.

---

## 3. Architecture

```
djw/
  src/
    core/               libdjw-core (C++23, no UI dependency)
      engine/           decks, buses, mixer, resampler, keylock
      transport/        per-deck transport sources: internal | jog | timecode
      decode/           decoder interface and implementations
      analysis/         BPM, beatgrid, waveform, loudness (worker threads)
      library/          SQLite, scanner, tags, playlists
      control/          MIDI/HID transport, mapping (TOML + Lua), feedback
      automix/          autoplay, autofade, queue
      api/              command definitions, state snapshots
      audio_io/         audio devices (miniaudio) behind an interface
      runtime/          implements the core API on top of engine, decode and audio_io
    player-ui/          frontends on top of the core
      sdl/              SDL3 player (custom widget layer)
    platform/           platform-specific code (Android, iOS, Web)
  mappings/             controller mappings (TOML + Lua)
  i18n/                 language files
  tests/                unit and offline render tests
```

### Threading

| Thread | Purpose | Rules |
|---|---|---|
| Audio | mixing, resampling, keylock, DVS decoding | no locks, no allocations, no I/O, no syscalls |
| Decoder (per deck) | decode file, fill ring buffer | may block |
| Library | folder scan, dead link check, reading and writing tags, playlist changes and M3U files, database writes | may block; own SQLite connection, the UI searches on its own |
| Analysis (pool) | BPM, waveform, loudness | low priority |
| Control | read MIDI/HID, mapping, Lua | never on the audio thread |
| UI | rendering, touch events | reads state snapshots only |
| Library | SQLite, scanner | own thread |

### Core API

- Commands go in through a lock-free queue (SPSC/MPSC), e.g. `DeckPlay{deck}`, `DeckSetPitch{deck, value}`, `MixerSetCrossfader{value}`.
- State comes out as a snapshot (triple buffer): position, pitch, BPM, VU, play state per deck, mixer state.
- The API is designed to be serializable. A later remote frontend (tablet on the LAN, OSC) is then just an adapter.
- Optional: stable C ABI on top of the C++ core for bindings.

---

## 4. Features

Priority: **M** = MVP, **1** = version 1.0, **L** = later/optional

### 4.1 Decks

| Feature | Prio | Description |
|---|---|---|
| Variable number of decks | M | No fixed upper limit, add/remove at runtime. The practical limit is the CPU. The control thread builds a new deck list and publishes it via an atomic pointer swap, the old list is freed outside the audio thread. |
| Loading | M | Via drag and drop from the file manager onto a player window, double tap in library, load button, controller. Smart load: into a free deck. |
| Play/pause | M | |
| Cue | M | Classic CDJ behavior: when stopped sets cue, while playing jumps back and stops, when held plays from cue. |
| Stored cue point | M | Cue point per track in the database. "Cue load" option: track starts at the stored point when loaded. |
| Time display | M | Toggle elapsed/remaining. |
| Pitch | M | Fader with range ±12 %, display in percent. Formant correction comes later. |
| Pitch bend | M | +/- buttons for temporary nudging. |
| Pitch zero | M | Reset fader to 0 with a double tap on the pitch fader. |
| Keylock (master tempo) | M | Tempo change without pitch change via SoundTouch. |
| Seek with audible scrub | M | Touching the waveform of a playing deck jumps there at once; on a deck that stands it plays a short loop where the finger is, which runs on until CUE sets the cue point there or play starts the deck from there. |
| Waveform overview | M | Whole track, click/touch to jump. |
| Scrolling waveform | 1 | Zoomable, with beatgrid. |
| BPM display | M | From the analysis, with the pitch taken into account. |
| Beatgrid | 1 | From analysis, manually adjustable. |
| Sync | 1 | Match tempo and phase to the master deck. |
| Hot cues | 1 | 4 to 8 per track, stored in the database. |
| Loops | 1 | Auto loop (1/4 to 32 beats), loop in/out, halve/double. |
| Jog/scratch | 1 | Via controller or touch, varispeed path, reverse playback possible. |
| Gain/autogain | 1 | Manual, optionally automatic from loudness analysis. |
| Slip mode | L | |
| Key detection | L | |

### 4.2 Mixer

| Feature | Prio | Description |
|---|---|---|
| Channel volume | M | Per deck. |
| VU meters | M | Per deck in the groove of the channel fader (level before the fader, the grip narrower and see-through over it), the master above the crossfader. LED segments from -40 dBFS, yellow from -12, red from -3, a clip lamp lit 2 s after a peak at full scale. |
| Crossfader | M | Decks freely assignable to side A, B or none. |
| Crossfader curve | 1 | Linear, constant power, cut. |
| Crossfader zero | M | Fader to center position. |
| Momentary A/B | M | From the original version: tap and hold jumps to one side and back on release. |
| Headphone cue | M | Per deck, headphone volume. |
| Master limiter | M | Prevents clipping. |
| 3-band EQ with kill | 1 | |
| Filter | 1 | One knob per deck: lowpass to the left, highpass to the right. |
| Effects | L | At most echo and reverb, no effect racks. |

### 4.3 Audio routing

| Mode | Prio | Description |
|---|---|---|
| One device, stereo | M | Master only, no pre-listening. |
| One device, 4 channels | M | Master on 1/2, headphones on 3/4 (e.g. Hercules DJControl Starlight). Output device and channels in Settings > Devices, from the next start. |
| Master/headphones on two devices | 1 | With drift compensation via resampling. |
| One output per deck | 1 | External mixer mode from the original version: every deck on its own channels, internal mixer bypassed. |
| DVS inputs | 1 | One input channel pair per deck, see 4.6. |
| Latency | M | Settings > Devices: buffer size from shortest to safe (128 to 1024 frames, 2.7 to 21 ms at 48 kHz), takes effect at the next start. The dialog is where driver and device are chosen later. |
| Sample rate | L | The device's rate is used, tracks are converted when loaded; float32 internally. A choice only if a device needs it. |
| Live switching | 1 | Change devices without restart (the original version required a restart). |

### 4.4 Automix and playlist

| Feature | Prio | Description |
|---|---|---|
| Playlist/queue | M | Create, reorder (up/down), delete entries. |
| Fade now | M | Immediate transition with the configured fade time. |
| Silence detection | 1 | Start/end from analysis instead of file boundaries. |
| Beatmatched automix | L | Transition with sync on the beatgrid. |

### 4.5 Library

| Feature | Prio | Description |
|---|---|---|
| Folder scan | M | Scan selected folders recursively, read tags. |
| Incremental rescan | M | Only changed files (mtime, size). |
| Dead link check | M | From the original version: find missing files and mark/remove them. |
| File watching | L | inotify / FSEvents / ReadDirectoryChangesW. |
| Formats | M | MP3, Ogg Vorbis, FLAC, WAV. |
| Additional formats | 1 | AIFF, Opus. AAC/M4A optional via FFmpeg. |
| Fast search | M | Search as you type across all fields (SQLite FTS5). |
| Column filters | M | From the original version: separate search fields for artist, title, genre and year directly above the columns. |
| Wildcard search | M | `*` and `?` in search fields. |
| Sorting | M | By any column. |
| Columns | M | Artist, title, genre, year, duration, BPM. |
| Tag editor | M | Table-based, writes to file and database (TagLib): ID3v2, Vorbis comments, FLAC. |
| Missing tags | M | Derive artist/title from the file name. |
| Saved playlists | M | In the database, import/export M3U/M3U8. |
| Crates | 1 | Manual collections. |
| Analysis on import | 1 | BPM, beatgrid, waveform, loudness in the background, results cached. |
| Track list export | 1 | Replaces the old print dialog: export as PDF, CSV or HTML, sortable by artist, title, genre, year. |
| Play history | 1 | Per session, exportable. |

### 4.6 DVS (timecode vinyl)

| Feature | Prio | Description |
|---|---|---|
| Serato Control Vinyl | 1 | CV02: `serato_2a`, `serato_2b`, `serato_cd`. Decoder from xwax (GPL-2+). |
| Additional timecodes | L | Traktor, MixVibes (also supported by the xwax decoder). |
| Absolute mode | 1 | Needle position = track position. |
| Relative mode | 1 | Speed and direction only, needle drops ignored. |
| Internal mode | 1 | Deck keeps playing without vinyl (e.g. after a needle skip). |
| Signal display | 1 | Scope and quality indicator per deck. |
| Calibration | 1 | Input level, phono/line. |
| Varispeed path | 1 | Playback at any speed including reverse and standstill. Keylock only outside of scratch movements. |

### 4.7 Controllers

| Feature | Prio | Description |
|---|---|---|
| MIDI | M | RtMidi on desktop, AMidi on Android, Web MIDI in the browser. |
| HID | L | hidapi, for controllers without MIDI. |
| Hotplug | 1 | Connect and disconnect controllers while running. |
| Declarative mapping | M | TOML: note/CC → action (play, cue, load, keylock, headphones, pitch bend, pitch, volume, headphone level, seek, crossfader, library browse), type (button, absolute, 14-bit, relative), deck index, shift layer, invert. In `mappings/` next to the program and in the user's folder; Settings > Devices assigns them to the MIDI ports. |
| Script mapping | 1 | Lua for jog acceleration, scratching, device-specific behavior. Runs on the control thread. |
| LED feedback | M | Player state → MIDI out, in the same mapping file (`[[light]]`): play while playing, headphone cue, CUE as on a CDJ (lit at the cue point and while held, blinking when paused elsewhere). The Mixxx import takes the play, cue and headphone outputs over. |
| MIDI learn | 1 | In the UI, writes the mapping file. |
| Mixxx import | M | Settings > Devices turns a Mixxx mapping (.midi.xml) into ours: its direct bindings to functions djw has; what its script does is left out and counted. Saved in the user's mapping folder. |
| Reference controller | M | Hercules DJControl Starlight (class compliant, 4-channel audio: master 1/2, headphones 3/4, touch-sensitive jog wheels, RGB LEDs). Recognized by name; its sound card is used automatically with the headphones on 3/4. Shift + Play loads, as it has no load buttons. |
| Keyboard shortcuts | M | From the original version, in the player that has the keyboard focus: P = play/pause, C = cue (acts while held), L = load. |

A mapping looks like this:
```toml
[device]
name = "Hercules DJControl Starlight"
match = ["DJControl Starlight"]
script = "hercules_starlight.lua"   # optional

[[control]]
midi = { type = "note", channel = 1, number = 0x07 }
action = "deck.play"
deck = 1
mode = "button"
feedback = { type = "note", channel = 1, number = 0x07, on = 0x7F, off = 0x00 }
```

### 4.8 User interface

| Feature | Prio | Description |
|---|---|---|
| Multitouch | M | Several fingers at once, each finger captures a widget on touch down. |
| Custom widget layer | M | Fader, button, waveform, list with kinetic scrolling (it runs on after a drag and slows down), text field. Knob and jog wheel come with M2. |
| Player windows | M | Core idea of the original version: every player unit is its own OS window, any number can be opened and closed, freely movable and resizable, also across several monitors. On platforms with only one window (KMSDRM kiosk, Android, iOS, Web) the player units are arranged inside the main window. |
| Saved layouts | M | Save and load position and size of all player windows. |
| HiDPI | M | Resolution-independent scaling. |
| Fullscreen/kiosk | M | Borderless, for touch terminals. |
| On-screen keyboard | M | For search on touch-only devices (SDL3 text input). |
| Themes | 1 | Dark/light, colors defined in a file. |
| Multilingual | M | Language files, starting with German, English and French (like the original version). |
| Always on top | L | Desktop option from the original version. |

Layout reference: the original version, see [djw.gif](https://dj-workstation.de/bilder/djw.gif). The new UI keeps its structure, adapted for touch:
- Player window: load button, pitch bend -/+, vertical pitch fader, large time display with elapsed/remaining toggle, pitch in percent and pitch zero, position slider, state (loaded/cued/playing), large CUE and play/pause buttons, channel volume and headphone level faders with headphone cue button, artist and title, Cue-Load option (a loaded track starts at the cue point stored for it, else at its first audible sound, else at the beginning; the last cue point the user set is stored per track in the library database).
- Crossfader window: deck selection for side A and B, zero, momentary A/B buttons, fade now, autoplay start before the end (s), fade time (s), autoplay and autofade switches.
- Playlist window: numbered list, next track highlighted.
- Library window: search fields directly above the columns (artist, title, genre, year), sortable list.

### 4.9 Recording

| Feature | Prio | Description |
|---|---|---|
| Record mix | L | Master to WAV or FLAC, with track list (cue sheet). |

### 4.10 Remote

| Feature | Prio | Description |
|---|---|---|
| Remote frontend | L | Command/state API over a local socket or OSC, e.g. a tablet as a second control surface. LAN only, no cloud. Source code notice required by AGPL §13. |

---

## 5. Performance targets

| Metric | Target |
|---|---|
| Startup until usable | < 1 s (desktop), < 2 s (Pi 5) |
| Native audio latency | ≤ 6 ms at 256 samples / 48 kHz |
| Decks on Pi 4 | 4 decks with keylock without dropouts |
| UI | 60 fps on Pi 5, GPU-accelerated waveform rendering |
| Library search | < 50 ms with 100,000 tracks |
| Binary (desktop, static, without FFmpeg) | single-digit MB |
| RAM | No target. Decoded tracks up to the configured length are held in memory (about 23 MB per minute at 48 kHz); this is audio, not video. |

---

## 6. Technology

| Area | Library | License |
|---|---|---|
| Language | C++23 (without modules) | |
| Build | CMake with presets per platform | |
| Audio I/O, decoding MP3/FLAC/WAV | miniaudio | Public domain / MIT-0 |
| Ogg Vorbis | stb_vorbis | Public domain / MIT |
| Opus | libopusfile | BSD |
| AAC/M4A (optional) | FFmpeg | LGPL |
| Keylock/timestretch | SoundTouch | LGPL-2.1 |
| High-quality timestretch (optional) | Rubber Band | GPL-2+ |
| BPM/beat detection | SoundTouch BPMDetect, optional aubio | LGPL / GPL-3+ |
| Database | SQLite (FTS5) | Public domain |
| Tags | TagLib | LGPL-2.1 / MPL-1.1 |
| UI, windowing, touch | SDL3 | zlib |
| Font rendering | SDL3_ttf (FreeType, HarfBuzz) | zlib (FreeType: FTL, HarfBuzz: MIT) |
| Vector graphics (optional) | NanoVG | zlib |
| MIDI | RtMidi (+ custom backends for Android/Web) | MIT-style |
| HID (optional) | hidapi | BSD |
| Scripting | Lua 5.4 | MIT |
| Config | toml++ | MIT |
| DVS | xwax timecoder | GPL-2+ |
| Tests | doctest or Catch2 | MIT / BSL |

---

## 7. Quality and CI

- Core fully testable headless.
- Offline render tests: a defined command sequence renders to WAV, compared against a reference.
- Audio thread checks: debug builds detect allocations and locks in the audio callback.
- GitHub Actions: builds for Linux x86_64, Linux arm64, Windows, macOS, Android, Web.
- Sanitizers (ASan, UBSan, TSan) in CI for the core.

---

## 8. Milestones

| Milestone | Scope |
|---|---|
| M0 Skeleton | CMake, core/UI separation, miniaudio output, one deck plays a file, SDL3 window with fader and button (multitouch capture) |
| M1 MVP | N decks, mixer with crossfader and headphone cue, keylock, library with scan and fast search, tag editor, playlist, desktop; MIDI controllers through TOML mappings with the Starlight as reference, LED feedback, Mixxx import |
| M2 Controllers | Lua scripting, MIDI learn, hotplug, jog/scratch with on-screen knob and jog wheel |
| M3 DVS | xwax timecoder, duplex audio, absolute/relative/internal modes, signal display |
| M4 Analysis | Beatgrid, scrolling waveform, sync, hot cues, loops, EQ, filter |
| M5 Mobile | Android, iOS |
| M6 Web | WebAssembly build |
| Later (not scheduled) | Autoplay/autofade with the crossfader window, Raspberry Pi build and its performance targets |
