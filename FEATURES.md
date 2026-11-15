# DJ-Workstation (djw): Feature Specification

Rewrite of the original DJ-Workstation, developed from 1998 to 2002. The goal is lean, high-performance DJ software for touchscreens and controllers, cross-platform, without any cloud.

License: **AGPLv3**

---

## 1. Principles

- **Lean over complete.** Not a clone of VirtualDJ, Serato or Traktor. Every feature has to earn its place.
- **Performance:** fast startup, low latency, runs smoothly on a Raspberry Pi 4/5.
- **Touch first:** multitouch is the primary input method, mouse and keyboard work just as well.
- **Offline:** no cloud, no accounts, no telemetry, no streaming services.
- **Core / UI separation:** the core runs headless and is testable without a UI. The UI talks to the core exclusively through commands and state snapshots.
- **Extensible through data, not code:** controller mappings, layouts and languages are files.

### Non-goals

- Streaming services, cloud sync, accounts
- Video, visuals, karaoke
- Sampler, stem separation, AI features
- Effects and filters, loops, key detection, beatmatched automix
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
- Enable NEON optimizations in SoundTouch (the Standard keylock quality).

**Android**
- RtMidi has no Android backend. A custom MIDI transport based on the NDK AMidi API is required.
- Class-compliant USB audio interfaces via AAudio, multichannel support depends on the device.
- File access via the Storage Access Framework.

**iOS/iPadOS**
- RtMidi supports CoreMIDI, miniaudio supports CoreAudio.
- **License:** GPL/AGPL code is incompatible with the App Store terms. The copyright holder can additionally license their own code under different terms, but third-party GPL code (QM-DSP for the BPM analysis, later the xwax timecoder) cannot be relicensed without the authors' consent. Distribution therefore initially via sideloading or alternative marketplaces, or an iOS build without those modules (no BPM analysis, no DVS).

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
      api/              the core as the frontends see it: commands, state snapshots, library and controller interfaces
      engine/           decks, buses, mixer, resampler, keylock
      decode/           decoders, tracks in memory or streamed
      analysis/         BPM, overview waveform (on the library's analysis thread)
      library/          SQLite, scanner, tags, playlists
      control/          MIDI, mappings (TOML), Mixxx import, light feedback
      audio_io/         audio devices (miniaudio) behind an interface
      runtime/          implements the core API on top of engine, decode and audio_io
    player-ui/          frontends on top of the core
      sdl/              SDL3 player (custom widget layer), languages in i18n/ as built-in catalogs
  mappings/             controller mappings (TOML)
  tests/                unit, integration and offline render tests
  website/              the project website (Hugo)
  misc/, tools/         logo, icons, packaging, the mutation test script
```

### Threading

| Thread | Purpose | Rules |
|---|---|---|
| Audio | mixing, resampling, keylock, DVS decoding | no locks, no allocations, no I/O, no syscalls |
| Decoder (per deck) | decode file, fill ring buffer | may block |
| Library | folder scan, dead link check, reading and writing tags, playlist changes and M3U files, database writes | may block; own SQLite connection, the UI searches on its own |
| Analysis (pool) | BPM, waveform, loudness | low priority |
| Control | read MIDI/HID, mapping | never on the audio thread |
| UI | rendering, touch events | reads state snapshots only |
| Library | SQLite, scanner | own thread |

### Core API

- Commands go in through a lock-free queue (SPSC/MPSC), e.g. `DeckPlay{deck}`, `DeckSetPitch{deck, value}`, `MixerSetCrossfader{value}`.
- State comes out as a snapshot (triple buffer): position, pitch, BPM, VU, play state per deck, mixer state.
- The API is designed to be serializable. A later remote frontend (tablet on the LAN, OSC) is then just an adapter.
- Optional: stable C ABI on top of the C++ core for bindings.

---

## 4. Features

Status: **done** = in the program, **planned** = not yet. Which planned feature comes next and what goes into which version is decided case by case; there is no fixed order.

### 4.1 Decks

| Feature | Status | Description |
|---|---|---|
| Variable number of decks | done | No fixed upper limit, add/remove at runtime. The practical limit is the CPU. The control thread builds a new deck list and publishes it via an atomic pointer swap, the old list is freed outside the audio thread. |
| Loading | done | Via drag and drop from the file manager onto a player window, double tap in library, load button, controller. Smart load: into a free deck. |
| Play/pause | done | |
| Cue | done | Classic CDJ behavior: when stopped sets cue, while playing jumps back and stops, when held plays from cue. |
| Stored cue point | done | Cue point per track in the database. "Cue load" option: track starts at the stored point when loaded. |
| Time display | done | Toggle elapsed/remaining. |
| Pitch | done | Fader with range ±10 % in steps of 0.02 % (as a CDJ), down is faster as on a turntable, display in percent with two decimals; a controller's fine steps count fully. Formant correction comes later. |
| Pitch bend | done | +/- buttons for temporary nudging. |
| Pitch zero | done | Reset fader to 0 with a double tap on the pitch fader. |
| Keylock (master tempo) | done | Tempo change without pitch change. Settings > Sound sets the keylock quality (next start): High (Signalsmith Stretch, the default) or Standard (SoundTouch, less computing power), behind one interface (`engine::TimeStretcher`, made by `makeTimeStretcher`). |
| Seek with audible scrub | done | Touching the waveform of a playing deck jumps there at once; on a deck that stands it plays a short loop where the finger is, which runs on until CUE sets the cue point there or play starts the deck from there. |
| Waveform overview | done | Whole track as its loudness (RMS), pulled apart between the quiet and the loud parts of the track, so intro, breakdown and drop show; click/touch to jump. |
| Scrolling waveform | done | Beat waveforms: the players on crossfader sides A and B in a part of their own (View menu), 6 s of real time around a fixed playhead, the pitch taken into account, in three colours (bass red, mids green, highs blue); lanes horizontal or vertical. Zoom and beatgrid are planned. |
| BPM display | done | From the analysis, exact to a few hundredths, with the pitch taken into account; shown with two decimals in the player and the library, so a pitch step of 0.02 % shows. |
| Beatgrid | planned | From analysis, manually adjustable. |
| Sync | planned | Match tempo and phase to the master deck. |
| Hot cues | planned | 4 to 8 per track, stored in the database. |
| Jog wheel | done | Via controller. Turned at the edge (`deck.jog`): while the deck plays it bends the pitch for as long as it turns (beatmatching), while it stands it steps through the track. Touched on top (`deck.scratch_touch`) it scratches: turning the top (`deck.scratch`) moves the track at the hand's speed, also backwards and held still, a turn in 1.8 s being the normal speed (a record at 33 1/3); let go, a deck that played plays on. The steps of one turn are set in the TOML mapping (`ticks_per_turn`); no scripts. Not touched, or a track too long to be held in memory (streamed): the top works like the edge. |
| Gain/autogain | planned | Manual, optionally automatic from loudness analysis. |
| Slip mode | planned | |

### 4.2 Mixer

| Feature | Status | Description |
|---|---|---|
| Channel volume | done | Per deck. |
| VU meters | done | Per deck in the groove of the channel fader (level before the fader, the grip narrower and see-through over it), the master above the crossfader. LED segments from -40 dBFS, yellow from -12, red from -3, a clip lamp lit 2 s after a peak at full scale. |
| Crossfader | done | Decks freely assignable to side A, B or none. |
| Crossfader zero | done | Fader to center position. |
| Momentary A/B | done | From the original version: tap and hold jumps to one side and back on release. |
| Headphone cue | done | Per deck, headphone volume. |
| Master limiter | done | Prevents clipping. |
| Bass with kill | planned | One bass knob per deck (low shelf) and a kill button that takes the bass out entirely, for swapping the bass in a transition; on a controller such as the Hercules DJControl Starlight its bass knob. No mids or highs. |

### 4.3 Audio routing

| Mode | Status | Description |
|---|---|---|
| One device, stereo | done | Master only, no pre-listening. |
| One device, 4 channels | done | Master on 1/2, headphones on 3/4 (e.g. Hercules DJControl Starlight). Output device and channels in Settings > Devices, from the next start. |
| Master/headphones on two devices | planned | With drift compensation via resampling. |
| One output per deck | planned | External mixer mode from the original version: every deck on its own channels, internal mixer bypassed. |
| DVS inputs | planned | One input channel pair per deck, see 4.6. |
| Latency | done | Settings > Devices: buffer size from shortest to safe (128 to 1024 frames, 2.7 to 21 ms at 48 kHz), takes effect at the next start. The dialog is where driver and device are chosen later. |
| Sample rate | planned | The device's rate is used, tracks are converted when loaded; float32 internally. A choice only if a device needs it. |

### 4.4 Automix and playlist

| Feature | Status | Description |
|---|---|---|
| Playlist/queue | done | Create, reorder (up/down), delete entries. |
| Fade now | done | Immediate transition with the configured fade time. |
| Autoplay/autofade | planned | From the original version, in the crossfader window: start the next track a set time before the end and fade over. |
| Silence detection | planned | Start/end from analysis instead of file boundaries. |

### 4.5 Library

| Feature | Status | Description |
|---|---|---|
| Folder scan | done | Scan selected folders recursively, read tags. |
| Incremental rescan | done | Only changed files (mtime, size). |
| Dead link check | done | From the original version: find missing files and mark/remove them. |
| Formats | done | MP3, Ogg Vorbis, FLAC, WAV. |
| Additional formats | planned | AIFF, Opus. |
| Fast search | done | Search as you type across all fields (SQLite FTS5). |
| Column filters | done | From the original version: separate search fields for artist, title, genre and year directly above the columns. |
| Clear search | done | One button empties the search field and all filters. |
| Wildcard search | done | `*` and `?` in search fields. |
| Sorting | done | By any column. |
| Columns | done | Artist, title, genre, year, BPM (two decimals, a little wider), duration. |
| Tag editor | done | Table-based, writes to file and database (TagLib): ID3v2, Vorbis comments, FLAC. |
| Missing tags | done | Derive artist/title from the file name. |
| Saved playlists | done | In the database, import/export M3U/M3U8. |
| Analysis on import | planned | BPM, beatgrid, waveform, loudness in the background, results cached. |
| Track list export | planned | Replaces the old print dialog: export as HTML with a print layout, opened in the browser to print it (or save it as PDF there), or as CSV; sortable by artist, title, genre, year. |
| Play history | planned | Per session, exportable. |

### 4.6 DVS (timecode vinyl)

| Feature | Status | Description |
|---|---|---|
| Serato Control Vinyl | planned | CV02: `serato_2a`, `serato_2b`, `serato_cd`. Decoder from xwax (GPL-2+). |
| Additional timecodes | planned | Traktor, MixVibes (also supported by the xwax decoder). |
| Absolute mode | planned | Needle position = track position. |
| Relative mode | planned | Speed and direction only, needle drops ignored. |
| Internal mode | planned | Deck keeps playing without vinyl (e.g. after a needle skip). |
| Signal display | planned | Scope and quality indicator per deck. |
| Calibration | planned | Input level, phono/line. |
| Varispeed path | done | Scratching plays at any speed from 8 times backwards to 8 times forwards, held still included, following the hand smoothly (about 8 ms), so a jerk does not click. Only for tracks held in memory; keylock does not apply while scratching. |

### 4.7 Controllers

| Feature | Status | Description |
|---|---|---|
| MIDI | done | RtMidi on desktop, AMidi on Android, Web MIDI in the browser. |
| HID | planned | hidapi, for controllers without MIDI. |
| Hotplug | done | Connect and disconnect controllers while running: about once a second the MIDI ports are compared with the last look; a new port gets the mapping assigned to it (or matching it), one that went is closed, and one plugged in again counts as new. The controller's sound card is not taken up again: unplugged, its audio stays gone until the next start. |
| Declarative mapping | done | TOML: note/CC → action (play, cue, load, keylock, headphones, pitch bend, pitch, volume, headphone level, seek, jog, scratch touch, scratch, crossfader, library browse), type (button, absolute, 14-bit, relative), deck index, shift layer, invert, steps per turn of a jog wheel. In `mappings/` next to the program and in the user's folder; Settings > Devices assigns them to the MIDI ports. |
| LED feedback | done | Player state → MIDI out, in the same mapping file (`[[light]]`): play while playing, headphone cue, CUE as on a CDJ (lit at the cue point and while held, blinking when paused elsewhere). The Mixxx import takes the play, cue and headphone outputs over. |
| Mixxx import | done | Settings > Devices turns a Mixxx mapping (.midi.xml) into ours: its direct bindings to functions djw has; what its script does is left out and counted. Saved in the user's mapping folder. |
| Reference controller | done | Hercules DJControl Starlight (class compliant, 4-channel audio: master 1/2, headphones 3/4, touch-sensitive jog wheels, RGB LEDs). Recognized by name; its sound card is used automatically with the headphones on 3/4. Shift + Play loads, as it has no load buttons. Its pitch faders are inverted in the mapping, so pulled down is faster, as on the screen and a turntable. |
| Keyboard shortcuts | done | In the player that has the keyboard focus, always with the left Alt (Option on the Mac), so typing never reaches a player: Alt+P = play/pause, Alt+C = cue (acts while held), Alt+L = load, Alt+←/→ = a step back/ahead (with Shift 10 s), Alt+↑/↓ = pitch ±0.02 % (with Shift ±0.1 %). A letter typed while no text field has the focus goes into the library's search. |

The mapping format is described in [mappings/README.md](mappings/README.md).

### 4.8 User interface

| Feature | Status | Description |
|---|---|---|
| Multitouch | done | Several fingers at once, each finger captures a widget on touch down. |
| Custom widget layer | done | Fader, button, waveform, list with kinetic scrolling (it runs on after a drag and slows down), text field. Knob and jog wheel are planned. |
| Player windows | done | Core idea of the original version: every player unit is its own OS window, any number can be opened and closed, freely movable and resizable, also across several monitors. On platforms with only one window (KMSDRM kiosk, Android, iOS, Web) the player units are arranged inside the main window. |
| One-window mode | done | View > One-window mode (from the next start): players, crossfader, playlist and library as parts of one window, arranged automatically without overlap. The default without a saved layout. |
| Menu bar | done | In the library, in both window modes: File, Playlist, Settings, View, Layout. Titles as wide as their text (at least a finger wide), check marks for what is on, a line above File > Quit. |
| Saved layouts | done | Layout > Save layout keeps the window mode, position and size of all windows and parts, and per player the time display and Cue-Load; the program opens as saved. |
| Edit layout | done | Layout > Edit layout in the one window: drag a part to move it or at its corner to resize it; edges snap to neighbouring parts and the window, else to a 5-point grid. Cursor keys move the selected part by 1 point (Shift: 10), with Ctrl they resize it; Tab selects the next part. Layout > Arrange automatically goes back to the automatic arrangement. |
| HiDPI | done | Resolution-independent scaling. Player, crossfader and playlist scale with their window or part. |
| Fullscreen/kiosk | done | Borderless, for touch terminals (View > Fullscreen, Ctrl+F). Without a saved layout the program starts as a window. |
| Help bubbles | done | A short help for a widget when the mouse rests on it; with touch everything works without it. |
| Safe quit | done | While a track plays, nothing closes that would stop it; File > Quit then asks first, Cancel being the default. |
| On-screen keyboard | done | For search on touch-only devices (SDL3 text input). |
| Daylight colors | done | Two fixed color sets, dark and daylight (light surfaces, dark text, strong contrast for sun or bright rooms), switched with View > Daylight colors at once and kept in the settings. The displays (time, waveforms) and what signals something (play, CUE, meters) look the same in both. No color files. |
| Multilingual | done | German, English, French, Spanish, Greek and Russian, chosen in Settings > Language and kept in the settings file. The player's controls keep the DJ's English words (Load, CUE, KEY, Pitch, Cue-Load and so on) in every language; their help bubbles are translated. |
| Always on top | planned | Desktop option from the original version. |

Layout reference: the original version, see [djw.gif](https://dj-workstation.de/bilder/djw.gif). The new UI keeps its structure, adapted for touch:
- Player window: load button, pitch bend -/+, vertical pitch fader, large time display with elapsed/remaining toggle, pitch in percent and pitch zero, position slider, state (loaded/cued/playing), large CUE and play/pause buttons, channel volume and headphone level faders with headphone cue button (an amber LED that lights while on), artist and title, Cue-Load option (a loaded track starts at the cue point stored for it, else at its first audible sound, else at the beginning; the last cue point the user set is stored per track in the library database).
- Crossfader window: deck selection for side A and B, zero, momentary A/B buttons, fade now, autoplay start before the end (s), fade time (s), autoplay and autofade switches.
- Playlist window: numbered list, next track highlighted.
- Library window: search fields directly above the columns (artist, title, genre, year), sortable list.

### 4.9 Recording and webradio

| Feature | Status | Description |
|---|---|---|
| Record mix | planned | Master to WAV, with track list (cue sheet). Compressing it (MP3, Ogg, FLAC) is left to other programs. |
| Webradio | planned | Send the master live to an Icecast or Shoutcast server, as the original version was used for. |

### 4.10 Remote

| Feature | Status | Description |
|---|---|---|
| Remote frontend | planned | Command/state API over a local socket or OSC, e.g. a tablet as a second control surface. LAN only, no cloud. Source code notice required by AGPL §13. |

---

## 5. Performance targets

| Metric | Target |
|---|---|
| Startup until usable | < 1 s (desktop), < 2 s (Pi 5) |
| Native audio latency | ≤ 6 ms at 256 samples / 48 kHz |
| Decks on Pi 4 | 4 decks with keylock without dropouts |
| UI | 60 fps on Pi 5, GPU-accelerated waveform rendering |
| Library search | < 50 ms with 100,000 tracks |
| Binary (desktop, static) | single-digit MB |
| RAM | No target. Decoded tracks up to the configured length are held in memory (about 23 MB per minute at 48 kHz); this is audio, not video. |

---

## 6. Technology

| Area | Library | License |
|---|---|---|
| Language | C++23 (without modules) | |
| Build | CMake with presets per platform | |
| Audio I/O, decoding MP3/FLAC/WAV | miniaudio | Public domain / MIT-0 |
| Ogg Vorbis | stb_vorbis (with miniaudio) | Public domain / MIT |
| Opus (planned) | libopusfile | BSD |
| Keylock/timestretch | SoundTouch, Signalsmith Stretch (Settings > Sound) | LGPL-2.1 / MIT |
| BPM detection | QM-DSP (onset detection, beat tracking; the tempo from a line through the beats, exact to a few hundredths) | GPL-2+ |
| Database | SQLite (FTS5) | Public domain |
| Tags | TagLib | LGPL-2.1 / MPL-1.1 |
| UI, windowing, touch | SDL3 | zlib |
| Font rendering | SDL3_ttf (FreeType, HarfBuzz) | zlib (FreeType: FTL, HarfBuzz: MIT) |
| Font | Noto Sans | OFL-1.1 |
| Logo (SVG) | nanosvg | zlib |
| MIDI | RtMidi (+ custom backends for Android/Web) | MIT-style |
| HID (planned) | hidapi | BSD |
| Controller mappings | toml++ | MIT |
| DVS (planned) | xwax timecoder | GPL-2+ |
| Tests | doctest | MIT |

---

## 7. Quality and CI

- Core fully testable headless.
- Offline render tests: a defined command sequence renders to WAV, compared against a reference.
- Audio thread checks: the tests count heap allocations in the audio path (decks, time stretchers, engine rendering); there must be none.
- GitHub Actions on every push: tests with ASan, UBSan and LSan and with TSan, clang-format and clang-tidy, line coverage; mutation tests with Mull once a month and on request.
- Release builds on a push to main once everything passed: Linux x86_64, Linux arm64, macOS and Windows. Android and Web are not built yet.
